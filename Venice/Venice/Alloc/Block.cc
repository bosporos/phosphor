//
// project venice
// author Maximilien M. Cura
//

#include <Venice/Alloc/Alloc>

using namespace vnz::alloc;

Block::Block (void * _memory)
    : range_begin { _memory }
    , freeptr_local { nullptr }
    , freeptr_global { nullptr }
    , object_size { 0 }
    , object_count { 0 }
    , allocation_count { 0 }
    , activation_state { Inactive }
    , bound_thread_id ()
    , globalfree_lock ()
    , left { nullptr }
    , right { nullptr }
{}

Block::~Block ()
{
    // Does absolutely nothing.
}

InvocationResult Block::allocate_object (void ** _object)
{
    if (LIKELY (freeptr_local != nullptr)) {
        return this->__allocate_object_internal (_object);
    } else {
#ifdef __VNZA_DEBUG
        if (_vnza_debug_state & VNZA_DBG_SIGNAL_ALLOCATION_BRANCHING) {
            vnza_debug_println ("block::allocate_object -> local free pointer failed, trying global free pointer");
        }
#endif
        globalfree_lock.lock ();
        // I'm sure there's an easier way to do this
        freeptr_local  = freeptr_global;
        freeptr_global = nullptr;
        globalfree_lock.unlock ();

        if (LIKELY (freeptr_local != nullptr)) {
#ifdef __VNZA_DEBUG
            if (_vnza_debug_state & VNZA_DBG_SIGNAL_ALLOCATION_BRANCHING) {
                vnza_debug_println ("block::allocate_object -> global free pointer swap succeeds");
            }
#endif
            return this->__allocate_object_internal (_object);
        } else {
#ifdef __VNZA_DEBUG
            if (_vnza_debug_state & VNZA_DBG_SIGNAL_ALLOCATION_BRANCHING) {
                vnza_debug_println ("block::allocate_object -> global free pointer swap failed");
            }
#endif
            return IR_FAIL;
        }
    }
}

InvocationResult Block::__allocate_object_internal (void ** _object)
{
    (*_object)     = freeptr_local;
    uint16_t _diff = *static_cast<uint16_t *> (freeptr_local);
    if (_diff != 0xffff) {
        freeptr_local = static_cast<void *> (
            static_cast<uint8_t *> (range_begin) + _diff);
#ifdef __VNZA_DEBUG
    } else if ((static_cast<uint8_t *> (freeptr_local) - static_cast<uint8_t *> (range_begin)) >= 0x4000) {
        vnza_debug_println ("block::__allocate_object_internal -> free chain corruption detected.");
        vnza_debug_println ("\tdelta offset: %hu to %p", _diff, freeptr_local);
        vnza_debug_println ("\tblock: %p backed %p", this, range_begin);
        return IR_FAIL;
#endif
    } else {
        freeptr_local = nullptr;
    }
    return IR_OK;
}

InvocationResult Block::deallocate_object (void * _object)
{
#ifdef __VNZA_DEBUG
    if ((_vnza_debug_state & VNZA_DBG_SIGNAL_BAD_DEALLOC) && static_cast<uintptr_t> (static_cast<uint8_t *> (_object) - static_cast<uint8_t *> (range_begin)) >= 0x4000) {
        vnza_debug_println ("block::deallocate_object -> bad deallocation detected.");
        vnza_debug_println ("\tobject: %p at offset %lli", _object, static_cast<uint64_t> (static_cast<uint8_t *> (_object) - static_cast<uint8_t *> (range_begin)));
        vnza_debug_println ("\tblock: %p backed %p", this, range_begin);
        return IR_FAIL;
    }
#endif
    if (bound_thread_id.is_current ()) {
        if (freeptr_local != nullptr) {
            *static_cast<uint16_t *> (_object) = static_cast<uint8_t *> (freeptr_local) - static_cast<uint8_t *> (range_begin);
        } else {
            *static_cast<uint16_t *> (_object) = 0xffff;
        }
        freeptr_local = _object;
    } else {
        globalfree_lock.lock ();
        if (freeptr_global != nullptr) {
            *static_cast<uint16_t *> (_object) = static_cast<uint8_t *> (freeptr_global) - static_cast<uint8_t *> (range_begin);
        } else {
            *static_cast<uint16_t *> (_object) = 0xffff;
        }
        freeptr_global = _object;
        globalfree_lock.unlock ();
    }

    return IR_OK;
}

InvocationResult Block::format (uint16_t _object_size)
{
#ifdef __VNZA_DEBUG
    if (_object_size == 0) {
        vnza_debug_println ("block::format -> object size given as zero (0)");
        return IR_FAIL;
    }
    if (_object_size >= 0x4000) {
        vnza_debug_println ("block::format -> object size larger than block (%hu)", _object_size);
        return IR_FAIL;
    }
#endif
    object_size  = _object_size;
    object_count = 0x4000 / _object_size;
    for (uint16_t _index = 0; _index < object_count; _index++) {
        uint16_t _next_index = LIKELY (_index != object_count - 1) ? (_index + 1) : 0xffff;
        *reinterpret_cast<uint16_t *> (
            &(static_cast<uint8_t *> (range_begin)
                  [_index * _object_size]))
            = _next_index;
    }

    return IR_OK;
}

InvocationResult Block::reassign ()
{
#ifdef __VNZA_DEBUG
    uint64_t _old_owner = bound_thread_id.inner;
#endif
    bound_thread_id.bind_to_current ();
#ifdef __VNZA_DEBUG
    if (_vnza_debug_state & VNZA_DBG_SIGNAL_REASSIGNMENT) {
        vnza_debug_println ("block::format -> thread changing, moving from TID %p to TID %p", reinterpret_cast<void *> (static_cast<uint64_t> (_old_owner)), reinterpret_cast<void *> (static_cast<uint64_t> (bound_thread_id.inner)));
    }
#endif
    return IR_OK;
}

void Block::link_excise ()
{
    if (left != nullptr)
        left->right = right;
    if (right != nullptr)
        right->left = left;
    // // Don't leave behind loose ends.
    // // UPDATE: actually, yes, do leave them. clear only as necessary.
    // left  = nullptr;
    // right = nullptr;
}
