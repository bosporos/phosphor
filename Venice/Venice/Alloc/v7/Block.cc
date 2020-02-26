//
// project venice
// author Maximilien M. Cura
//

#include <Venice/Alloc/Alloc>

using namespace vnz::alloc;

Block::Block ()
    : range_begin { nullptr }
    , freeptr_local { nullptr }
    , freeptr_global { nullptr }
    , object_size { 0 }
    , object_count { 0 }
    , allocation_count { 0 }
    , activation_state { BS::Inactive }
    , btti_state { BTTI::Unbound }
    , state_change_responder { nullptr }
    , bound_thread_id ()
    , globalfree_lock ()
    , left { nullptr }
    , right { nullptr }
{}

Block::~Block ()
{}

IR Block::hook_linkage_informs_block_of_allocation_request (void ** object)
{
    if (freeptr_local != nullptr) {
        this->__local_alloc (object);
        return IR_OK;
    } else {
#ifdef __VNZA_DEBUG
        if (vnza_debug_state & VNZA_DBG_SIGNAL_ALLOCATION_BRANCHING) {
            vnza_debug_println ("hook.[lkg block]:allocation request => FPL check failed");
            vnza_debug_println ("hook.[lkg block]:allocation request => falling back to FPG");
        }
#endif

        globalfree_lock.lock ();
        freeptr_local  = freeptr_global;
        freeptr_global = nullptr;
        globalfree_lock.unlock ();

        if (freeptr_local != nullptr) {
#ifdef __VNZA_DEBUG
            if (vnza_debug_state & VNZA_DBG_SIGNAL_ALLOCATION_BRANCHING) {
                vnza_debug_println ("hook.[lkg block]:allocation request => FPL swapped successfully, continuing");
            }
#endif

            this->__local_alloc (object);
            return IR_OK;
        }

#ifdef __VNZA_DEBUG
        if (vnza_debug_state & VNZA_DBG_SIGNAL_ALLOCATION_BRANCHING) {
            vnza_debug_println ("hook.[lkg block]:allocation request => FPL could not be swapped");
        }
#endif
        return IR_FAIL;
    }
}

IR Block::hook_client_informs_block_of_deallocation_request (void * object)
{
#ifdef __VNZA_DEBUG
    if (vnza_debug_state & VNZA_DBG_SIGNAL_BAD_DEALLOC) {
        if (reinterpret_cast<uintptr_t> (static_cast<uint8_t *> (object) - static_cast<uint8_t *> (range_begin)) >= 0x4000) {
            vnza_debug_println ("hook.[cli block]:deallocation request => bad dealloc; object (%p) out of range (%p - %p)", object, range_begin, static_cast<uint8_t *> (range_begin) + 0x3fff);

            return IR_FAIL;
        }
    }
#endif

    if (__atomic_load_n (&btti_state, __ATOMIC_ACQUIRE) == BTTI::Bound && bound_thread_id.is_current ()) {
        if (freeptr_local != nullptr) {
            *static_cast<uint16_t *> (object) = static_cast<uint8_t *> (object) - static_cast<uint8_t *> (range_begin);
        } else {
            *static_cast<uint16_t *> (object) = 0xffff;
        }
        freeptr_local = object;
    } else {

        globalfree_lock.lock ();
        if (freeptr_global != nullptr) {
            *static_cast<uint16_t *> (object) = static_cast<uint8_t *> (object) - static_cast<uint8_t *> (range_begin);
        } else {
            *static_cast<uint16_t *> (object) = 0xffff;
        }
        freeptr_global = object;
        globalfree_lock.unlock ();
    }

    if (__atomic_load_n (&activation_state, __ATOMIC_ACQUIRE) == BS::Inactive) {
        uint16_t remaining = __atomic_sub_fetch (&allocation_count, 1, __ATOMIC_ACQ_REL);
        if (remaining == 0) {
            __atomic_load_n (&state_change_responder, __ATOMIC_ACQUIRE)
                ->hook_block_informs_linkage_of_empty_state (this);
        } else if (remaining <= (object_count / 4)) {
            __atomic_load_n (&state_change_responder, __ATOMIC_ACQUIRE)
                ->hook_block_informs_linkage_of_empty_enough_state (this);
        }
    }

    return IR_OK;
}
