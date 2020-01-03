//
// project venice
// author Maximilien M. Cura
//

#include <Venice/Alloc/Alloc>

using vnz::alloc::Superblock;
using namespace vnz::math;
using vnz::atomic::Ordering;

Superblock::Superblock ()
    : range_begin { nullptr }
    , freeptr_local { nullptr }
    , freeptr_global { nullptr }
    , free_blocks (0)
    , block_size { 0 }
    , block_capacity { 0 }
    , thread_id ()
    , freeptr_global_mutex ()
    , chain ()
{}

Superblock::~Superblock ()
{}

void Superblock::set (void * memory)
{
    this->range_begin    = memory;
    this->freeptr_local  = memory;
    this->freeptr_global = nullptr;
}

void Superblock::format (math::_u64 block_size)
{
    this->block_size     = block_size;
    this->block_capacity = 0x4000 / block_size;
    this->free_blocks.store (this->block_capacity);

    this->freeptr_local = static_cast<_u8 *> (this->range_begin) + ((this->block_capacity - 1) * this->block_size);

    _u16 last = 0xFFFF;

    for (usize i = 0uz; i < this->block_capacity; i++) {
        *static_cast<_u16 *> (this->freeptr_local) = last;
        last                                       = static_cast<_u8 *> (this->freeptr_local) - static_cast<_u8 *> (this->range_begin);
        this->freeptr_local                        = static_cast<_u8 *> (this->freeptr_local) - this->block_size;
    }

    // debug_eq (this->freeptr_local - this->range_begin, 0);
}

void * Superblock::allocate ()
{
    if (this->freeptr_local != nullptr) {
        void * block = this->freeptr_local;
        _u16 delta   = *static_cast<_u16 *> (block);
        if (delta != 0xffff) {
            this->freeptr_local = delta + static_cast<_u8 *> (this->range_begin);
        } else {
            this->freeptr_local = nullptr;
        }
        this->free_blocks.fetch_sub (1, Ordering::Release);
        return block;
    } else {
        this->freeptr_global_mutex.lock ();
        if (this->freeptr_global != nullptr) {
            void * block = this->freeptr_global;
            _u16 delta   = *static_cast<_u16 *> (block);
            if (delta != 0xffff) {
                this->freeptr_global = delta + static_cast<_u8 *> (this->range_begin);
            } else {
                this->freeptr_global = nullptr;
            }
            this->freeptr_global_mutex.unlock ();
            this->free_blocks.fetch_sub (1, Ordering::Release);
            return block;
        } else {
            this->freeptr_global_mutex.unlock ();
            return nullptr;
        }
    }
}

void Superblock::deallocate (void * block)
{
    if (this->thread_id.is_current ()) {
        if (this->freeptr_local == nullptr) {
            *static_cast<_u16 *> (block) = 0xffff;
        } else {
            *static_cast<_u16 *> (block) = static_cast<_u8 *> (this->freeptr_local) - static_cast<_u8 *> (this->range_begin);
        }
        this->freeptr_local = block;
        this->free_blocks.fetch_add (1, Ordering::Acquire);
    } else {
        this->freeptr_global_mutex.lock ();
        if (this->freeptr_global == nullptr) {
            *static_cast<_u16 *> (block) = 0xffff;
        } else {
            *static_cast<_u16 *> (block) = static_cast<_u8 *> (this->freeptr_global) - static_cast<_u8 *> (this->range_begin);
        }
        this->freeptr_global = block;
        this->freeptr_global_mutex.unlock ();
        this->free_blocks.fetch_add (1, Ordering::Acquire);
    }
}

#include <sys/mman.h>
#include <errno.h>

using vnz::alloc::Chunk;

Chunk::Chunk ()
    : left { nullptr }
    , right { nullptr }
{
    void * mem = mmap (NULL, 0x100000, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANON | MAP_SHARED, 0, 0);
    if (mem == MAP_FAILED) {
        abort ();
    }
    this->range_begin    = mem;
    math::_u8 * byte_ptr = static_cast<_u8 *> (this->range_begin);
    for (usize i = 0uz; i < 64; i++) {
        this->superblocks[i].set (byte_ptr);
        byte_ptr += 0x4000;
    }
}

