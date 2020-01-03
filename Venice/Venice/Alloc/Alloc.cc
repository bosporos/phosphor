//
// project venice
// author Maximilien M. Cura
//

#include <Venice/Alloc/Alloc>

using namespace vnz::math;
using vnz::atomic::Ordering;
using namespace vnz::alloc;

ALWAYS_INLINE _u8 * align8 (void * ptr) { return static_cast<_u8 *> (ptr); }

template <class T>
static DLListItem<T> * furthest_left (DLListItem<T> * node)
{
    while (node->left != nullptr) {
        node = node->left;
    }
    return node;
}

template <class T>
static DLListItem<T> * furthest_right (DLListItem<T> * node)
{
    while (node->right != nullptr) {
        node = node->right;
    }
    return node;
}

template <class T>
static DLListItem<T> * find_left (T * x, DLListItem<T> * node)
{
    while (node->left != nullptr) {
        if (node->inner == x)
            return node;
        node = node->left;
    }
    return nullptr;
}

template <class T>
static DLListItem<T> * find_right (T * x, DLListItem<T> * node)
{
    while (node->right != nullptr) {
        if (node->inner == x)
            return node;
        node = node->right;
    }
    return nullptr;
}

Block::Block ()
    : range_begin { nullptr }
    , freeptr_local { nullptr }
    , freeptr_global { nullptr }
    , objects_free (0)
    , object_size { 0 }
    , object_capacity { 0 }
    , __padding0 { 0, 0 }
    , local_thread_id ()
    , global_lock ()
    , chain { nullptr }
{}

Block::~Block ()
{}

void Block::assign (void * mem)
{
    this->range_begin = mem;
}

void Block::assign_chain (void * chain)
{
    this->chain = chain;
}

void Block::format (_u16 const size)
{
    this->object_size     = size;
    this->object_capacity = __VNZA_PAGE_SIZE / size;
    this->objects_free.store (this->object_capacity, Ordering::Relaxed);

    this->freeptr_local = align8 (this->range_begin)
        + ((this->object_capacity - 1) * this->object_size);

    _u16 last = 0xffff;

    while (last > 0) {
        *static_cast<_u16 *> (this->freeptr_local) = last;
        last                                       = align8 (this->freeptr_local) - align8 (this->range_begin);
        this->freeptr_local                        = align8 (this->freeptr_local) - this->object_size;
    }

    // debug_assert_eq(this->freeptr_local - this->range_begin, 0);
}

void * Block::alloc_object ()
{
    if (this->freeptr_local != nullptr) {
        return this->alloc_object_local ();
    } else {
        this->global_lock.lock ();
        // freeptr_local is nullptr, and can only be changed from the local thread i.e. sequentially
        this->freeptr_local  = this->freeptr_global;
        this->freeptr_global = nullptr;
        this->global_lock.unlock ();
        if (this->freeptr_local != nullptr) {
            return this->alloc_object_local ();
        } else {
            return nullptr;
        }
    }
}

void * Block::alloc_object_local ()
{
    void * object                = this->freeptr_local;
    _u16 next_object_indirection = *static_cast<_u16 *> (object);
    if (next_object_indirection != 0xffff) {
        this->freeptr_local = next_object_indirection + align8 (this->range_begin);
    } else {
        this->freeptr_local = nullptr;
    }
    this->objects_free.fetch_sub (1, Ordering::Release);
    // if (this->chain != nullptr)
    // static_cast<BlockChain *> (this->chain)->metric_alloc (this);
    return object;
}

void Block::dealloc_object (void * object)
{
    if (this->local_thread_id.is_current ()) {
        if (this->freeptr_local == nullptr) {
            *static_cast<_u16 *> (object) = 0xffff;
        } else {
            *static_cast<_u16 *> (object) = align8 (this->freeptr_local) - align8 (this->range_begin);
        }
        this->freeptr_local = object;
        this->objects_free.fetch_add (1, Ordering::Release);
    } else {
        this->global_lock.lock ();
        if (this->freeptr_global == nullptr) {
            *static_cast<_u16 *> (object) = 0xffff;
        } else {
            *static_cast<_u16 *> (object) = align8 (this->freeptr_global) - align8 (this->range_begin);
        }
        this->freeptr_global = object;
        this->global_lock.unlock ();
        this->objects_free.fetch_add (1, Ordering::Release);
    }
    // static_cast<BlockChain *> (this->chain)->metric_dealloc (this);
    if (this->objects_free.load (Ordering::Relaxed) == this->object_capacity)
        static_cast<BlockChain *> (this->chain)->block_empty (this);
}

#include <sys/mman.h>

