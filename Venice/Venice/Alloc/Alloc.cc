//
// project venice
// author Maximilien M. Cura
//

#include <Venice/Alloc/Alloc>

vnz::alloc::DebugSignaling vnz::alloc::_vnza_debug_state
    = static_cast<vnz::alloc::DebugSignaling> (
        vnz::alloc::VNZA_DBG_SIGNAL_DESTRUCTION_ORDER
        | vnz::alloc::VNZA_DBG_SIGNAL_ALLOCATION_BRANCHING
        | vnz::alloc::VNZA_DBG_SIGNAL_DEALLOCATION_BRANCHING
        | vnz::alloc::VNZA_DBG_SIGNAL_CHUNK_LAYOUT
        | vnz::alloc::VNZA_DBG_SIGNAL_LINKAGE_CHANGES
        | vnz::alloc::VNZA_DBG_SIGNAL_BAD_DEALLOC);
