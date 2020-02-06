//
// project venice
// author Maximilien M. Cura
//

#include <Venice/Alloc/Alloc>

using namespace vnz::alloc;

RRLU::RRLU ()
    : rrlu_head { nullptr }
    , rrlu_length { 0 }
    , rrlu_lock ()
{}

RRLU::~RRLU ()
{
    rrlu_lock.lock ();
    __atomic_store_n (&rrlu_length, 0, __ATOMIC_RELEASE);
    Block * block = rrlu_head;
    while (block != nullptr) {
        static_cast<RH *> (this->rlh_heap)->hook_linkage_informs_heap_of_evacuating_block (block);
        block = block->l_right;
    }
    rrlu_lock.unlock ();
}

InvocationResult RRLU::hook_heap_informs_linkage_of_block_request (OSize os, Block ** block)
{
    rrlu_lock.lock ();
    if (rrlu_head != nullptr) {
        __atomic_sub_fetch (&rrlu_length, 1, __ATOMIC_ACQ_REL);
        *block    = rrlu_head;
        rrlu_head = rrlu_head->l_right;
        rrlu_lock.unlock ();
        (*block)->globalfree_lock.lock ();
        (*block)->hook_linkage_informs_block_of_format_request (os);
        (*block)->hook_linkage_informs_block_of_reassignment ();
        return IR_OK;
    } else {
        rrlu_lock.unlock ();
        return IR_FAIL;
    }
}

InvocationResult RRLU::hook_heap_informs_linkage_of_migrating_block (Block * block)
{
    rrlu_lock.lock ();
    __atomic_add_fetch (&rrlu_length, 1, __ATOMIC_ACQ_REL);
    block->globalfree_lock.unlock ();
    block->l_right = rrlu_head;
    rrlu_head      = block;
    rrlu_lock.unlock ();
    return IR_OK;
}