Chunk::~Chunk ()
{
    if (munmap (this->range_begin, 0x100000) != 0) {
        abort ();
    }
}

using vnz::alloc::ChunkChain;

ChunkChain::ChunkChain ()
    : head { nullptr }
    , length (0)
    , lock ()
{}

ChunkChain::~ChunkChain ()
{}

Chunk * ChunkChain::find_chunk (void * addr)
{
    this->lock.read ();
    Chunk * current = this->head;
    // head->left is always nullptr
    // the last element in the chain will have ()->right == nullptr
    while (current != nullptr) {
        if ((static_cast<math::_u8 *> (addr)
             - static_cast<math::_u8 *> (current->range_begin))
            < 0x100000) {
            this->lock.read_unlock ();
            return current;
        }
        current = current->right;
    }
    this->lock.read_unlock ();
    return nullptr;
}

void ChunkChain::add_chunk (Chunk * chunk)
{
    this->lock.write ();
    chunk->right = this->head;
    chunk->left  = nullptr;
    this->head   = chunk;
    if (chunk->right != nullptr) {
        chunk->right->left = chunk;
    }
    this->lock.write_unlock ();
    this->length.fetch_add (1, Ordering::Relaxed);
}

void ChunkChain::remove_chunk (Chunk * chunk)
{
    this->lock.write ();
    if (chunk->left == nullptr) {
        // i.e. chunk is head
        this->head = chunk->right;
        if (chunk->right != nullptr) {
            chunk->right->left = chunk->left;
        }
    } else if (chunk->right == nullptr) {
        // i.e. chunk is last
        if (chunk->left != nullptr) {
            chunk->left->right = chunk->right;
        } else {
            // impossible();
            __builtin_unreachable ();
            // chunk is head & last
            this->head = nullptr;
        }
    } else {
        chunk->right->left = chunk->left;
        chunk->left->right = chunk->right;
    }
    this->lock.write_unlock ();
    this->length.fetch_sub (1, Ordering::Relaxed);
}

#include <new>
using vnz::alloc::ChunkTable;

ChunkTable::ChunkTable (math::_usize num_entries)
    : chains { nullptr }
    , entries { num_entries }
    , chunks (0)
{
    // debug_assert_neq(num_entries, 0)
    this->chains = static_cast<ChunkChain *> (calloc (num_entries, sizeof (ChunkChain)));
    if (this->chains == NULL) {
        abort ();
    }
    for (usize i = 0uz; i < num_entries; i++) {
        new (this->chains + i) ChunkChain ();
    }
}

ChunkTable::ChunkTable (math::_usize num_entries, void * memory, void (*dealloc_proc) (void *))
    : chains { static_cast<ChunkChain *> (memory) }
    , entries { num_entries }
    , chunks (0)
    , deallocation_procedure { dealloc_proc }
{
    // debug_assert_neq(num_entries, 0)
    // debug_assert(memory != NULL && memory != nullptr)
    for (usize i = 0uz; i < num_entries; i++) {
        new (this->chains + i) ChunkChain ();
    }
}

ChunkTable::~ChunkTable ()
{
    this->deallocation_procedure (this->chains);
}

void ChunkTable::add_chunk (Chunk * chunk)
{
    this->chains[this->index_hash (chunk->range_begin)].add_chunk (chunk);
    this->chunks.fetch_add (1, Ordering::AcqRel);
}

void ChunkTable::remove_chunk (Chunk * chunk)
{
    this->chains[this->index_hash (chunk->range_begin)].remove_chunk (chunk);
    this->chunks.fetch_sub (1, Ordering::AcqRel);
}

using vnz::alloc::SuperblockChainNode;

