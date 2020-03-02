//
// project pewter
// author Maximilien M. Cura
//

#include <Pewter/Display>
#include <Pewter/Engines/TTY/TTY>

#include <unistd.h>
#include <stdio.h>

using namespace pewter;
namespace tty = pewter::engine::tty;

void tty_main ();

int main (int argc, char ** argv)
{
    tty::TTYGlyphMapping glym;
    glym.map (0, " ");
    glym.map (1, "@");

    GlyphSet gs (Glyph (0, Pair (0xff, 0xff, 0xff, 0, 0, 0)), Pair (0xff, 0xff, 0xff, 0, 0, 0));
    set_default_glyphset (gs);

    tty::TTY tty (STDOUT_FILENO, STDIN_FILENO);

    tty::TTYRenderer renderer (&glym, &tty);
    renderer.notify_of_size_change ();
    renderer.init ();

    Display display (renderer.current_size);
    Layer primary_layer (display.box);

    display.layers.push_back (&primary_layer);
    display.layer_order.push_back (&primary_layer);

    display.mask_layer (&primary_layer, true);

    for (int i = 0; i < (renderer.current_size.x * renderer.current_size.y); i++) {
        primary_layer.layer_glyph_buffer[i] = Glyph (1, Pair (255, 0, 0, 0, 0, 0));
    }

    // Color c (255, 0, 0);
    // tty.printf ("COLOR: %xu (%i %i %i %i)", c.raw, c.ordered.alpha, c.ordered.red, c.ordered.green, c.ordered.blue);
    // tty.read ();

    renderer.render_full (&display);

    Point<DisplayCoord> pos (40, 20), oldpos (39, 20);

    while (true) {
        // printf ("old: (%i %i) new: (%i %i)\n", oldpos.x, oldpos.y, pos.x, pos.y);
        display.at (&primary_layer, oldpos).set (0, Glyph::PROPERTY_DEFAULT_COLOR_MASK);
        display.at (&primary_layer, pos).set (1, Glyph::PROPERTIES_NONE);
        display.needs_update (&primary_layer, oldpos);
        display.needs_update (&primary_layer, pos);

        renderer.render_deltas (&display);

        int c = tty.read ();

        oldpos = pos;
        if (c == '8')
            pos.y--;
        if (c == '2')
            pos.y++;
        if (c == '4')
            pos.x--;
        if (c == '6')
            pos.x++;
        if (c == 'q')
            break;
    }

    renderer.close ();

    return 0;
}
