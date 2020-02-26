//
// project pewter
// author Maximilien M. Cura
//

#include <Pewter/Display/Buffer>

namespace vma = vnz::math;
using namespace pewter::display;

Buffer::Buffer ()
    : glyph_buffer { nullptr }
    , dirty_buffer { nullptr }
    , mask_buffer { nullptr }
    , bounding_box (0, 0, 0, 0)
    , buffer_is_allocated { false }
{}

Buffer::~Buffer ()
{
    if (buffer_is_allocated) {
        delete[] glyph_buffer;
        delete[] dirty_buffer;
        delete[] mask_buffer;
    }
}

void Buffer::allocate_buffers (math::Rect<DisplayCoordinate> _bbox)
{
    if (buffer_is_allocated) {
        delete[] glyph_buffer;
        delete[] dirty_buffer;
        delete[] mask_buffer;
    }
    glyph_buffer = new Glyph[_bbox.size.x * _bbox.size.y];
    dirty_buffer = new vma::_u64[_bbox.size.x * _bbox.size.y / 64];
    mask_buffer  = new vma::_u64[_bbox.size.x * _bbox.size.y / 64];
    dirty_all (true);
    mask_all (true);

    bounding_box        = _bbox;
    buffer_is_allocated = true;
}

void Buffer::mask (math::Point<DisplayCoordinate> _point, bool _value)
{
    DisplayCoordinate _offset = _point.x + _point.y * bounding_box.size.x;
    mask_buffer[_offset / 64] = static_cast<vma::_u64> (_value) << (_offset % 64);

    dirty (_point, true);
}

void Buffer::mask (math::Rect<DisplayCoordinate> _box, bool _value)
{
    typedef vma::_u64 Q;
    // first line:
    //  A <- (OX + OY*BSW)
    //  B <- A / 64
    //  C <- A % 64
    //  D <- (~(V-1))
    //  E1 <- D >> C
    //  E2 <- ~E1
    //   M[B] <- (M[B] & E2) | (D & E1)
    //   M[B+k] <- D
    //   M[B+K] <- (M[B+K] & E1) | (D & E2)

    const Q c                 = _box.origin.x % 64;
    const Q d                 = ~(_value - 1);
    const Q e1                = d >> c;
    const Q e2                = d << ((_box.origin.x + _box.size.x) % 64);
    const DisplayCoordinate w = _box.size.x + (_box.origin.x % 64) + (64 - ((_box.origin.x + _box.size.x) % 64));
    const DisplayCoordinate i = w / 64;
    const DisplayCoordinate I = i - 2;
    Q a                       = _box.origin.x + (_box.origin.y * bounding_box.size.x);
    for (int line = 0; line < _box.size.y; line++) {
        const Q b = a / 64;

        if (i == 1) {
            const DisplayCoordinate ox = _box.origin.x % 64;
            Q mask                     = (~static_cast<Q> (0)) >> (64 - _box.size.x) << (64 - _box.size.x - ox);
            mask_buffer[b]             = (~mask & mask_buffer[b]) | (d & mask);
        } else {
            mask_buffer[b] = (mask_buffer[b] & e2) | (d & e1);
            int j          = 0;
            for (j = 0; j < I; j++) {
                mask_buffer[b + 1 + j] = d;
            }
            mask_buffer[b + j] = (mask_buffer[b] & e1) | (d & e2);
        }
        a += bounding_box.size.x;
    }

    dirty (_box, true);
}

void Buffer::mask_all (bool value)
{
    const vnz::math::_usize _len  = bounding_box.size.x * bounding_box.size.y;
    const vnz::math::_usize len   = (_len / 64);
    const vnz::math::_u64 backing = ~(static_cast<vnz::math::_u64> (value) - 1);
    for (vnz::math::_usize i = 0; i < len; i++) {
        mask_buffer[i] = backing;
    }
    if (_len % 64) {
        mask_buffer[len] = ~(backing >> (_len % 64));
    }

    dirty_all (true);
}

void Buffer::dirty (math::Point<DisplayCoordinate> _point, bool _value)
{
    DisplayCoordinate _offset  = _point.x + _point.y * bounding_box.size.x;
    dirty_buffer[_offset / 64] = static_cast<vma::_u64> (_value) << (_offset % 64);

    buffer_has_deltas = true;
}

void Buffer::dirty (math::Rect<DisplayCoordinate> _box, bool _value)
{
    typedef vma::_u64 Q;
    // first line:
    //  A <- (OX + OY*BSW)
    //  B <- A / 64
    //  C <- A % 64
    //  D <- (~(V-1))
    //  E1 <- D >> C
    //  E2 <- ~E1
    //   D[B] <- (D[B] & E2) | (D & E1)
    //   D[B+k] <- D
    //   D[B+K] <- (D[B+K] & E1) | (D & E2)

    Q a                       = _box.origin.x + (_box.origin.y * bounding_box.size.x);
    const Q c                 = _box.origin.x % 64;
    const Q d                 = ~(_value - 1);
    const Q e1                = d >> c;
    const Q e2                = d << ((_box.origin.x + _box.size.x) % 64);
    const DisplayCoordinate w = _box.size.x + (_box.origin.x % 64) + (64 - ((_box.origin.x + _box.size.x) % 64));
    const DisplayCoordinate i = w / 64;
    const DisplayCoordinate I = i - 2;
    for (int line = 0; line < _box.size.y; line++) {
        const Q b = a / 64;
        if (i == 1) {
            const DisplayCoordinate ox = _box.origin.x % 64;
            Q mask                     = (~static_cast<Q> (0)) >> (64 - _box.size.x) << (64 - _box.size.x - ox);
            dirty_buffer[b]            = (~mask & dirty_buffer[b]) | (d & mask);
        } else {
            dirty_buffer[b] = (dirty_buffer[b] & e2) | (d & e1);
            int j           = 0;
            for (j = 0; j < I; j++) {
                dirty_buffer[b + 1 + j] = d;
            }
            dirty_buffer[b + j] = (dirty_buffer[b] & e1) | (d & e2);
        }
        a += bounding_box.size.x;
    }

    buffer_has_deltas = true;
}

void Buffer::dirty_all (bool value)
{
    const vnz::math::_usize _len  = bounding_box.size.x * bounding_box.size.y;
    const vnz::math::_usize len   = (_len / 64);
    const vnz::math::_u64 backing = ~(static_cast<vnz::math::_u64> (value) - 1);
    for (vnz::math::_usize i = 0; i < len; i++) {
        dirty_buffer[i] = backing;
    }
    if (_len % 64) {
        dirty_buffer[len] = ~(backing >> (_len % 64));
    }

    buffer_has_deltas = true;
}

void Buffer::undirty ()
{
    buffer_has_deltas = false;
}