SuperblockChainNode::SuperblockChainNode (Superblock * x)
    : inner (x)
    , left (nullptr)
    , right (nullptr)
{}

SuperblockChainNode::~SuperblockChainNode ()
{}

using vnz::alloc::LocalChain;

LocalChain::LocalChain (math::_u16 block_size)
    : head (nullptr)
    , num_nodes (0)
    , num_superblocks (0)
    , left_length (0)
    , right_length (0)
    , block_size { block_size }
    , __shadow { 0 }
    , chain_lock ()
// empty {} will zero it, so we leave it as undefined
// , __padding[0] {}
{}

LocalChain::~LocalChain ()
{
    // Send to the appropriate reclamation heap
}

using vnz::sys::sync::Mutex;

struct VNZ_VEILED __ShimLock
{
    Mutex & inner;

    __ShimLock (Mutex & x)
        : inner { x }
    {
        inner.lock ();
    }

    ~__ShimLock ()
    {
        inner.unlock ();
    }
};

Superblock * LocalChain::add_as_head (Superblock * sb)
{
    // Only one possible conflict here: reorder()
    // request_head_change() and add_as_head() are LT only
    // reserve() and trim() both lock before performing ops

    __ShimLock __shimlock (chain_lock);
    // Atomicity is a non-problem here, so we use Atomic<T>.inner for direct pointer access
    if (head->inner.inner != nullptr) {
        SuperblockChainNode * old_head = head;
        head                           = static_cast<SuperblockChainNode *> (malloc (sizeof (SuperblockChainNode)));
        if (old_head->inner.inner->free_blocks.load (Ordering::Acquire)) {
            head->right.inner = old_head;
            head->left.inner  = old_head->left.inner;
            if (head->left.inner != nullptr) {
                head->left.inner->right.inner = head;
            }
            old_head->left.inner = head;
        } else {
            head->left.inner  = old_head;
            head->right.inner = old_head->right.inner;
            if (head->right.inner != nullptr) {
                head->right.inner->left.inner = head;
            }
            old_head->right.inner = head;
        }
    } else {
        this->head = static_cast<SuperblockChainNode *> (malloc (sizeof (SuperblockChainNode)));
        new (this->head) SuperblockChainNode (sb);
    }
    this->num_nodes.fetch_add (1, Ordering::AcqRel);
    this->num_superblocks.fetch_add (1, Ordering::AcqRel);
    // left_ and right_length are both still 0
    return this->head->inner.inner;
}

Superblock * LocalChain::reserve (Superblock * sb)
{
    __ShimLock __shimlock (chain_lock);
    SuperblockChainNode * tmp_ptr = static_cast<SuperblockChainNode *> (malloc (sizeof (SuperblockChainNode)));
    new (tmp_ptr) SuperblockChainNode (sb);
    if (this->head->inner.inner != nullptr) {
        SuperblockChainNode * old_right = this->head->right.inner;
        tmp_ptr->left.inner             = this->head;
        tmp_ptr->right.inner            = old_right;
        if (old_right != nullptr) {
            old_right->left.inner = tmp_ptr;
        }
        this->head->right.inner = tmp_ptr;
        this->right_length.fetch_add (1, Ordering::AcqRel);
    } else {
        this->head = tmp_ptr;
    }
    this->num_nodes.fetch_add (1, Ordering::AcqRel);
    this->num_superblocks.fetch_add (1, Ordering::AcqRel);
    return this->head->inner.inner;
}

Superblock * LocalChain::request_head_change (Heap * parent)
{
    __ShimLock __shimlock (chain_lock);
    if (this->head != nullptr && this->head->right.inner != nullptr) {
        this->head = this->head->right.inner;
        return this->head->inner.inner;
    } else {
        Superblock * commission = parent->request_sized (usize (this->block_size));
        if (commission == nullptr) {
            return nullptr;
        } else {
            return this->add_as_head (commission);
        }
    }
}

void LocalChain::reorder (Superblock * sb)
{
    // TODO
}
