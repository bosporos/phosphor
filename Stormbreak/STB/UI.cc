//
// project stormbreak
// author Maximilien M. Cura
//

#include <STB/UI>

#include <map>
// #include <string>
#include <string.h>

using namespace STB;
using namespace pewter;
using namespace vnz::math;

Menu::Menu (const char * _mttl)
    : menu_title { _mttl }
{}

Menu & Menu::item (const char * _ilbl, MenuChoice _mchoice, char _key, bool _en)
{
    menu_items.push_back (Item { _ilbl, _mchoice, _key, _en });

    return *this;
}

MenuUI::MenuUI (Rect<DisplayCoord> _box, Display & _disp)
    : menu_layer (_box)
    , display { _disp }
{}

void MenuUI::ui_sizechange (Rect<DisplayCoord> _new_box)
{
    menu_layer.box = _new_box;
    menu_layer.layer_glyph_buffer.resize (_new_box);
    menu_layer.layer_mask_buffer.resize (_new_box);
}

MenuChoice make_choice (std::map<char, MenuChoice> & map)
{
    while (true) {
        Key k = tty->read ();
        if (k == VkEscape) {
            return -1;
        } else if (k == VkSpace) {
            return -2;
        } else {
            if (map.contains ((char)k)) {
                return map[(char)k];
            } else {
                tty->bell ();
            }
        }
    }
}

void place_on_layer (
    Display & display,
    Layer & layer,
    DisplayCoord x,
    const DisplayCoord y,
    const GlyphInternal * seq,
    const _usize seqlen,
    GlyphProperties gp,
    Pair coloring)
{
    for (_u64 i = 0; i < seqlen; i++) {
        display.at (&layer, x, y).set (seq[i], coloring, gp);
        display.needs_update (&layer, x++, y);
    }
}

MenuChoice MenuUI::ui_menu (Menu menu)
{
    const DisplayCoord max_lines  = menu_layer.box.size.y;
    const DisplayCoord line_width = menu_layer.box.size.x;
    const DisplayCoord lbl_width  = line_width - 4;

    DisplayCoord y                               = 0;
    std::map<char, MenuChoice> available_choices = {};
    MenuChoice exported_choice                   = MenuChoice { -1 };

    char displayed_char = 'a';

    display.mask_layer (&menu_layer, true);
    menu_layer.clear (' ');

    place_on_layer (display, menu_layer, menu_layer.box.origin.x, menu_layer.box.origin.y + y++, menu.menu_title, strlen (menu.menu_title), Glyph::PROPERTY_DEFAULT_COLOR_MASK, Pair (255, 255, 255, 0, 0, 0));

    _usize nli = menu.menu_items.size ();
    for (_usize i = 0; i < nli; i++) {
        Menu::Item item = menu.menu_items[i];

        char __operative_selector = item.item_selector;
        if (__operative_selector == ' ') {
            __operative_selector = displayed_char++;
            if (displayed_char == 'z' + 1)
                displayed_char = 'A';
            if (displayed_char == 'Z' + 1) {
                MenuChoice m_choice = make_choice (available_choices);
                if (m_choice == -2) {
                    // keep going
                    menu_layer.clear (' ');
                    y = 0;
                    available_choices.clear ();
                    displayed_char = 'a';
                    place_on_layer (display, menu_layer, menu_layer.box.origin.x, menu_layer.box.origin.y + y++, menu.menu_title, strlen (menu.menu_title), Glyph::PROPERTY_DEFAULT_COLOR_MASK, Pair (255, 255, 255, 0, 0, 0));
                } else {
                    display.mask_layer (&menu_layer, false);
                    tty_renderer->render_full (&display);
                    return m_choice;
                }
            }
        }

        const _usize lbl_len = strlen (item.item_label);

        int ncalc_y = (lbl_len / lbl_width) + (0 < (lbl_len % lbl_width));
        if ((y + ncalc_y) >= max_lines - 1 && i < nli - 1) {
            place_on_layer (display, menu_layer, menu_layer.box.origin.x, menu_layer.box.origin.y + y, "--MORE--", 8, Glyph::PROPERTY_DEFAULT_COLOR_MASK, Pair (255, 255, 255, 0, 0, 0));
            tty_renderer->render_full (&display);
            MenuChoice m_choice = make_choice (available_choices);
            if (m_choice == -2) {
                menu_layer.clear (' ');
                y = 0;
                available_choices.clear ();
                displayed_char = 'a';
                place_on_layer (display,
                                menu_layer,
                                menu_layer.box.origin.x,
                                menu_layer.box.origin.y + y++,
                                menu.menu_title,
                                strlen (menu.menu_title),
                                Glyph::PROPERTY_DEFAULT_COLOR_MASK,
                                Pair (255, 255, 255, 0, 0, 0));
            } else {
                display.mask_layer (&menu_layer, false);
                tty_renderer->render_full (&display);
                return m_choice;
            }
        }

#define ___min(a, b) (((a) < (b)) ? (a) : (b))

        menu_layer.layer_glyph_buffer[y * line_width].set (__operative_selector);
        menu_layer.layer_glyph_buffer[2 + y * line_width].set ('-');
        for (int iy = 0; iy < ncalc_y; iy++) {
            place_on_layer (display,
                            menu_layer,
                            menu_layer.box.origin.x + 4,
                            menu_layer.box.origin.y + y++,
                            &item.item_label[iy * lbl_width],
                            ___min (lbl_len - (iy * lbl_width), lbl_width),
                            Glyph::PROPERTY_DEFAULT_COLOR_MASK,
                            Pair (255, 255, 255, 0, 0, 0));
        }
    }
    tty_renderer->render_full (&display);
    MenuChoice m_choice = make_choice (available_choices);

    if (m_choice == -2)
        m_choice = -1;

    display.mask_layer (&menu_layer, false);
    tty_renderer->render_full (&display);

    return m_choice;
}
