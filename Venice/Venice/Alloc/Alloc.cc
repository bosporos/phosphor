//
// project venice
// author Maximilien M. Cura
//

#include <Venice/Alloc/Alloc>

using namespace vnz::alloc;
using vnz::atomic::Ordering;

/*
 * Block
 */

Block::Block (void (*__shadow) (Block *))
    : range_begin { nullptr }
    , freeptr_local { nullptr }
    , freeptr_global (nullptr)
    , object_size { 0 }
    , object_count { 0 }
    , __allocation_count { 0 }
    , allocation_count (0)
    /* Initialize the thread ID to the calling thread */
    , local_thread_id ()
    /* Set up the shadow destructor */
    , __shadow_free { __shadow }
{
}

Block::~Block ()
{
    if (this->__shadow_free) {
        this->__shadow_free (this);
    }
}

// it was a great deal of trouble to figure out how to make Block-level allocation/deallocation
// lockless, but I eventually found a solution
// this is based on several facets of the architecture:
//  1. only chain heads will receive request_allocation() calls
//  2. chain heads are 'stranded', that is, they may not be moved to higher heaps
// a Block will not become a chain head unless it is "empty enough"

void * Block::request_allocation ()
{
    if (__atomic_add_fetch (&this->__allocation_count, 1, __ATOMIC_ACQ_REL) <= this->object_count) {
        this->allocation_count.fetch_add (1, Ordering::AcqRel);
        if (this->freeptr_local != nullptr) {
            return this->request_allocation_from_local_list ();
        } else {
            void * global = __atomic_exchange_n (&this->freeptr_global, nullptr, __ATOMIC_ACQ_REL);
            // we know that freeptr_local is still a nullptr as request_allocation()
            // is only available to the owning thread, and only the owning thread
            // can deallocate objects to the local freelist
            this->freeptr_local = global;
            if (this->freeptr_local != nullptr) {
                return this->request_allocation_from_local_list ();
            } else {
                // global was blank too
                return nullptr;
            }
        }
    } else {
        // because the count checking mechanism unconditionally adds 1 to __allocation_count,
        // we have to disregard
        // because of this, __allocation_count is not completely accurate...
        // external observers are better off using allocation_count
        __atomic_sub_fetch (&this->__allocation_count, 1, __ATOMIC_ACQ_REL);
        return nullptr;
    }
}

void * Block::request_allocation_from_local_list ()
{
    void * object     = this->freeptr_local;
    math::_u16 offset = *internal::af (object);
    if (offset != 0xffff) {
        this->freeptr_local = internal::a8 (this->range_begin) + offset;
    } else {
        this->freeptr_local = nullptr;
    }
    return object;
}

