//
// project venice
// author Maximilien M. Cura
//

#include <Venice/Alloc/Alloc>

using namespace vnz::alloc;

LAL::LAL ()
    : lal_osize { 0 }
    , lal_active { nullptr }
    , lal_lock ()
{
}

LAL::~LAL ()
{
    lal_lock.lock ();
    Block *cphase_active = __atomic_exchange_n (&lal_active, nullptr, __ATOMIC_ACQ_REL), *current;

    current = cphase_active->l_left;
    while (current != nullptr) {
        current->globalfree_lock.lock ();
        static_cast<LH *> (rlh_heap)->hook_linkage_informs_heap_of_evacuating_block (current);
        current = current->l_left;
    }
    current = cphase_active->l_right;
    while (current != nullptr) {
        current->globalfree_lock.lock ();
        static_cast<LH *> (rlh_heap)->hook_linkage_informs_heap_of_evacuating_block (current);
        current = current->l_right;
    }
    cphase_active->globalfree_lock.lock ();
    __atomic_store_n (&cphase_active->block_state, Inactive, __ATOMIC_RELEASE);
    static_cast<LH *> (rlh_heap)->hook_linkage_informs_heap_of_evacuating_block (cphase_active);

    lal_lock.unlock ();
}

InvocationResult LAL::hook_block_informs_linkage_of_empty_state (Block * invoker)
{
    // try lock then double-check active
    lal_lock.lock ();
    if (__atomic_load_n (&invoker->block_state, __ATOMIC_ACQUIRE) == Active) {
        // we became the active block; relinquish lock and drop
        lal_lock.unlock ();
        return IR_FAIL;
    }
    if (invoker->l_left != nullptr)
        invoker->l_left->l_right = invoker->l_right;
    if (invoker->l_right != nullptr)
        invoker->l_right->l_left = invoker->l_left;
    lal_lock.unlock ();
    return static_cast<LH *> (rlh_heap)->hook_linkage_informs_heap_of_surplus_block (invoker);
}

InvocationResult LAL::hook_block_informs_linkage_of_empty_enough_state (Block * invoker)
{
    // try lock then double-check active
    lal_lock.lock ();
    if (__atomic_load_n (&invoker->block_state, __ATOMIC_ACQUIRE) == Active) {
        // we became the active block; relinquish lock and drop
        lal_lock.unlock ();
        return IR_FAIL;
    }
    if (invoker->l_left != nullptr)
        invoker->l_left->l_right = invoker->l_right;
    if (invoker->l_right != nullptr)
        invoker->l_right->l_left = invoker->l_left;

    Block * cphase_active  = __atomic_load_n (&lal_active, __ATOMIC_ACQUIRE);
    invoker->l_left        = cphase_active;
    invoker->l_right       = cphase_active->l_right;
    cphase_active->l_right = invoker;
    // might be able to unlock here
    if (invoker->l_right != nullptr)
        invoker->l_right->l_left = invoker;
    lal_lock.unlock ();
    return IR_OK;
}

InvocationResult LAL::hook_heap_informs_linkage_of_allocation_request (void ** object, LH * heap)
{
    if (UNLIKELY (__atomic_load_n (&lal_active, __ATOMIC_ACQUIRE) == nullptr)) {
        lal_lock.lock ();
        Block * tmp_active;
        if (IR_OK == heap->hook_linkage_informs_heap_of_block_request (lal_osize, &tmp_active)) {
            tmp_active->state_change_responder = static_cast<RL__EEState_EState *> (this);
            tmp_active->globalfree_lock.unlock ();
            __atomic_store_n (&lal_active, tmp_active, __ATOMIC_RELEASE);
        } else {
            return IR_FAIL;
            lal_lock.unlock ();
        }
        lal_lock.unlock ();
    }
    // there will never be a write-race on head.
    // only a read-race
    if (IR_OK == __atomic_load_n (&lal_active, __ATOMIC_ACQUIRE)->hook_linkage_informs_block_of_allocation_request (object))
        return IR_OK;

    // lock for l_right
    lal_lock.lock ();
    Block * cphase_active = __atomic_load_n (&lal_active, __ATOMIC_ACQUIRE);
    if (cphase_active->l_right != nullptr) {
        __atomic_store_n (&cphase_active->l_right->block_state, Active, __ATOMIC_RELEASE);
        __atomic_store_n (&cphase_active->block_state, Inactive, __ATOMIC_RELEASE);

        __atomic_store_n (&lal_active, cphase_active->l_right, __ATOMIC_RELEASE);

        lal_lock.unlock ();
        // should always return IR_OK in this case
        return __atomic_load_n (&lal_active, __ATOMIC_ACQUIRE)
            ->hook_linkage_informs_block_of_allocation_request (object);
    }
    // aight, so no l_right
    if (IR_OK == heap->hook_linkage_informs_heap_of_block_request (lal_osize, &cphase_active->l_right)) {
        cphase_active->l_right->state_change_responder = static_cast<RL__EEState_EState *> (this);
        cphase_active->globalfree_lock.unlock ();
        __atomic_store_n (&cphase_active->l_right->block_state, Active, __ATOMIC_RELEASE);
        __atomic_store_n (&cphase_active->block_state, Inactive, __ATOMIC_RELEASE);

        __atomic_store_n (&lal_active, cphase_active->l_right, __ATOMIC_RELEASE);

        lal_lock.unlock ();
        return __atomic_load_n (&lal_active, __ATOMIC_ACQUIRE)
            ->hook_linkage_informs_block_of_allocation_request (object);
    }

    lal_lock.unlock ();

    // not sure if this is productive or not
    return __atomic_load_n (&lal_active, __ATOMIC_ACQUIRE)
        ->hook_linkage_informs_block_of_allocation_request (object);
}
