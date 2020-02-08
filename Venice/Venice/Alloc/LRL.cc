//
// project venice
// author Maximilien M. Cura
//

#include <Venice/Alloc/Alloc>

using namespace vnz::alloc;

LRL::LRL ()
    : lrl_head { nullptr }
    , lrl_length { 0 }
    , lrl_lock ()
{}

LRL::~LRL ()
{
#if __VNZA_DEBUG_DESTRUCTION
    printf ("LRL dying: %p-- blocks being sent to %p\n", this, this->rlh_heap);
    fflush (stdout);
#endif

    lrl_lock.lock ();
    __atomic_store_n (&lrl_length, 0, __ATOMIC_RELEASE);
    Block *block = lrl_head, *next_block;
    while (block != nullptr) {
        next_block = block->l_right;
        static_cast<LH *> (rlh_heap)->hook_linkage_informs_heap_of_evacuating_block (block);
        block = next_block;
    }
    lrl_lock.unlock ();

#if __VNZA_DEBUG_DESTRUCTION
    printf ("LRL dead: %p (%p)\n", this, this->rlh_heap);
    fflush (stdout);
#endif
}

InvocationResult LRL::hook_heap_informs_linkage_of_block_request (OSize os, Block ** block)
{
    lrl_lock.lock ();
    if (lrl_head != nullptr) {
        __atomic_sub_fetch (&lrl_length, 1, __ATOMIC_ACQ_REL);
        *block   = lrl_head;
        lrl_head = lrl_head->l_right;
        (*block)->hook_linkage_informs_block_of_format_request (os);
        lrl_lock.unlock ();
        (*block)->globalfree_lock.lock ();
        return IR_OK;
    } else {
        lrl_lock.unlock ();
        return IR_FAIL;
    }
}

InvocationResult LRL::hook_heap_informs_linkage_of_migrating_block (Block * block)
{
    lrl_lock.lock ();
#if __VNZA_DEBUG_BLOCK_MIGRATION
    printf ("LRL %p; migrating block %p (%p)\n", this, block, lrl_head);
#endif
    __atomic_add_fetch (&lrl_length, 1, __ATOMIC_ACQ_REL);
    block->globalfree_lock.unlock ();
    block->l_right = lrl_head;
    lrl_head       = block;
    lrl_lock.unlock ();
    return IR_OK;
}
