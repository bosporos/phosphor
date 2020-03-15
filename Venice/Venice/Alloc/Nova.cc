//
// project venice
// author Maximilien Angelo M. Cura
//

#include <Venice/Alloc/Nova>
#include <libkern/OSAtomic.h>

using namespace vnz::nova;
namespace vnza = vnz::alloc;
using namespace vnz::math;

NChunk * NChunk::New ()
{
    void * memory     = vnza::internal::allocate_contiguous_aligned (VZNOVA_CHUNK_SIZE, VZNOVA_CHUNK_SIZE);
    NChunk * chunk    = reinterpret_cast<NChunk *> (memory);
    chunk->next_chunk = nullptr;
    return chunk;
}

NChunk::~NChunk ()
{
    vnza::internal::free_contiguous_memory (reinterpret_cast<void *> (this), VZNOVA_CHUNK_SIZE);
}

NBlock::NBlock (void * memory)
    : inner { memory }
{ }

NIR client_informs_block_of_deallocation_request (NBlock * bl, void * object)
{
#if VZNOVA_CHECK_DEALLOCATIONS
    const _uptr pdiff = static_cast<_uptr> (   // signed-unsigned P/N range trick
        static_cast<_u8 *> (object) - static_cast<_u8 *> (bl->inner)
    if (pdiff >= VZNOVA_SMOBJPOOL_SIZE)
    {
        return NIR_FAIL;
    }
#endif /* @VZNOVA_CHECK_DEALLOCATIONS */
    if (bl->owner.is_current_thread ()) {
        // deallocate from local
        if (bl->free_local != nullptr) {
            *static_cast<_u16 *> (object) = static_cast<_u8 *> (bl->free_local) - static_cast<_u8 *> (bl->inner);
        }
        free_local = object;
    } else {
        bl->fgm.lock ();
        if (bl->free_global != nullptr) {
            *static_cast<_u16 *> (object) = static_cast<_u8 *> (bl->free_global) - static_cast<_u8 *> (bl->inner);
        }
        free_global = object;
        bl->fgm.unlock ();
    }

    i64 resultant { OSAtomicDecrement64Barrier(&bl->acount.inner) };
    if(!resultant) {
    }
}

NIR linkage_informs_block_of_allocation_request (NBlock * bl, void ** object, NLAL * lal)
{
}
