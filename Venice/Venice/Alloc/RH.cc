//
// project venice
// author Maximilien M. Cura
//

#include <Venice/Alloc/Alloc>

namespace math = vnz::math;
using namespace vnz::alloc;
using vnz::atomic::Ordering;

extern "C"
{
    // technically _i64 -1 on error, so we _u64 > __VNZA_SL_NUMBER on error
    math::_u64 __vnza_chain_lookup (math::_u64 size);
}

RH::RH ()
    // default constructors...
    : rh_active_references { 0 }
{
    for (math::_i32 i = 0; i < __VNZA_SL_NUMBER; i++) {
        rrls_table[i].rlh_heap = this;
    }
    rrlu.rlh_heap = this;
}

RH::~RH ()
{
    // well, the linkages'll do their thing
    dynamic_cast<RH__Parent *> (this->rhh_parent)->hook_heap_informs_heap_of_heap_death ();
}

InvocationResult RH::hook_linkage_informs_heap_of_surplus_block (Block * block)
{
    return this->__process_surplus_from_child (block);
}

InvocationResult RH::hook_linkage_informs_heap_of_evacuating_block (Block * block)
{
    // this->__evacuate_from_child (block);
    return dynamic_cast<RH__HPropagator *> (this->rhh_parent)->hook_heap_informs_heap_of_evacuating_block (block);
}

InvocationResult RH::hook_heap_informs_heap_of_surplus_block (Block * block)
{
    return this->__process_surplus_from_child (block);
}

InvocationResult RH::hook_heap_informs_heap_of_evacuating_block (Block * block)
{
    return this->__evacuate_from_child (block);
}

InvocationResult RH::__process_surplus_from_child (Block * block)
{
    // surplus = empty
    if (this->rhh_parent->rhh_parent == nullptr) {
        // GH tributary RH
        return rrlu.hook_heap_informs_linkage_of_migrating_block (block);
    } else {
        if (__atomic_load_n (&rrlu.rrlu_length, __ATOMIC_ACQUIRE) <= __VNZA_RRLU_CAP) {
            return rrlu.hook_heap_informs_linkage_of_migrating_block (block);
        } else {
            return dynamic_cast<RH__HPropagator *> (this->rhh_parent)
                ->hook_heap_informs_heap_of_surplus_block (block);
        }
    }
}

InvocationResult RH::__evacuate_from_child (Block * block)
{
    math::_u64 _rrl_luti = __vnza_chain_lookup (block->object_size);
    if (_rrl_luti > __VNZA_SL_NUMBER)
        abort ();
    if (this->rhh_parent->rhh_parent == nullptr) {
        // GH tributary RH
        if (__atomic_load_n (&block->allocation_count, __ATOMIC_ACQUIRE) == 0)
            return rrlu.hook_heap_informs_linkage_of_migrating_block (block);
        return rrls_table[_rrl_luti].hook_heap_informs_linkage_of_migrating_block (block);
    } else {
        if (__atomic_load_n (&block->allocation_count, __ATOMIC_ACQUIRE) == 0) {
            if (__atomic_load_n (&rrlu.rrlu_length, __ATOMIC_ACQUIRE) <= __VNZA_RRLU_CAP)
                return rrlu.hook_heap_informs_linkage_of_migrating_block (block);
        } else {
            if (__atomic_load_n (&rrls_table[_rrl_luti].rrls_length, __ATOMIC_ACQUIRE) <= __VNZA_RRLS_CAP)
                return rrls_table[_rrl_luti].hook_heap_informs_linkage_of_migrating_block (block);
        }
    }
    return dynamic_cast<RH__HPropagator *> (this->rhh_parent)
        ->hook_heap_informs_heap_of_evacuating_block (block);
}

InvocationResult RH::hook_heap_informs_heap_of_block_request (OSize os, Block ** block)
{
    math::_u64 _rrl_luti = __vnza_chain_lookup (os);
    if (_rrl_luti > __VNZA_SL_NUMBER)
        abort ();
    rrls_table[_rrl_luti].hook_heap_informs_linkage_of_block_request (os, block);
    if (*block != nullptr)
        return IR_OK;
    rrlu.hook_heap_informs_linkage_of_block_request (os, block);
    if (*block != nullptr)
        return IR_OK;
    return dynamic_cast<RH__HBlockRequest *> (this->rhh_parent)->hook_heap_informs_heap_of_block_request (os, block);
}

InvocationResult RH::hook_heap_informs_heap_of_heap_death ()
{
    if (__atomic_sub_fetch (&rh_active_references, 1, __ATOMIC_ACQ_REL) == 0) {
        // if there are no remaining references...
        if (this->rhh_parent->rhh_parent == nullptr) {
            // if it's the GHTRH, then we just ignore it, because if the GHTRH has no remaining
            // references, then one of two things will happen:
            //  a) the GH dies, and the blocks will no longer exist, much less be evacuable
            //  b) or the GH doesnt die, in which case, the GH will be GHTRH will be rebuilt
        } else {
            // Linkages are guaranteed to have evacuated at this point

            dynamic_cast<RH__Parent *> (this->rhh_parent)
                ->hook_heap_informs_heap_of_heap_death ();
        }
    }
    return IR_OK;
}
