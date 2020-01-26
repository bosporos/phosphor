//
// project venice
// author Maximilien M. Cura
//

#include <Venice/Alloc/Alloc>

using namespace vnz::alloc;
using namespace vnz::math;
using vnz::atomic::Ordering;

Block::Block (void * memory, void (*__shadow_free) (Block *))
    : range_begin { memory }
    , freeptr_local { nullptr }
    , freeptr_global { nullptr }
    , object_size { 0 }
    , object_count { 0 }
    , _allocation_count { 0 }
    , allocation_count (0)
    , local_thread_id ()
    , _shadow_free (__shadow_free)
    , active_linkage { nullptr }
{}

Block::~Block ()
{
    if (this->_shadow_free != nullptr) {
        this->_shadow_free (this);
    }
}

void Block::hook_informs_block_of_owner_change (ActiveBlockLinkage * new_linkage)
{
    this->local_thread_id.bind_to_current ();
    __atomic_store_n (&this->active_linkage, new_linkage, __ATOMIC_RELEASE);
}

void * Block::hook_linkage_informs_block_of_allocation_request ()
{
    if (__atomic_add_fetch (&this->_allocation_count, 1, __ATOMIC_ACQ_REL) <= this->object_count) {
        this->allocation_count.fetch_add (1, Ordering::AcqRel);
        if (this->freeptr_local != nullptr) {
            return this->hook_block_informs_block_of_local_list_allocation_request ();
        } else {
            void * _global      = __atomic_exchange_n (&this->freeptr_global, nullptr, __ATOMIC_ACQ_REL);
            this->freeptr_local = _global;
            if (this->freeptr_local != nullptr) {
                return this->hook_block_informs_block_of_local_list_allocation_request ();
            } else {
                return nullptr;
            }
        }
    } else {
        __atomic_sub_fetch (&this->_allocation_count, 1, __ATOMIC_ACQ_REL);
        return nullptr;
    }
}

void * Block::hook_block_informs_block_of_local_list_allocation_request ()
{
    void * object = this->freeptr_local;
    _u16 offset   = *detail::af (object);
    // if offset != 0xffff -> ~offset != 0
    if (~offset) {
        this->freeptr_local = detail::a8 (this->range_begin) + offset;
    } else {
        this->freeptr_global = nullptr;
    }
    return object;
}

