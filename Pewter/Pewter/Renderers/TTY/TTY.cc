//
// project pewter
// author Maximilien M. Cura
//

#include <Pewter/Renderers/TTY/TTY>

#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

using namespace pewter;
using namespace pewter::render::tty;

void TTYGlyphMapping::map (GlyphInternal _gi, const char * str)
{
    mapping[_gi] = str;
}

const char * TTYGlyphMapping::mapped (GlyphInternal _gi)
{
    return mapping[_gi];
}

TTYRenderer::TTYRenderer (vnz::math::_i32 _fd, TTYGlyphMapping * ttgm)
    : fd { _fd }
    , glyph_mapping { ttgm }
    , delta_cache (Rect<DisplayCoord> (0, 0, 0, 0))
{}

TTYRenderer::~TTYRenderer ()
{
    if (is_alt)
        close ();
}

void TTYRenderer::init ()
{
    tty_measure ();

    tcgetattr (fd, &tty_initial_state);
    tty_escape ("?1049h");
    tty_escape ("?25l");

    tcgetattr (fd, &tty_current_state);
    // ^C no longer sends an interrupt, but rather the sequence \377 \0 \0
    tty_current_state.c_iflag &= ~(IGNBRK | BRKINT | PARMRK);
    // Don't strip the eighth bit
    tty_current_state.c_iflag &= ~(ISTRIP);
    // Don't do LF -> CR translation, don't ignore CR, don't turn CR -> LF
    tty_current_state.c_iflag &= ~(INLCR | IGNCR);
    // Disable XON/XOFF
    tty_current_state.c_iflag &= ~(IXON);
    // Turn off implementation-defined output processing
    tty_current_state.c_oflag &= ~(OPOST);
    // zero the CSIZE mask.
    tty_current_state.c_cflag &= ~(CSIZE);
    // Set the character size mask
    tty_current_state.c_cflag |= CS8;
    // Disable parity generation
    tty_current_state.c_cflag &= ~(PARENB);
    // Bunch of random input processing stuff
    tty_current_state.c_lflag &= ~(ISIG | ICANON | IEXTEN);
    // Disable echoing, etc.
    tty_current_state.c_lflag &= ~(ICANON | ECHOE | ECHO);

    tcsetattr (fd, TCSANOW, &tty_current_state);

    is_alt = true;
}

void TTYRenderer::close ()
{
    tty_escape ("?25h");
    tty_escape ("?1049l");

    tcsetattr (fd, TCSADRAIN, &tty_initial_state);

    is_alt = false;
}

void TTYRenderer::tty_escape (const char * fmt, ...)
{
    va_list argl;
    va_start (argl, fmt);

    write (fd, "\x1b[", 2);
    vdprintf (fd, fmt, argl);

    va_end (argl);
}

void TTYRenderer::tty_measure ()
{
    struct winsize ws;

    ioctl (fd, TIOCGWINSZ, &ws);

    current_size.x = ws.ws_col;
    current_size.y = ws.ws_row;

    delta_cache.resize (Rect<DisplayCoord> (0, 0, current_size.x, current_size.y));
}

void TTYRenderer::tty_render_glyph (Glyph * glyph)
{
    // First, color
    if (glyph->properties & Glyph::PROPERTY_DEFAULT_COLOR_MASK) {
        this->tty_escape ("0m");
    }
    Color fg = glyph->coloration.foreground, bg = glyph->coloration.background;
    // if (flags & TTOM_24COLOR) {
    if (!(glyph->properties & Glyph::PROPERTY_DEFAULT_FORE)) {
        this->tty_escape ("38;2;%i;%i;%im", fg.ordered.red, fg.ordered.green, fg.ordered.blue);
    }
    if (!(glyph->properties & Glyph::PROPERTY_DEFAULT_BACK)) {
        this->tty_escape ("48;2;%i;%i;%im", bg.ordered.red, bg.ordered.green, bg.ordered.blue);
    }
    // } else {
    //     if (!(glyph->properties & Glyph::PROPERTY_DEFAULT_FORE)) {
    //         // well, 256/216/240color then
    //         if (fg.ordered.red == fg.ordered.green && fg.ordered.green == fg.ordered.blue) {
    //             // greyscale starts at 232, but 232~233 and 254!~255
    //             this->tty_escape ("38;5;%im", 233 + (fg.ordered.red / 11));
    //         } else {
    //             this->tty_escape ("38;5;%im", 16 + (36 * (fg.ordered.red / 42)) + (6 * (fg.ordered.green / 42)) + (fg.ordered.blue / 42));
    //         }
    //     }
    //     if (!(glyph->properties & Glyph::PROPERTY_DEFAULT_BACK)) {
    //         if (bg.ordered.red == bg.ordered.green && bg.ordered.green == bg.ordered.blue) {
    //             // greyscale starts at 232, but 232~233 and 254!~255
    //             this->tty_escape ("38;5;%im", 233 + (bg.ordered.red / 11));
    //         } else {
    //             this->tty_escape ("38;5;%im", 16 + (36 * (bg.ordered.red / 42)) + (6 * (bg.ordered.green / 42)) + (bg.ordered.blue / 42));
    //         }
    //     }
    // }
    dprintf (fd, (const char *)(glyph_mapping->mapped (glyph->inner)));
}

void TTYRenderer::render_full (Display * display)
{
    tty_escape ("H");

    const DisplayCoord _offset_final = display->box.size.x * display->box.size.y;
    for (DisplayCoord i = 0; i < _offset_final; i++) {
        tty_render_glyph (
            &display->layers[display->layer_index_buffer[i]]
                 ->layer_glyph_buffer[_offset_final]);
    }
}

void TTYRenderer::render_deltas (Display * display)
{
    memset (delta_cache.inner, 0, display->box.size.x * display->box.size.y);
    while (!display->deltas.empty ()) {
        DisplayDelta delta = display->deltas.back ();
        display->deltas.pop_back ();
        if (!delta_cache[delta.where.x + delta.where.y * display->box.size.x]) {
            tty_escape ("%i;%iH", delta.where.y + 1, delta.where.x + 1);
            tty_render_glyph (delta.what);
        }
    }
}
