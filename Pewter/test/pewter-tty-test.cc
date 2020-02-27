//
// project pewter
// author Maximilien M. Cura
//

#include <Pewter/Engines/TTY/TTY>

#include <unistd.h>
#include <stdio.h>

using namespace pewter::engines::tty;

int main (int argc, char ** argv)
{
    TTYGlyphMapping tty_gm;
    tty_gm.map (0, u8"#");
    tty_gm.map (1, u8" ");
    tty_gm.map (2, u8"@");

    TTYOutputManager tty_om (&tty_gm);
    tty_om.init (STDOUT_FILENO, false);

    tty_om.enter_raw_state ();
    tty_om.echo (false, false);

    pewter::Glyph clear (1);
    pewter::Glyph player (2);

    pewter::GlyphSet gs (clear, pewter::color::Pair (0, 0, 0, 255, 255, 255));

    pewter::set_default_glyphset (gs);

    pewter::event::EventRegistry ev_er;
    pewter::display::Buffer pbuf;
    pewter::math::Rect<pewter::display::DisplayCoordinate> bounding_box (
        pewter::math::Point<pewter::display::DisplayCoordinate> (0, 0),
        tty_om.size ());
    pbuf.allocate_buffers (bounding_box);
    pewter::display::View view (bounding_box, &ev_er);
    view.add_buffer (&pbuf);

    TTYFrame tty_frame (&view, &tty_om);

    pbuf.mask_all (true);

    int x = 0, y = 0, xold = 0, yold = 0;

    int should_close = false;

    for (int i = 0; i < bounding_box.size.y; i++) {
        for (int j = 0; j < bounding_box.size.x; j++) {
            printf (".");
        }
        // printf ("\n");
    }
    printf ("Window: (%lli, %lli)", bounding_box.size.x, bounding_box.size.y);

    fgetc (stdin);

    tty_om.escape ("1;1H");

    while (!should_close) {
        pbuf.glyph_buffer[yold * bounding_box.size.x + xold] = clear;
        pbuf.glyph_buffer[y * bounding_box.size.x + x]       = player;
        pbuf.dirty (pewter::math::Point<pewter::display::DisplayCoordinate> (x, y), true);

        tty_frame.update ();

        int c = fgetc (stdin);
        if (c == 'q')
            should_close = true;
        if (c == '4')
            x = (x - 1) % bounding_box.size.x;
        if (c == '6')
            x = (x + 1) % bounding_box.size.x;
        if (c == '8')
            y = (y - 1) % bounding_box.size.y;
        if (c == '2')
            y = (y + 1) % bounding_box.size.y;
    }

    tty_om.close ();

    return 0;
}