Chunk::Chunk ()
{
    void * mem = mmap (NULL, __VNZA_CHUNK_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANON | MAP_SHARED, 0, 0);
    if (mem == MAP_FAILED) {
        abort ();
    }
    this->range_begin = mem;
    _u8 * byte_ptr    = align8 (this->range_begin);
    for (_usize i = 0; i < 64; i++) {
        this->blocks[i].assign (static_cast<void *> (byte_ptr));
        byte_ptr += __VNZA_PAGE_SIZE;
    }
}

Chunk::~Chunk ()
{
    if (munmap (this->range_begin, __VNZA_CHUNK_SIZE) != 0) {
        abort ();
    }
}

ChunkChain::ChunkChain ()
    : head (nullptr)
    , chain_lock ()
{
    head.right = nullptr;
    head.left  = nullptr;
}

ChunkChain::~ChunkChain ()
{}

Chunk * ChunkChain::find_chunk (void * addr)
{
    this->chain_lock.read ();
    if (this->head.inner == nullptr) {
        return nullptr;
    }
    DLListItem<Chunk> * current = &this->head;
    while (current != nullptr) {
        if (align8 (addr) - align8 (current->inner->range_begin) < __VNZA_CHUNK_SIZE) {
            // It's OK to return at this point because the Chunk is a) disassociated from the chain and b) not going to suddenly disappear while there are outstanding references to it
            Chunk * chunk = current->inner;
            this->chain_lock.read_unlock ();
            return chunk;
        }
        current = current->right;
    }
    this->chain_lock.read_unlock ();
    return nullptr;
}

void ChunkChain::add_chunk (Chunk * chunk)
{
    this->chain_lock.write ();
    if (this->head.inner != nullptr) {
        DLListItem<Chunk> * link = static_cast<DLListItem<Chunk> *> (internal::alloc24 ());
        *link                    = this->head;
        link->left               = &this->head;
        this->head.right         = link;
    }
    this->head.inner = chunk;
    this->chain_lock.write_unlock ();
}

Chunk * ChunkChain::remove_chunk (Chunk * chunk)
{
    this->chain_lock.write ();
    if (this->head.inner == nullptr) {
        return nullptr;
    } else {
        DLListItem<Chunk> * current = &this->head;
        while (current != nullptr) {
            if (current->inner == chunk) {
                Chunk * chunk = current->inner;
                if (current != &this->head) {
                    // Left pointer is guaranteed if not HEAD
                    current->left->right = current->right;
                    // But the right pointer isn't
                    if (current->right != nullptr) {
                        current->right->left = current->left;
                    }
                    internal::free24 (current);
                } else {
                    Chunk * chunk = head.inner;
                    if (current->right == nullptr) {
                        // HEAD is the only block in the chain
                        current->inner = nullptr;
                    } else {
                        // there are multiple blocks in the chain
                        DLListItem<Chunk> * drop = head.right;
                        head.right               = head.right->right;
                        head.inner               = head.right->inner;
                        if (head.right != nullptr) {
                            head.right->left = &head;
                        }
                        internal::free24 (drop);
                    }
                }
                this->chain_lock.write_unlock ();
                return chunk;
            }
            current = current->right;
        }
    }
    this->chain_lock.write_unlock ();
    return nullptr;
}

#include <new>

ChunkTable::ChunkTable (math::_usize num_slots)
    : chains { nullptr }
    , hash_slots { num_slots }
{
    this->chains = static_cast<ChunkChain *> (internal::alloc_ct (this->hash_slots * sizeof (ChunkChain)));
    if (this->chains == nullptr) {
        abort ();
    }
    for (_usize i = 0; i < this->hash_slots; i++) {
        new (this->chains + i) ChunkChain ();
    }
}

ChunkTable::~ChunkTable ()
{
    for (_usize i = 0; i < this->hash_slots; i++) {
        this->chains[i].~ChunkChain ();
    }
    internal::free_ct (static_cast<void *> (this->chains), this->hash_slots * sizeof (ChunkChain));
}

void ChunkTable::add_chunk (Chunk * chunk)
{
    _usize range_low  = this->slot_hash (chunk->range_begin);
    _usize range_high = this->slot_hash (align8 (chunk->range_begin) + __VNZA_CHUNK_SIZE - 1);
    if (range_low != range_high) {
        this->chains[range_low].add_chunk (chunk);
        this->chains[range_high].add_chunk (chunk);
    }
}

Chunk * ChunkTable::remove_chunk (Chunk * chunk)
{
    _usize range_low  = this->slot_hash (chunk->range_begin);
    _usize range_high = this->slot_hash (align8 (chunk->range_begin) + __VNZA_CHUNK_SIZE - 1);
    if (range_low != range_high) {
        this->chains[range_low].remove_chunk (chunk);
        this->chains[range_high].remove_chunk (chunk);
    }
    return chunk;
}

BlockChain::BlockChain (Heap * heap)
    : head (nullptr, nullptr, nullptr)
    , parent { heap }
    , chain_lock ()
{}

