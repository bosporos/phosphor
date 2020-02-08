//
// project venice
// author Maximilien M. Cura
//

#include <Venice/Alloc/Alloc>
#include <Venice/Compiler>

using namespace vnz::math;
using namespace vnz::alloc;

Block::Block ()
    : range_begin { nullptr }
    , freeptr_local { nullptr }
    , freeptr_global { nullptr }
    , object_size { 0 }
    , object_count { 0 }
    , allocation_count { 0 }
    , block_state { Inactive }
    , state_change_responder { nullptr }   // needs to be set!!!
    , bound_thread_id ()
    , globalfree_lock ()
    , l_left { nullptr }
    , l_right { nullptr }
{}

Block::~Block ()
{
#if __VNZA_DEBUG_DESTRUCTION
    printf ("Block dead: %p (%p)\n", this, this->state_change_responder);
    fflush (stdout);
#endif
}

InvocationResult Block::hook_linkage_informs_block_of_allocation_request (void ** object)
{
    if (freeptr_local != nullptr) {
        // always returns IR_OK
        __atomic_add_fetch (&allocation_count, 1, __ATOMIC_ACQ_REL);
        __local_alloc (object);
        return IR_OK;
    } else {
        printf ("DEFAULTING TO FPG CHECK (%p)\n", this);
        globalfree_lock.lock ();
        // dont know if compiler is smart enough to recognize this as a swap operation
        // #if 0 && CPU(X86_64)
        //         // __asm__(
        //         //     "xchgq %0, %1;\n"
        //         //     : "+m"(freeptr_local), "+m"(freeptr_global)
        //         //     :
        //         //     : "memory");
        // #else
        freeptr_local  = freeptr_global;
        freeptr_global = nullptr;
        // #endif
        globalfree_lock.unlock ();
        if (freeptr_local != nullptr) {
            printf ("    SUCCESSFUL\n");
            // always returns IR_OK;
            __atomic_add_fetch (&allocation_count, 1, __ATOMIC_ACQ_REL);
            __local_alloc (object);
            return IR_OK;
        }
        printf ("    FAIL\n");
        return IR_FAIL;
    }
}

InvocationResult Block::__local_alloc (void ** object)
{
    *object  = freeptr_local;
    _u16 off = *static_cast<_u16 *> (*object);
    if (off != 0xffff) {
        freeptr_local = static_cast<void *> (static_cast<_u8 *> (range_begin) + off);
    } else {
        freeptr_local = nullptr;
    }
    return IR_OK;
}

InvocationResult Block::hook_client_informs_block_of_deallocation_request (void * object)
{
    if (static_cast<_usize> (static_cast<_u8 *> (object) - static_cast<_u8 *> (range_begin)) >= 0x4000) {
        __asm__("ud2");
    }
    if (bound_thread_id.is_current ()) {
        if (LIKELY (freeptr_local != nullptr)) {
            *static_cast<_u16 *> (object) = static_cast<_u8 *> (freeptr_local) - static_cast<_u8 *> (range_begin);
        } else {
            *static_cast<_u16 *> (object) = 0xffff;
        }
        freeptr_local = object;
        OCount oc     = __atomic_sub_fetch (&allocation_count, 1, __ATOMIC_ACQ_REL);
        if (__atomic_load_n (&block_state, __ATOMIC_ACQUIRE) != Active) {
            if (oc == 0)
                // if we took it down to 0...
                state_change_responder->hook_block_informs_linkage_of_empty_state (this);
            else if (oc <= object_count / 4)
                // if we made it empty enough...
                state_change_responder->hook_block_informs_linkage_of_empty_enough_state (this);
        }
    } else {
        globalfree_lock.lock ();
        if (LIKELY (freeptr_global != nullptr)) {
            *static_cast<_u16 *> (object) = static_cast<_u8 *> (freeptr_global) - static_cast<_u8 *> (range_begin);
        } else {
            *static_cast<_u16 *> (object) = 0xffff;
        }
        freeptr_global = object;
        // aight we need this INSIDE the globalfree_lock
        OCount oc = __atomic_sub_fetch (&allocation_count, 1, __ATOMIC_ACQ_REL);
        if (__atomic_load_n (&block_state, __ATOMIC_ACQUIRE) != Active) {
            if (oc == 0)
                // if we took it down to 0...
                state_change_responder->hook_block_informs_linkage_of_empty_state (this);
            else if (oc <= object_count / 4)
                // if we made it empty enough...
                state_change_responder->hook_block_informs_linkage_of_empty_enough_state (this);
        }
        globalfree_lock.unlock ();
    }
    return IR_OK;
}

InvocationResult Block::hook_linkage_informs_block_of_format_request (OSize size_to_format)
{
    // we know we have sole control and that we are empty
    // therefore, allocation_count is already 0

    // quick check
    // if (size_to_format != object_size) {
    object_size  = size_to_format;
    object_count = 0x4000 / object_size;
    // object_count - 1 b/c offset 0 is object 0
    _u8 * bptr = static_cast<_u8 *> (range_begin) + object_size * (object_count - 1);
    _u16 index = 0xffff;
    while (bptr >= static_cast<_u8 *> (range_begin)) {
        *reinterpret_cast<_u16 *> (bptr) = index;
        index                            = bptr - static_cast<_u8 *> (range_begin);
        bptr -= object_size;
    }
    freeptr_local  = range_begin;
    freeptr_global = nullptr;
    // }
    return IR_OK;
}

InvocationResult Block::hook_linkage_informs_block_of_reassignment ()
{
    bound_thread_id.bind_to_current ();
    return IR_OK;
}
