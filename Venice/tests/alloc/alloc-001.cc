//
// project venice
// author Maximilien M. Cura
//

#include <Venice/Alloc/Alloc>

namespace math = vnz::math;
using namespace vnz::alloc;

LH * __vnza_lh_threadinit ();

thread_local LH * __vnza_lh__ = __vnza_lh_threadinit ();
thread_local LH __vnza_lh;
static GH __vnza_gh;
static RH __vnza_rh_main;

void * __vnza_malloc (math::_usize size)
{
    void * object;
    if (size <= 0x2000) {
        if (IR_OK == __vnza_lh.hook_client_informs_heap_of_allocation_request (size, &object)) {
            return object;
        } else {
            return nullptr;
        }
    } else {
        object = malloc (size);
        if (object != NULL) {
            return object;
        } else {
            return nullptr;
        }
    }
}

void __vnza_free (void * object)
{
    const math::_usize o = reinterpret_cast<math::_usize> (object);
    Block * floor        = reinterpret_cast<Block *> (o & ~(0x100000ull << 1));
    floor[(o % 0x100000ull) / 0x4000].hook_client_informs_block_of_deallocation_request (object);
}

LH * __vnza_lh_threadinit ()
{
    __vnza_lh.rhh_parent = &__vnza_rh_main;
    __atomic_add_fetch (&__vnza_rh_main.rh_active_references, 1, __ATOMIC_ACQ_REL);
    return &__vnza_lh;
}

void __vnza_init ()
{
    __vnza_gh.gh_tributary    = &__vnza_rh_main;
    __vnza_rh_main.rhh_parent = &__vnza_gh;
}

#include <stdio.h>

int main (int argc, char ** argv)
{
    __vnza_init ();

    void * p;

    for (int i = 0; i < 0xffff; i++) {
        printf ("%i --> ", i);
        p = __vnza_malloc (8);
        printf ("%i %p\n", i, p);
        __vnza_free (p);
    }

    exit (0);
    // return 0;
}
