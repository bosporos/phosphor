//
// project venice
// author Maximilien M. Cura
//

#include <Venice/Alloc/Alloc>

namespace math = vnz::math;
using namespace vnz::alloc;

extern "C"
{
    // technically _i64 -1 on error, so we _u64 > __VNZA_SL_NUMBER on error
    math::_u64 __vnza_chain_lookup (math::_u64 size);
}

LH::LH ()
// default constructors
{
    for (math::_i32 i = 0; i < __VNZA_SL_NUMBER; i++) {
        lal_table[i].rlh_heap = this;
    }
    lrl.rlh_heap = this;
}

LH::~LH ()
{
    dynamic_cast<RH__Parent *> (this->rhh_parent)->hook_heap_informs_heap_of_heap_death ();
}

InvocationResult LH::hook_linkage_informs_heap_of_surplus_block (Block * block)
{
    if (__atomic_load_n (&lrl.lrl_length, __ATOMIC_ACQUIRE) > __VNZA_LRL_CAP) {
        return dynamic_cast<RH *> (this->rhh_parent)->hook_heap_informs_heap_of_surplus_block (block);
    } else {
        return lrl.hook_heap_informs_linkage_of_migrating_block (block);
    }
}

InvocationResult LH::hook_linkage_informs_heap_of_evacuating_block (Block * block)
{
    // LH needs to pass directly up to RH; LH isn't going to last, but the RRH might
    return dynamic_cast<RH__HPropagator *> (this->rhh_parent)->hook_heap_informs_heap_of_evacuating_block (block);
}

InvocationResult LH::hook_linkage_informs_heap_of_block_request (OSize os, Block ** block)
{
    if (IR_OK == lrl.hook_heap_informs_linkage_of_block_request (os, block))
        return IR_OK;
    return dynamic_cast<RH__HBlockRequest *> (this->rhh_parent)->hook_heap_informs_heap_of_block_request (os, block);
}

InvocationResult LH::hook_client_informs_heap_of_allocation_request (OSize os, void ** object)
{
    math::_u64 _lal_luti = __vnza_chain_lookup (os);
    if (_lal_luti > __VNZA_SL_NUMBER)
        abort ();
    return lal_table[_lal_luti].hook_heap_informs_linkage_of_allocation_request (object, this);
}
