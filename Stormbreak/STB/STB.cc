//
// project stormbreak
// author Maximilien M. Cura
//

#include <STB/ECS>
#include <STB/UI>
#include <unistd.h>

using namespace pewter;
namespace tty = pewter::engine::tty;

tty::TTY * STB::tty                  = nullptr;
tty::TTYRenderer * STB::tty_renderer = nullptr;

int main (int argc, char ** argv)
{
    set_default_glyphset (
        GlyphSet (
            Glyph (' ', Pair (0xff, 0xff, 0xff, 0, 0, 0)),
            Pair (0xff, 0xff, 0xff, 0, 0, 0)));

    tty::TTY stb_tty (STDOUT_FILENO, STDIN_FILENO);
    tty::TTYRenderer stb_renderer (&stb_tty);

    STB::tty          = &stb_tty;
    STB::tty_renderer = &stb_renderer;

    stb_renderer.notify_of_size_change ();
    stb_renderer.init ();

    Display stb_display (stb_renderer.current_size);

    Layer stb_game_layer (stb_display.box);
    STB::MenuUI menu_ui ({ 40, 0, 40, 24 }, stb_display);
    STB::NoticeUI notice_ui (stb_display);

    stb_display.layers.push_back (&menu_ui.menu_layer);
    stb_display.layers.push_back (&notice_ui.notice_layer);
    stb_display.layers.push_back (&stb_game_layer);

    // layer 0 = highest priority
    // stb_display.layers.push_back (notice_ui.notice_layer);
    // stb_display.layer_order.push_back (&menu_ui.menu_layer);
    // stb_display.layer_order.push_back (&stb_game_layer);

    // stb_display.mask_layer (&menu_ui.menu_layer, true);
    stb_display.mask_layer (&stb_game_layer, true);

    // for (int i = 0; i < (stb_game_layer.box.size.x * stb_game_layer.box.size.y); i++) {
    //     stb_game_layer.layer_glyph_buffer[i] = Glyph ('#', Pair (255, 0, 0, 0, 0, 0));
    // }
    // for (int i = 0; i < (menu_ui.menu_layer.box.size.x * menu_ui.menu_layer.box.size.y); i++) {
    //     menu_ui.menu_layer.layer_glyph_buffer[i] = Glyph ('.', Pair (0, 255, 0, 0, 0, 0));
    // }
    stb_game_layer.clear (' ');

    // printf ("Primary layer: %i x %i @ %i %i\n", stb_game_layer.box.size.x, stb_game_layer.box.size.y, stb_game_layer.box.origin.x, stb_game_layer.box.origin.y);
    // printf ("Menu layer: %i x %i @ %i %i\n", menu_ui.menu_layer.box.size.x, menu_ui.menu_layer.box.size.y, menu_ui.menu_layer.box.origin.x, menu_ui.menu_layer.box.origin.y);

    // stb_renderer.render_full (&stb_display);
    // stb_tty.escape ("H");
    // for (int i = 0; i < (stb_display.box.size.x * stb_display.box.size.y); i++) {
    //     stb_tty.render_glyph (&display.layers[0].layer_glyph_buffer[i]);
    // }

    // stb_tty.read ();

    STB::Menu my_menu ("My first menu!");
    my_menu
        .item ("Item 1 Is An Extremely, Extremely, Extremely Long Menu Item", 1)
        .item ("Item 2  Is An Extremely, Extremely, Extremely Long Menu Item", 2)
        .item ("Item 3 Is An Extremely, Extremely, Extremely Long Menu Item", 3)
        .item ("Item 4 Is An Extremely, Extremely, Extremely Long Menu Item", 4)
        .item ("Item 5 Is An Extremely, Extremely, Extremely Long Menu Item", 5)
        .item ("Item 6 Is An Extremely, Extremely, Extremely Long Menu Item", 6)
        .item ("Item 7 Is An Extremely, Extremely, Extremely Long Menu Item", 7)
        .item ("Item 8 Is An Extremely, Extremely, Extremely Long Menu Item", 8)
        .item ("Item 9 Is An Extremely, Extremely, Extremely Long Menu Item", 9)
        .item ("Item 10 Is An Extremely, Extremely, Extremely Long Menu Item", 10)
        .item ("Item 11 Is An Extremely, Extremely, Extremely Long Menu Item", 11)
        .item ("Item 12 Is An Extremely, Extremely, Extremely Long Menu Item", 12)
        .item ("Item 13 Is An Extremely, Extremely, Extremely Long Menu Item", 13)
        .item ("Item 14 Is An Extremely, Extremely, Extremely Long Menu Item", 14)
        .item ("Item 15 Is An Extremely, Extremely, Extremely Long Menu Item", 15)
        .item ("Item 16", 16)
        .item ("Item 17", 17)
        .item ("Item 18", 18)
        .item ("Item 19", 19)
        .item ("Item 20", 20)
        .item ("Item 21", 21)
        .item ("Item 22", 22)
        .item ("Item 23", 23)
        .item ("Item 24", 24)
        .item ("Item 25", 25)
        .item ("Item 26", 26)
        .item ("Item 27", 27)
        .item ("Item 28", 28)
        .item ("Item 29", 29)
        .item ("Item 30", 30)
        .item ("Item 31", 31)
        .item ("Item 32", 32)
        .item ("Item 33", 33)
        .item ("Item 34", 34)
        .item ("Item 35", 35);

    STB::MenuChoice mc = menu_ui.ui_menu (my_menu);

    // stb_tty.read ();

    stb_renderer.close ();

    printf ("Choice: %i", (int)mc);

    return 0;
}