void Block::hook_informs_block_of_deallocation_request (void * object_to_deallocate)
{
    if (this->local_thread_id.is_current ()) {
        if (this->freeptr_local != nullptr) {
            *detail::af (object_to_deallocate)
                = detail::a8 (this->freeptr_local)
                - detail::a8 (this->range_begin);
        } else {
            *detail::af (object_to_deallocate) = 0xffff;
        }
        this->freeptr_local = object_to_deallocate;
    } else {
        void * loadref = __atomic_load_n (&this->freeptr_global, __ATOMIC_ACQUIRE);
        do {
            if (loadref != nullptr) {
                *detail::af (object_to_deallocate)
                    = detail::a8 (loadref)
                    - detail::a8 (this->range_begin);
            } else {
                *detail::af (object_to_deallocate) = 0xffff;
            }
        } while (__atomic_compare_exchange_n (
            &this->freeptr_global,
            &loadref,
            object_to_deallocate,
            false,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE));
    }

    if (__atomic_load_n (&this->active_linkage->linkage_head, __ATOMIC_ACQUIRE)->inner != this) {
        __atomic_fetch_sub (&this->_allocation_count, 1, __ATOMIC_ACQ_REL);
        if (this->allocation_count.fetch_sub (1, Ordering::AcqRel) <= __atomic_load_n (&this->active_linkage, __ATOMIC_ACQUIRE)->metric->metric_block_sufficient_emptiness (this)) {
            this->hook_block_informs_block_of_sufficient_emptiness ();
        }

        math::_u16 __zero_ref16 = 0;
        if (__atomic_compare_exchange_n (
                &this->_allocation_count, &__zero_ref16, 0xf000, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
            this->hook_block_informs_block_of_emptiness ();
        }
    }
}

void Block::hook_block_informs_block_of_emptiness ()
{
    __atomic_load_n (&this->active_linkage, __ATOMIC_ACQUIRE)->hook_block_informs_linkage_of_empty_block (this);
}

void Block::hook_block_informs_block_of_sufficient_emptiness ()
{
    __atomic_load_n (&this->active_linkage, __ATOMIC_ACQUIRE)->hook_block_informs_linkage_of_sufficiently_empty_block (this);
}

void Block::hook_informs_block_of_memory_range_assignment (void * new_memory, void (*_new_shadow_free) (Block *))
{
    if (this->_shadow_free != nullptr) {
        this->_shadow_free (this);
    }
    this->range_begin  = new_memory;
    this->_shadow_free = _new_shadow_free;
}

void Block::hook_informs_block_of_format_request (_u16 const obj_sz)
{
    this->object_size  = obj_sz;
    this->object_count = 0x4000 / obj_sz;

    this->freeptr_global = nullptr;
    this->freeptr_local
        = detail::a8 (this->range_begin)
        + this->object_count * this->object_size;
    *detail::af (this->freeptr_local) = 0xffff;
    do {
        math::_u16 offset = detail::af (this->freeptr_local)
            - detail::af (this->range_begin);
        this->freeptr_local = detail::a8 (this->freeptr_local)
            - this->object_size;
        *detail::af (this->freeptr_local) = offset;
    } while (this->freeptr_local > this->range_begin);
    // this is owning thread, format implies empty, so we're clear to do direct
    // access
    this->allocation_count.inner = 0;
    this->_allocation_count      = 0;
}

ActiveBlockLinkage::ActiveBlockLinkage (Metric * _metric)
    : metric { _metric }
{}

ActiveLocalLinkage::ActiveLocalLinkage (_u16 const obj_sz,
                                        LocalHeap * _heap,
                                        Metric * _metric)
    : ActiveBlockLinkage (_metric)
    , linkage_head { nullptr }
    , linkage_lock ()
    , count_of_empty_blocks (0)
    , object_size { obj_sz }
    , heap { _heap }
{}

ActiveLocalLinkage::~ActiveLocalLinkage ()
{
    this->linkage_lock.lock ();
    detail::Item<Block> *current_item = this->linkage_head->left, *next_item;
    while (current_item != nullptr) {
        this->heap->hook_linkage_informs_heap_of_propagating_block (current_item->inner);
        next_item = current_item->left;
        detail::Item<Block>::free (current_item);
        current_item = next_item;
    }
    current_item = this->linkage_head->right;
    while (current_item != nullptr) {
        this->heap->hook_linkage_informs_heap_of_propagating_block (current_item->inner);
        next_item = current_item->right;
        detail::Item<Block>::free (current_item);
        current_item = next_item;
    }
    __atomic_store_n (&this->linkage_head->inner->_allocation_count, 0xf000, __ATOMIC_RELEASE);
    // force synchronization of happens-before to ensure lockedness
    _u16 _dummy_ = __atomic_load_n (&this->linkage_head->inner->_allocation_count, __ATOMIC_ACQUIRE);
    this->heap->hook_linkage_informs_heap_of_propagating_block (this->linkage_head->inner);
    detail::Item<Block>::free (this->linkage_head);
    this->linkage_head = nullptr;
    this->linkage_lock.unlock ();
}

void ActiveLocalLinkage::hook_block_informs_linkage_of_empty_block (Block * informer)
{
    // if this is the heap's unsized linkage, then there are no further blocks to
    // deallocate: therefore, this will not be called in such a situation
    // then, we know that the linkage is in fact a sized linkage
    // so, we de-link it and send it to the heap

    // however! if the block in question is our linkage_head, then we have a problem
    // because of the implementation of hook_linkage_informs_block_of_allocation_request
    // we know that the block is locked against allocation until we reset the
    // _allocation_count to allocation_count's inner
    // because of this, we know that hook_linkage_informs_block_of_allocation_request
    // will return a nullptr until _allocation_count is reset, and that
    // hook_heap_informs_linkage_of_allocation_request will try to bring in a new
    // block if it can't allocate an object

    detail::Item<Block>::free (this->linkage_head->find_right_left (informer));
    this->heap->hook_linkage_informs_heap_of_empty_block (informer);
}

void ActiveLocalLinkage::hook_block_informs_linkage_of_sufficiently_empty_block (Block * informer)
{
    // since the condition here is "sufficiently empty", we can draw the conclusion
    // that this linkage is a sized one
    // we know that the block in question is a) the head or b) to the left of the head
}

void ActiveLocalLinkage::hook_informs_linkage_of_block_addition (Block * new_block)
{
}
