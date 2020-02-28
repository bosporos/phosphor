//
// project pewter
// author Maximilien M. Cura
//

#include <Pewter/Display>
#include <Pewter/Renderers/TTY/TTY>

#include <unistd.h>
#include <stdio.h>

using namespace pewter;
namespace tty = pewter::render::tty;

int main (int argc, char ** argv)
{
    tty::TTYGlyphMapping glym;

    tty::TTYRenderer renderer (STDOUT_FILENO, &glym);
    renderer.init ();

    Display display (renderer.current_size);
    Layer primary_layer (display.box);

    display.layers.push_back (&primary_layer);
    display.layer_order.push_back (&primary_layer);

    glym.map (0, " ");
    glym.map (1, "@");

    display.mask_layer (&primary_layer, true);

    for (int i = 0; i < (renderer.current_size.x * renderer.current_size.y); i++) {
        primary_layer.layer_glyph_buffer[i] = Glyph (1, Pair (255, 0, 0, 0, 0, 0));
    }

    Color c (255, 0, 0);
    printf ("COLOR: %xu (%i %i %i %i)", c.raw, c.ordered.alpha, c.ordered.red, c.ordered.green, c.ordered.blue);
    fgetc (stdin);

    renderer.render_full (&display);

    Point<DisplayCoord> pos (40, 20), oldpos (39, 20);

    while (true) {
        display.at (&primary_layer, oldpos).set (0, Glyph::PROPERTY_DEFAULT_COLOR_MASK);
        display.at (&primary_layer, pos).set (1, Glyph::PROPERTIES_NONE);
        display.needs_update (&primary_layer, oldpos);
        display.needs_update (&primary_layer, pos);

        renderer.render_deltas (&display);

        int c = fgetc (stdin);

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
