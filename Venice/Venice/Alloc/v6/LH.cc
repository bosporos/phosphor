//
// project venice
// author Maximilien M. Cura
//

#include <Venice/Alloc/Alloc>

#include <new>

namespace math = vnz::math;
using namespace vnz::alloc;

extern "C"
{
    // technically _i64 -1 on error, so we _u64 > __VNZA_SL_NUMBER on error
    math::_u64 __vnza_chain_lookup (math::_u64 size);
}

LH::LH ()
{
    // 0x18 = 16 + 8 = 24
    const OSize sizes[] = { 2, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048, 3072, 4096 };
    for (math::_i32 i = 0; i < __VNZA_SL_NUMBER; i++) {
        new (&lal_table[i]) LAL;
        lal_table[i].rlh_heap  = this;
        lal_table[i].lal_osize = sizes[i];
    }
    new (&lrl) LRL;
    lrl.rlh_heap = this;
}

LH::~LH ()
{
    for (math::_i32 i = 0; i < __VNZA_SL_NUMBER; i++) {
        lal_table[i].LAL::~LAL ();
    }
    lrl.LRL::~LRL ();
#if __VNZA_DEBUG_DESTRUCTION
    printf ("LH %p dying; invoking parent %p heap death\n", this, this->rhh_parent);
    fflush (stdout);
#endif
    this->rhh_parent->hook_heap_informs_heap_of_heap_death ();
#if __VNZA_DEBUG_DESTRUCTION
    printf ("LH %p dead\n", this);
    fflush (stdout);
#endif
}

InvocationResult LH::hook_linkage_informs_heap_of_surplus_block (Block * block)
{
    if (__atomic_load_n (&lrl.lrl_length, __ATOMIC_ACQUIRE) > __VNZA_LRL_CAP) {
        return this->rhh_parent->hook_heap_informs_heap_of_surplus_block (block);
    } else {
        return lrl.hook_heap_informs_linkage_of_migrating_block (block);
    }
}

InvocationResult LH::hook_linkage_informs_heap_of_evacuating_block (Block * block)
{
    // LH needs to pass directly up to RH; LH isn't going to last, but the RRH might
    return this->rhh_parent->hook_heap_informs_heap_of_evacuating_block (block);
}

InvocationResult LH::hook_linkage_informs_heap_of_block_request (OSize os, Block ** block)
{
#if __VNZA_DEBUG_TRACE_BLOCK_REQUEST
    printf ("LH block request (%p)\n", this);
#endif
    if (IR_OK == lrl.hook_heap_informs_linkage_of_block_request (os, block)) {
#if __VNZA_DEBUG_TRACE_BLOCK_REQUEST
        printf ("-> [LH] LRL (%p) request successful (%p)\n", &lrl, *block);
#endif
        return IR_OK;
    }
#if __VNZA_DEBUG_TRACE_BLOCK_REQUEST
    printf ("-> [LH] trying parent (%p)\n", rhh_parent);
#endif
    return this->rhh_parent->hook_heap_informs_heap_of_block_request (os, block);
}

InvocationResult LH::hook_client_informs_heap_of_allocation_request (OSize os, void ** object)
{
    math::_u64 _lal_luti = __vnza_chain_lookup (os);
    if (_lal_luti > __VNZA_SL_NUMBER)
        abort ();
    return lal_table[_lal_luti].hook_heap_informs_linkage_of_allocation_request (object, this);
}