void Block::request_deallocation (void * block_to_deallocate)
{
    if (this->local_thread_id.is_current ()) {
        if (this->freeptr_local != nullptr) {
            *internal::af (block_to_deallocate)
                = internal::a8 (this->freeptr_local)
                - internal::a8 (this->range_begin);
        } else {
            *internal::af (block_to_deallocate) = 0xffff;
        }
    } else {
        // this little routine here could potentially become quite contentious in a bad turn
        void * loadref = __atomic_load_n (&freeptr_global, __ATOMIC_ACQUIRE);
        do {
            if (loadref != nullptr) {
                *internal::af (block_to_deallocate)
                    /* __atomic_compare_exchange_n puts the new value of freeptr_global into loadref
                    if the compare fails, which means we only need the direct load once */
                    = internal::a8 (loadref)
                    - internal::a8 (this->range_begin);
            } else {
                // terminating value
                *internal::af (block_to_deallocate) = 0xffff;
            }
        } while (__atomic_compare_exchange_n (&this->freeptr_global, &loadref, block_to_deallocate, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE));
    }
    __atomic_fetch_sub (&this->__allocation_count, 1, __ATOMIC_ACQ_REL);
    if (this->allocation_count.fetch_sub (1, Ordering::AcqRel) <= __atomic_load_n (&this->parent, __ATOMIC_ACQUIRE)->metric->metric_block_sufficient_emptiness (this)) {
        this->hook_is_empty_enough ();
    }

    math::_u16 __zero_ref16 = 0;
    // we can rely on __allocation_count here because __allocation_count >= allocation_count
    // if __allocation_count is 0, then, allocation count must be 0 as well
    if (__atomic_compare_exchange_n (&this->__allocation_count, &__zero_ref16, 0xf000, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        this->hook_is_empty ();
    }
}

void Block::hook_is_empty_enough ()
{
    __atomic_load_n (&this->parent, __ATOMIC_ACQUIRE)->hook_block_is_empty (this);
}

void Block::hook_is_empty ()
{
    __atomic_load_n (&this->parent, __ATOMIC_ACQUIRE)->hook_block_is_empty_enough (this);
}

void Block::assign_memory_range (void * range_beginning)
{
    this->range_begin = range_beginning;
}

Block * Block::format (math::_u16 const obj_sz)
{
    this->object_size  = obj_sz;
    this->object_count = 0x4000 / obj_sz;

    this->freeptr_global = nullptr;
    this->freeptr_local
        = internal::a8 (this->range_begin)
        + this->object_count * this->object_size;
    *internal::af (this->freeptr_local) = 0xffff;
    do {
        math::_u16 offset = internal::af (this->freeptr_local)
            - internal::af (this->range_begin);
        this->freeptr_local = internal::a8 (this->freeptr_local)
            - this->object_size;
        *internal::af (this->freeptr_local) = offset;
    } while (this->freeptr_local > this->range_begin);
    // this is owning thread, format implies empty, so we're clear to do direct
    // access
    this->allocation_count.inner = 0;
    this->__allocation_count     = 0;

    return this;
}

#include <sys/mman.h> /* mmap */
#include <stdlib.h> /* abort */
#include <new> /* placement new */

/*
 * Chunk
 */

Chunk::Chunk ()
    : range_begin { nullptr }
    , block_availability { ~0ull }
    , chunk_lock ()
{
    void * memory_pages = mmap (
        /* no prespecified address */
        NULL,
        /* 1 MiB = 1024 KiB = 64 Blocks */
        0x100000,
        /* All permissions */
        PROT_READ | PROT_EXEC | PROT_WRITE,
        /* Anonymous map, unassociated with any external data */
        MAP_ANON | MAP_PRIVATE,
        /* Inapplicable values related to file mappings */
        0,
        0);
    if (memory_pages == MAP_FAILED) {
        // todo replace with panic
        abort ();
    }

    this->range_begin = memory_pages;
    for (int i = 0; i < 64; ++i) {
        new (&this->blocks[i]) Block ();
        this->blocks[i].assign_memory_range (internal::a8 (this->range_begin) + i * 0x4000);
    }
}

Chunk::~Chunk ()
{
    for (int i = 0; i < 64; ++i) {
        this->blocks[i].~Block ();
    }

    if (-1 == munmap (&this->range_begin, 0x100000)) {
        // a) not aligned to page (impossibe unless range_begin is externally edited)
        // b) length parameter negative or 0 (imposible, it's a constant)
        // c) some part of the mapped region is in non-valid address space
        // theoretically impossible, but todo replace with panic anyways
        abort ();
    }
}

Block * Chunk::request_block_if_available ()
{
    Block * block = nullptr;
    // need chunk_lock to guard block_availability
    this->chunk_lock.lock ();
    if (this->block_availability) {
        // number of leading zero bits...
        int offset = __builtin_clzl (this->block_availability);
        this->block_availability &= ~(0x8000000000000000 >> offset);
        block = &this->blocks[offset];
    }
    this->chunk_lock.unlock ();
    return block;
}

Block * Chunk::find_block_for_object (void * object)
{
    math::_u64 offset = internal::a8 (object) - internal::a8 (this->range_begin);
    // assert (offset < 0x100000);
    offset /= 0x4000;
    return &this->blocks[offset];
}

/*
 * ChunkLinkage
 */

ChunkLinkage::ChunkLinkage ()
    : chunks ()
    , linkage_lock ()
{}

ChunkLinkage::~ChunkLinkage ()
{}

Chunk * ChunkLinkage::find_chunk_for_object (void * object)
{
    this->linkage_lock.lock ();
    internal::DLListItem<Chunk> * link = chunks.head;
    while (link != nullptr) {
        if (0x100000 > (internal::a8 (object) - internal::a8 (link->inner->range_begin))) {
            this->linkage_lock.unlock ();
            return link->inner;
        }
        link = link->right;
    }
    this->linkage_lock.unlock ();
    return nullptr;
}

void ChunkLinkage::add_chunk (Chunk * chunk)
{
    this->linkage_lock.lock ();
    this->chunks.add_as_head (chunk);
    this->linkage_lock.unlock ();
}

Chunk * ChunkLinkage::remove_chunk (Chunk * chunk)
{
    this->linkage_lock.lock ();
    internal::DLListItem<Chunk> * link = this->chunks.head;
    while (link != nullptr && link->inner != chunk) {
    }
    if (link == nullptr) {
        this->linkage_lock.unlock ();
        return nullptr;
    }
    this->chunks.remove (link);
    this->linkage_lock.unlock ();
    return chunk;
}

/*
 * ChunkTable
 */

ChunkTable::ChunkTable (const math::_usize slots)
    : hash_slots (slots)
{
    this->linkages = internal::allocate_dalt_linkages<ChunkLinkage> (slots);
    for (math::_usize i = 0; i < slots; i++) {
        new (&this->linkages[i]) ChunkLinkage ();
    }
}

ChunkTable::~ChunkTable ()
{
    for (math::_usize i = 0; i < this->hash_slots; i++) {
        this->linkages[i].~ChunkLinkage ();
    }
}

Block * ChunkTable::find_block_for_object (void * object)
{
    return this->find_chunk_for_object (object)->find_block_for_object (object);
}

void ChunkTable::add_chunk (Chunk * chunk)
{
    this->linkages[this->slot_hash (chunk->range_begin)].add_chunk (chunk);
}

Chunk * ChunkTable::remove_chunk (Chunk * chunk)
{
    return this->linkages[this->slot_hash (chunk->range_begin)].remove_chunk (chunk);
}

/*
 * DeallocationBlockLinkage
 */

DeallocationBlockLinkage::DeallocationBlockLinkage ()
    : blocks ()
    , linkage_lock ()
{}

DeallocationBlockLinkage::~DeallocationBlockLinkage ()
{}

Block * DeallocationBlockLinkage::find_block_for_object (void * object)
{
    this->linkage_lock.lock ();
    internal::DLListItem<Block> * link = this->blocks.head;
    while (link != nullptr) {
        if (0x100000 > (internal::a8 (object) - internal::a8 (link->inner->range_begin))) {
            this->linkage_lock.unlock ();
            return link->inner;
        }
        link = link->right;
    }
    this->linkage_lock.unlock ();
    return nullptr;
}

void DeallocationBlockLinkage::add_block (Block * chunk)
{
    this->linkage_lock.lock ();
    this->blocks.add_as_head (chunk);
    this->linkage_lock.unlock ();
}

Block * DeallocationBlockLinkage::remove_block (Block * chunk)
{
    this->linkage_lock.lock ();
    internal::DLListItem<Block> * link = blocks.head;
    while (link != nullptr && link->inner != chunk) {
    }
    if (link == nullptr) {
        this->linkage_lock.unlock ();
        return nullptr;
    }
    this->blocks.remove (link);
    this->linkage_lock.unlock ();
    return chunk;
}

/*
 * BlockTable
 */

BlockTable::BlockTable (const math::_usize slots)
    : hash_slots (slots)
{
    this->linkages = internal::allocate_dalt_linkages<DeallocationBlockLinkage> (slots);
    for (math::_usize i = 0; i < slots; i++) {
        new (&this->linkages[i]) DeallocationBlockLinkage ();
    }
}

BlockTable::~BlockTable ()
{
    for (math::_usize i = 0; i < this->hash_slots; i++) {
        this->linkages[i].~DeallocationBlockLinkage ();
    }
}

Block * BlockTable::find_block_for_object (void * object)
{
    return this->linkages[this->slot_hash (object)].find_block_for_object (object);
}

/*
 * BlockAllocation Linkage
 */

BlockAllocationLinkage::BlockAllocationLinkage (Metric * metric, math::_u16 const obj_sz)
    : metric { metric }
    , bal_lock ()
    , parent { nullptr }
    , head (nullptr)
    , empties (0)
    , object_size { obj_sz }
{
}

BlockAllocationLinkage::~BlockAllocationLinkage ()
{
    // metric is a pointer, and bal_lock::~Mutex can handle itself
    // just gotta take care of the blocks

    this->bal_lock.lock ();
    internal::DLListItem<Block> *li = head->left, *new_li;
    while (li != nullptr) {
        __atomic_load_n (&this->parent, __ATOMIC_ACQUIRE)->parent_sized->hook_child_recycling_block (li->inner);
        new_li = li->left;
        internal::free_dllistitem (li);
        li = new_li;
    }
    li = head->right;
    while (li != nullptr) {
        if (li->inner->allocation_count.load (Ordering::Acquire) == 0) {
            __atomic_load_n (&this->parent, __ATOMIC_ACQUIRE)->parent_unsized->hook_child_recycling_block (li->inner);
        } else {
            __atomic_load_n (&this->parent, __ATOMIC_ACQUIRE)->parent_sized->hook_child_recycling_block (li->inner);
        }
        new_Li = li->right;
        internal::free_dllistitem (li);
        li = new_li;
    }
    // special lock
    // higher than the normal lock
    __atomic_store_n (&this->head->inner->__allocation_count, 0xf800, __ATOMIC_RELEASE);
    if (this->head->inner->allocation_count.load (Ordering::Acquire) == 0) {
        __atomic_load_n (&this->parent, __ATOMIC_ACQUIRE)->parent_sized = this->head->inner;
        internal::free_dllistitem (this->head);
        this->head = nullptr;
    }
    this->bal_lock.unlock ();
}

void BlockAllocationLinkage::hook_block_is_empty (Block * block)
{
    typedef internal::DLListItem<Block> LI;
    // because of the implementation of ``Block::request_allocation'' and
    // ``Block::request_deallocation'', we know that ``block'' is empty and locked
    // until __allocation_count is moved back within object_count, so we can feel
    // free to do as we wish with it.
    this->bal_lock.lock ();
    // inner never changes, so a simple atomic read of the pointer is sufficient
    if (block == this->head->inner) {
        this->bal_lock.unlock ();
        return;
    }
    if (this->empties.load (Ordering::Acquire) > this->metric->metric_block_allocation_linkage_max_empties (this)) {
        LI * li = this->head->find_left_right (block);
        li->excise ();
        internal::free_dllistitem (li);
        this->bal_lock.unlock ();
        __atomic_load_n (&this->parent, __ATOMIC_ACQUIRE)->hook_block_is_empty (block);
    } else {
        LI * li = this->head->find_left_right (block);
        li->excise ();
        this->head->add_right (li);
        this->bal_lock.unlock ();
    }
}

void BlockAllocationLinkage::hook_block_is_empty_enough (Block * block)
{
    typedef internal::DLListItem<Block> LI;
    // we have no idea what's going on inside ``block''.
    // it's not guarenteed to be empty, and it's not locked
    // neither is it even guaranteed to be empty enough
    this->bal_lock.lock ();
    if (block == this->head->inner) {
        this->bal_lock.unlock ();
        return;
    }
    LI * li = this->head->find_left_right (block);
    li->excise ();
    this->head->add_right (li);

    this->bal_lock.unlock ();
}

#define __VNZA_BLOCK_ALLOCATION_SHORTHAND              \
    object = this->head->inner->request_allocation (); \
    if (LIKELY (object != nullptr))                    \
        return object;
#define __VNZA_BLOCK_ALLOCATION_SHORTHAND_L            \
    object = this->head->inner->request_allocation (); \
    if (LIKELY (object != nullptr)) {                  \
        this->bal_lock.unlock ();                      \
        return object;                                 \
    }

void * BlockAllocationLinkage::allocate_from_linkage ()
{
    void * object;
    __VNZA_BLOCK_ALLOCATION_SHORTHAND
    this->bal_lock.lock ();
    if (this->head->right != nullptr) {
        this->head = this->head->right;
    }
    __VNZA_BLOCK_ALLOCATION_SHORTHAND_L
    Block * block = __atomic_load_n (&this->parent, __ATOMIC_ACQUIRE)->hook_child_requesting_block (this->object_size);
    __atomic_store_n (&block->parent, this, __ATOMIC_RELEASE);
    this->head->add_right (internal::alloc_dllistitem (block));

    this->head = this->head->right;
    __VNZA_BLOCK_ALLOCATION_SHORTHAND_L

    this->bal_lock.unlock ();
    return nullptr;
}

/*
 * Metric
 */

vnz::math::_u16 Metric::metric_block_sufficient_emptiness (Block * block)
{
    return block->object_count / 4;
}

/*
 * LocalAllocatingHeap
 */

extern "C"
{
    // floor(log2(a - 2^floor(log2(a)) + 2 * log2(a))
    vnz::math::_i64 __vnz_chain_lookup (vnz::math::_u64 obj_size);
}

LocalAllocatingHeap::LocalAllocatingHeap (Metric * metric)
    : parent_unsized { nullptr }
    , parent_sized { nullptr }
    , metric { metric }
    , lah_lock ()
{
    for (i32 i = 0; i < 20; i++) {
        // 2 ^ floor(i / 2) + (2 ^ floor(i / 2)) / (2 - i mod 2)
        new (&this->linkages[i]) BlockAllocationLinkage (
            this->metric,
            1 << (i >> 1) /* 2 ^ floor(i / 2) */
                | ((1 << (i >> 1)) /* 2 ^ floor(i / 2) */
                   / (2 - (i & 1)))); /* / (2 - i mod 2) */
        __atomic_store_n (&this->linkages[i].parent, this, __ATOMIC_RELEASE);
        Block * block = this->hook_child_requesting_block (this->linkages[i].object_size);
        __atomic_store_n (&block->parent, &this->linkages[i], __ATOMIC_RELEASE);
        this->linkages[i].head = internal::alloc_dllistitem (block);
    }
}

LocalAllocatingHeap::~LocalAllocatingHeap ()
{
    for (i32 i = 0; i < 20; i++) {
        this->linkages[i].~BlockAllocationLinkage ();
    }
}

void * LocalAllocatingHeap (math::_u16 const obj_size)
{
    math::_u64 linkage_index = static_cast<math::_u64> (
        __vnz_chain_lookup (
            static_cast<math::_u64> (obj_size)));
    // linkage_index == -1 -> error
    // linkage_index >= 20 -> no deal, fall back to other allocators
    // return nullptr for both
    // use the negative uint trick for this:
    if (UNLIKELY (20 <= linkage_index)) {
        return nullptr;
    }

    return this->linkages[linkage_index].allocate_from_linkage ();
}

void LocalAllocatingHeap::hook_block_is_empty (Block * block)
{
    // called by BlockAllocationLinkage's hook_block_is_empty()
    // At this point, ``block'' is the sole remaining reference to the Block

    // basically, it was evicted from the linkage because the linkage had too many
    // empties, so we can pass it back up to ``parent_unsized'' to be stored, or
    // whatever it wants to do
    this->parent_unsized->hook_block_is_empty (block);
}

Block * LocalAllocatingHeap::hook_child_requesting_block (const math::_u16 size)
{
    // called by BlockAllocationLinkage's allocate_from_linkage()
    // Block is not reformatted on arrival, so that we can still use pre-formatted Blocks
    // additionally, we must re-point the owning thread id
    // also have to take care of the parent pointers

    Block * block = this->parent_sized->hook_child_requesting_block (size);
    if (block == nullptr) {
        // unsized heaps don't care about size, so we use a constant for a marginal
        // speed-up (don't have to deal with the ``size'' variable)
        block = this->parent_unsized->hook_child_requesting_block (0);
    }
    if (LIKELY (block != nullptr)) {
        block->local_thread_id.bind_to_current ();
        if (block->object_size != size)
            block > format (size);
        return block;
    } else {
        // out-of-memory, pretty much
        // should probably abort or something
        // return nullptr;
        abort ();
    }
}

void LocalAllocatingHeap::hook_child_recycling_block (Block *)
{
    // BlockAllocationLinkage goes straight to parent_sized and parent_unsized
    // for hook_child_recycling_block
    // so, basically, nothing to do here
    // this is an impossible!/unreachable!
    abort ();
}

/*
 * UnsizedReclamationLinkage
 */

UnsizedReclamationLinkage::UnsizedReclamationLinkage (Metric * metric)
    : metric { metric }
    , parent { nullptr }
    , head { nullptr }
    ,
{
}
