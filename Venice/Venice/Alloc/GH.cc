//
// project venice
// author Maximilien M. Cura
//

#include <Venice/Compiler>
#include <Venice/Alloc/Alloc>

#include <new>

namespace math = vnz::math;
using namespace vnz::alloc;

#include <sys/mman.h>

struct __vnza_align4k_helper
{
    math::_u8 __padding0[0x4000];
};

InvocationResult GH::hook_heap_informs_heap_of_block_request (OSize os, Block ** block)
{
    void * _memory;
#if __VNZA_DEBUG_TRACE_BLOCK_REQUEST
    printf ("GH block request %hu (%p)\n", os, this);
#endif
    if (0 != posix_memalign (&_memory, 0x100000, 0x100000)) {
        // panic
        return IR_FAIL;
    }
#if __VNZA_DEBUG_GH_ALLOCATIONS
    printf ("-> [GH] allocated chunk: %p\n", _memory);
#endif

    // align it to 0x4000
    __vnza_align4k_helper * _block_align = static_cast<__vnza_align4k_helper *> (_memory);
    Block * _header_align                = static_cast<Block *> (_memory);

    // first 0x4000 is reserved for Block headers
    ++_block_align;
    // to speed up deallocation calculations (as well as to minimize malloc calls),
    // we reserve the first 128 bytes as well, and use part of that for the
    // next_chain pointer
    this->gh_chunklist_lock.lock ();
    *reinterpret_cast<void **> (_header_align) = this->gh_chunklist;
    this->gh_chunklist                         = _memory;
    this->gh_chunklist_lock.unlock ();
    // ++_header_align;

    // Initialize the block_headers
    Block * _block;
    for (math::_i32 i = 0; i < 63; i++) {
        _block              = new (&_header_align[i + 1]) Block;
        _block->range_begin = static_cast<void *> (&_block_align[i]);
#if __VNZA_DEBUG_GH_BLOCK_PLACEMENT
        printf ("--> [GH] block (%p) => %p [%p]\n", _block, _block->range_begin);
#endif
        if (i < 62) {
            gh_tributary->hook_heap_informs_heap_of_surplus_block (_block);
        }
    }

    *block = _block;
    // not a linkage, but GH has no linkages so we have to do this here
    (*block)->hook_linkage_informs_block_of_format_request (os);
    (*block)->hook_linkage_informs_block_of_reassignment ();
    (*block)->globalfree_lock.lock ();

    return IR_OK;
};

InvocationResult GH::hook_heap_informs_heap_of_heap_death ()
{
    // At GH, this doesn't actually do anything
    // RH needs it 'cuz it's RC'd, and for polymorphic purposes, GH implements this as well
    return IR_OK;
}

InvocationResult GH::hook_heap_informs_heap_of_surplus_block (Block * block)
{
    // tributary RH is dead. we're "losing" the blocks now (although theoretically we could rebuild)
    return IR_OK;
}

InvocationResult GH::hook_heap_informs_heap_of_evacuating_block (Block * block)
{
    // tributary RH is dead. we're "losing" the blocks now (although theoretically we could rebuild)
    return IR_OK;
}

GH::GH ()
    : gh_stat_chunks { 0 }
    , gh_chunklist { nullptr }
    , gh_chunklist_lock ()
    , gh_tributary { nullptr }
{}

// *will* die after all of the LHs, and thus, after all of the RHs, have died
GH::~GH ()
{
    // return memory to OS
    void *chunk = this->gh_chunklist, *next;
    while (chunk != nullptr) {
        next = *static_cast<void **> (chunk);
        free (chunk);
        chunk = next;
    }

#if __VNZA_DEBUG_DESTRUCTION
    printf ("GH %p dying\n", this);
    fflush (stdout);
#endif
}
