//
// project venice
// author Maximilien M. Cura
//

#include <Venice/Alloc/Alloc>
#include <Venice/Alloc/Internal>

#include <new>

using namespace vnz;

/*
 * Slab
 */

alloc::Slab::Slab (usize _size, usize _align)
    : memory { alloc::internal::allocate_contiguous_aligned (_size, _align) }
    , size { _size }
    , align { _align }
{ }

alloc::Slab::~Slab ()
{
    alloc::internal::free_contiguous_memory (memory, size);
}

void * alloc::Slab::byte_offset (usize _offset)
{
    return static_cast<void *> (static_cast<u8 *> (memory) + _offset);
}

usize alloc::Slab::offset_align_ceil (usize _offset, usize _align)
{
    usize _abs_offset = reinterpret_cast<usize::UnderlyingType> (static_cast<u8 *> (memory)) + _offset;
    if (_abs_offset % _align == 0)
        return _abs_offset;
    _abs_offset += _align;
    _abs_offset -= (_abs_offset % _align);
    return _abs_offset;
}

/*
 * RegionAllocator
 */

alloc::RegionAllocator::RegionAllocator (usize _size, usize _align)
    : slab (_size, _align)
{ }

alloc::RegionAllocator::~RegionAllocator ()
{
    // Slab destructor handles deallocation
}

void * alloc::RegionAllocator::allocate_aligned (usize _osize, usize _oalign)
{
    usize starting_offset = slab.offset_align_ceil (mark, _oalign);
    // not sure if this should be +_osize or +_oalign
    mark = starting_offset + _osize;
    return slab.byte_offset (starting_offset);
}

/*
 * RCRegionAllocator
 */

alloc::RCRegionAllocator::RCRegionAllocator (usize _size, usize _align)
{
    new (&slab) Slab (_size, _align);
}

alloc::RCRegionAllocator::~RCRegionAllocator ()
{
    if (0 < extant_references || 0 == mark) {
        this->free_region ();
    }
}

void * alloc::RCRegionAllocator::allocate_aligned (usize _osize, usize _oalign)
{
    usize starting_offset = slab.offset_align_ceil (mark, _oalign);
    mark                  = starting_offset + _osize;
    extant_references++;
    return slab.byte_offset (starting_offset);
}

void alloc::RCRegionAllocator::deallocate ()
{
    if (0 == --extant_references) {
        this->free_region ();
    }
}

void alloc::RCRegionAllocator::free_region ()
{
    slab.~Slab ();
}
