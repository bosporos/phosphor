//
// project pewter
// author Maximilien M. Cura
//

#include <Pewter/Display>

using namespace pewter;

DisplayDelta::DisplayDelta (Point<DisplayCoord> _p, Glyph * _g)
    : where { _p }
    , what { _g }
{}

Display::Display (Point<DisplayCoord> _size)
    : layers ()
    , deltas ()
    , layer_order ()
    , box (Point<DisplayCoord> (0, 0), _size)
    , layer_index_buffer (Rect<DisplayCoord> (0, 0, _size.x, _size.y))
{}

void Display::size_change (Rect<DisplayCoord> _new_size)
{
    this->box = _new_size;
    this->layer_index_buffer.resize (this->box);
}

#include <string.h>

void Display::mask_layer (Layer * _layer, bool _value)
{
    memset (_layer->layer_mask_buffer.inner, true, _layer->box.size.x * _layer->box.size.y);
    // this->mask_box_on_layer (_layer, _layer->box, _value);
}

void Display::mask_box_on_layer (Layer * _layer, Rect<DisplayCoord> _box, bool _value)
{
    const DisplayCoord ox = _layer->box.origin.x, oy = _layer->box.origin.y;
    const DisplayCoord ow = _layer->box.size.x, oh = _layer->box.size.y;
    const DisplayCoord bx = _box.origin.x, by = _box.origin.y;
    const DisplayCoord bw = _box.size.x, bh = _box.size.y;

    for (DisplayCoord i = 0; i < _box.size.y; i++) {
        const DisplayCoord my = by + i;
        for (DisplayCoord j = 0; j < _box.size.x; j++) {
            const DisplayCoord mx            = bx + j;
            const DisplayCoord _local_offset = mx + (my * bw);

            const bool _prev = _layer->layer_mask_buffer[_local_offset];
            if (_prev == _value)
                continue;

            const DisplayCoord _global_offset = mx + ox + (ow * (my + oy));

            _layer->layer_mask_buffer[_local_offset] = _value;
            // unnecessary extra comparison, but it's more readable this way
            if (_prev == true && _value == false) {
                if (_layer->order == this->layer_index_buffer[_global_offset]) {
                    // _layer->order == 0 would be layer_order.front()
                    // lower order = higher priority
                    // so therefore we need to look towards higher orders

                    DisplayDelta _delta { Point<DisplayCoord> (mx + ox, my + oy), nullptr };

                    auto _iter = this->layer_order.begin ();
                    std::advance (_iter, _layer->order);
                    for (; _iter != this->layer_order.end (); _iter++) {
                        if ((*_iter)->box.contains (_delta.where)) {
                            const DisplayCoord _foreign_offset = mx - ((*_iter)->box.origin.x - ox) + (*_iter)->box.size.x * (my - ((*_iter)->box.origin.y - oy));
                            if ((*_iter)->layer_mask_buffer[_foreign_offset]) {
                                _delta.what                              = &(*_iter)->layer_glyph_buffer[_foreign_offset];
                                this->layer_index_buffer[_global_offset] = (*_iter)->order;
                                break;
                            }
                        }
                    }

                    // std::list<Layer *>::reverse_iterator _liter = this->layer_order.rend () - _layer->order - 1;
                    // for (; _liter != this->layer_order.rend (); _liter++) {
                    // const DisplayCoord _foreign_offset = mx - ((*_liter)->box.origin.x - ox) + (*_liter)->box.size.x * (my - ((*_liter)->box.origin.y - oy));
                    // if ((*_liter)->layer_mask_buffer[_foreign_offset]) {
                    //     _delta.what                              = &(*_liter)->layer_glyph_buffer[_foreign_offset];
                    //     this->layer_index_buffer[_global_offset] = (*_liter)->order;
                    //     break;
                    // }
                    // }
                    if (_delta.what == nullptr) {
                        this->layer_index_buffer[_global_offset] = -1;
                    }
                    this->deltas.push_back (_delta);
                }
            } else if (_prev == false and _value == true) {
                if (_layer->order < this->layer_index_buffer[_global_offset]) {
                    this->layer_index_buffer[_global_offset] = _layer->order;
                    DisplayDelta _delta                      = {
                        Point<DisplayCoord> (mx + ox, my + oy),
                        &_layer->layer_glyph_buffer[_local_offset]
                    };
                    this->deltas.push_back (_delta);
                }
            }
        }
    }
}

Glyph & Display::at (Layer * _layer, Point<DisplayCoord> _where)
{
    // return _layer->layer_glyph_buffer[_where.x - _layer->box.origin.x + ((_where.y - _layer->box.origin.y) * _layer->box.size.x)];
    return at (_layer, _where.x, _where.y);
}

// #include <stdio.h>

Glyph & Display::at (Layer * _layer, DisplayCoord _x, DisplayCoord _y)
{
    int tr = _x - _layer->box.origin.x + ((_y - _layer->box.origin.y) * _layer->box.size.x);
    // printf ("Translation: (%i %i / %i %i / %i %i) -> %i\n", _x, _y, _layer->box.origin.x, _layer->box.origin.x, _layer->box.size.x, _layer->box.size.y, tr);
    return _layer->layer_glyph_buffer[tr];
}

void Display::needs_update (Layer * _layer, Point<DisplayCoord> _where)
{
    return needs_update (_layer, _where.x, _where.y);
}

void Display::needs_update (Layer * _layer, DisplayCoord _x, DisplayCoord _y)
{
    deltas.push_back ({ { _x, _y }, &_layer->layer_glyph_buffer[_x + _y * box.size.x] });
}

Layer::Layer (Rect<DisplayCoord> _box)
    : box { _box }
    , layer_glyph_buffer (_box)
    , layer_mask_buffer (_box)
    , order { -1 }
{}

void Layer::clear (Glyph g)
{
    const DisplayCoord l = box.size.x * box.size.y;
    for (DisplayCoord i = 0; i < l; i++) {
        layer_glyph_buffer[i] = g;
    }
}