BlockChain::~BlockChain ()
{
    this->chain_lock.lock ();
    if (this->head.inner != nullptr) {
        DLListItem<Block> * current = &this->head;
        while (current != nullptr) {
            this->parent->handle_free_block (current->inner);
            DLListItem<Block> * old = current;
            current                 = current->right;
            if (old != &this->head) {
                internal::free24 (old);
            }
        }
        current = this->head.left;
        while (current != nullptr) {
            this->parent->handle_free_block (current->inner);
            DLListItem<Block> * old = current;
            current                 = current->left;
        }
    }
    this->chain_lock.unlock ();
}

SizedBlockChain::SizedBlockChain (const _u16 os, Heap * heap)
    : BlockChain (heap)
    , object_size { os }
    , left_length (0)
    , right_length (0)
{}

SizedBlockChain::~SizedBlockChain ()
{}

void * SizedBlockChain::alloc_object ()
{
    void * object = this->head.inner->alloc_object ();
    if (LIKELY (object != nullptr)) {
        return object;
    } else {
        Block * block = this->needs_block ();
        if (block != nullptr)
            return block->alloc_object ();
        else
            return nullptr;
    }
}

// ew: k(y)d djs ksj
// me: ... blzjernju srzamokju
// ew: :skol:

Block * SizedBlockChain::needs_block ()
{
    Block * block = this->parent->request_block ();
    if (block != nullptr) {
        this->add_as_head (block);
    }
    return block;
}

void SizedBlockChain::block_empty (Block * block)
{
    this->chain_lock.lock ();
    if (block == this->head.inner) {
        this->chain_lock.unlock ();
        return;
    } else {
        // move or throw up the chain
        DLListItem<Block> * node = find_left<Block> (block, &this->head);
        this->left_length.fetch_sub (1, Ordering::AcqRel);

        // cauterize the left chain
        node->right->left = node->left;
        if (node->left != nullptr)
            node->left->right = node->right;

        if (this->right_length.load (Ordering::Acquire) <= this->metric->right_trim ()) {
            // add to the right chain
            node->right = this->head.right;
            node->left  = &this->head;
            if (node->right != nullptr)
                node->right->left = node;
            this->head.right = node;
            this->right_length.fetch_add (1, Ordering::AcqRel);
        } else {
            // throw to parent
            this->parent->handle_free_block (node->inner);
            internal::free24 (node);
        }
        this->chain_lock.unlock ();
    }
}

void SizedBlockChain::add_as_head (Block * block)
{
    this->chain_lock.lock ();
    if (this->head.inner != nullptr) {
        DLListItem<Block> * link = static_cast<DLListItem<Block> *> (internal::alloc24 ());
        *link                    = this->head;
        if (link->left != nullptr)
            link->left->right = link;
        link->right     = &this->head;
        this->head.left = link;
    }
    this->head.inner = block;
    if (this->left_length.fetch_add (1, Ordering::AcqRel) > this->metric->left_trim ()) {
        DLListItem<Block> * leftmost = furthest_left (&this->head);
        leftmost->right->left        = nullptr;
        this->parent->reclaim_filled_block (leftmost->inner);
        internal::free24 (leftmost);
        this->left_length.fetch_sub (1, Ordering::AcqRel);
    }
    this->chain_lock.unlock ();
}

UnsizedBlockChain::UnsizedBlockChain (Heap * heap)
    : BlockChain (heap)
    , tail_lock ()
    , length (0)
{}

UnsizedBlockChain::~UnsizedBlockChain ()
{}

void UnsizedBlockChain::push (Block * block)
{
    this->chain_lock.lock ();
    if (this->head.inner == nullptr) {
        this->head.inner = block;
        this->tail       = &this->head;
    } else {
        if (this->length.fetch_add (1, Ordering::AcqRel) <= this->metric->trim ()) {
            DLListItem<Block> * link = static_cast<DLListItem<Block> *> (internal::alloc24 ());
            link->right              = this->tail->right;   // nullptr
            this->tail->right        = link;
            link->left               = this->tail;
            link->inner              = block;
            this->tail               = link;
        } else {
            // If it's too long, promote
            this->parent->handle_free_block (this->pop ());
        }
    }
    this->chain_lock.unlock ();
}

Block * UnsizedBlockChain::pop ()
{
    this->tail_lock.lock ();
    Block * chop;
    if (this->tail == &this->head) {
        chop             = this->head.inner;
        this->head.inner = nullptr;
        this->tail       = nullptr;
        this->length.fetch_sub (1, Ordering::AcqRel);
        this->tail_lock.unlock ();
    } else {
        this->tail->left->right      = nullptr;
        DLListItem<Block> * cut_link = this->tail;
        this->tail                   = this->tail->left;
        this->length.fetch_sub (1, Ordering::AcqRel);
        this->tail_lock.unlock ();
        chop = cut_link->inner;
        internal::free24 (cut_link);
    }
}
