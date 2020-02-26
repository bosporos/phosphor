//
// project venice
// author Maximilien M. Cura
//

#include <Venice/Alloc/Alloc>

using namespace vnz::alloc;

RRLS::RRLS ()
    : rrls_head { nullptr }
    , rrls_length { 0 }
    , rrls_lock ()
{}

RRLS::~RRLS ()
{
#if __VNZA_DEBUG_DESTRUCTION
    printf ("RRLS dying: %p (%p)\n", this, this->rlh_heap);
    fflush (stdout);
#endif

    rrls_lock.lock ();
    __atomic_store_n (&rrls_length, 0, __ATOMIC_RELEASE);
    if (rrls_head != nullptr) {
        Block * block = rrls_head->l_right;
        while (block != nullptr) {
            static_cast<RH *> (this->rlh_heap)->hook_linkage_informs_heap_of_evacuating_block (block);
            block = block->l_right;
        }
        block = rrls_head->l_left;
        while (block != nullptr) {
            static_cast<RH *> (this->rlh_heap)->hook_linkage_informs_heap_of_evacuating_block (block);
            block = block->l_left;
        }
        static_cast<RH *> (this->rlh_heap)->hook_linkage_informs_heap_of_evacuating_block (rrls_head);
    }
    rrls_lock.unlock ();
}

InvocationResult RRLS::hook_heap_informs_linkage_of_block_request (OSize os, Block ** block)
{
#if __VNZA_DEBUG_TRACE_BLOCK_REQUEST
    printf ("RRLS block request %hu (%p) => [L%p , H%p]\n", os, this, __atomic_load_n (&rrls_length, __ATOMIC_ACQUIRE), rrls_head);
#endif
    if (__atomic_load_n (&rrls_length, __ATOMIC_ACQUIRE) > 0) {
#if __VNZA_DEBUG_TRACE_BLOCK_REQUEST
        printf ("-> [RRLS] length sufficient!\n");
#endif
        rrls_lock.lock ();
        if (__atomic_load_n (&rrls_length, __ATOMIC_ACQUIRE) > 0) {
#if __VNZA_DEBUG_TRACE_BLOCK_REQUEST
            printf ("-> [RRLS] length sufficient, post-lock!\n");
#endif
            __atomic_sub_fetch (&rrls_length, 1, __ATOMIC_ACQ_REL);
            *block = rrls_head;
            (*block)->globalfree_lock.lock ();
            bool _r_head = true;
            if (rrls_head->l_right != nullptr)
                rrls_head->l_right->l_left = rrls_head->l_left;
            else
                _r_head = false;
            if (rrls_head->l_left != nullptr)
                rrls_head->l_left->l_right = rrls_head->l_right;

            if (_r_head)
                rrls_head = rrls_head->l_right;
            else
                rrls_head = rrls_head->l_left;
            rrls_lock.unlock ();
            (*block)->hook_linkage_informs_block_of_reassignment ();
            return IR_OK;
        }
        rrls_lock.unlock ();
    }
    return IR_FAIL;
}

InvocationResult RRLS::hook_heap_informs_linkage_of_migrating_block (Block * block)
{
    rrls_lock.lock ();
    __atomic_store_n (&block->state_change_responder, static_cast<RL__EEState_EState *> (this), __ATOMIC_RELEASE);
    __atomic_add_fetch (&rrls_length, 1, __ATOMIC_ACQ_REL);
    if (rrls_head == nullptr) {
        rrls_head      = block;
        block->l_left  = nullptr;
        block->l_right = nullptr;
    } else {
        if (__atomic_load_n (&block->allocation_count, __ATOMIC_ACQUIRE) <= block->object_count / 4) {
            // empty-enough
            block->l_right = rrls_head->l_right;
            if (block->l_right != nullptr)
                block->l_right->l_left = block;
            block->l_left = rrls_head;
            rrls_head     = block;
        } else {
            // not empty-enough
            block->l_left = rrls_head->l_left;
            if (block->l_left->l_right != nullptr)
                block->l_left->l_right = block;
            block->l_right = rrls_head;
        }
    }
    block->globalfree_lock.unlock ();
    rrls_lock.unlock ();
    return IR_OK;
}

InvocationResult RRLS::hook_block_informs_linkage_of_empty_state (Block * invoker)
{
    rrls_lock.lock ();
    __atomic_sub_fetch (&rrls_length, 1, __ATOMIC_ACQ_REL);
    if (invoker->l_left != nullptr)
        invoker->l_left->l_right = invoker->l_right;
    if (invoker->l_right != nullptr)
        invoker->l_right->l_left = invoker->l_left;
    // will never be head
    rrls_lock.unlock ();
    return this->rlh_heap->hook_linkage_informs_heap_of_surplus_block (invoker);
}

InvocationResult RRLS::hook_block_informs_linkage_of_empty_enough_state (Block * invoker)
{
    rrls_lock.lock ();
    if (invoker->l_left != nullptr)
        invoker->l_left->l_right = invoker->l_right;
    invoker->l_right->l_left = invoker->l_left;
    // will never be head
    invoker->l_right = rrls_head->l_right;
    if (invoker->l_right != nullptr)
        invoker->l_right->l_left = invoker;
    invoker->l_left    = rrls_head;
    rrls_head->l_right = invoker;
    rrls_lock.unlock ();
    return IR_OK;
}
