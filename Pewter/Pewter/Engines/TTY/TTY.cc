//
// project pewter
// author Maximilien M. Cura
//

#include <Pewter/Engines/TTY/TTY>

#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>

using namespace pewter;
using namespace pewter::engine::tty;

void TTYGlyphMapping::map (GlyphInternal _gi, const char * str)
{
    mapping[_gi] = str;
}

const char * TTYGlyphMapping::mapped (GlyphInternal _gi)
{
    return mapping[_gi];
}

TTYRenderer::TTYRenderer (TTYGlyphMapping * ttgm, TTY * _tty)
    : glyph_mapping { ttgm }
    , tty { _tty }
    , delta_cache (Rect<DisplayCoord> (0, 0, 0, 0))
{}

TTYRenderer::~TTYRenderer ()
{
    if (is_alt)
        close ();
}

void TTYRenderer::notify_of_size_change ()
{
    current_size = tty->measure ();
    delta_cache.resize (Rect<DisplayCoord> (0, 0, current_size.x, current_size.y));
}

void TTYRenderer::render_glyph (Glyph * glyph)
{
    if (glyph == nullptr) {
        tty->escape ("0m");
        write (tty->out_fd, " ", 1);
        return;
    }

    // First, color
    if (glyph->properties & Glyph::PROPERTY_DEFAULT_COLOR_MASK) {
        tty->escape ("0m");
        // dprintf (tty->out_fd, "\x1b[0m");
    }
    Color fg = glyph->coloration.foreground, bg = glyph->coloration.background;
    // if (flags & TTOM_24COLOR) {

    if (!(glyph->properties & Glyph::PROPERTY_DEFAULT_FORE)) {
        tty->escape ("38;2;%i;%i;%im", fg.ordered.red, fg.ordered.green, fg.ordered.blue);
        // dprintf (tty->out_fd, "\x1b[38;2;%i;%i;%im", fg.ordered.red, fg.ordered.green, fg.ordered.blue);
    }
    if (!(glyph->properties & Glyph::PROPERTY_DEFAULT_BACK)) {
        tty->escape ("48;2;%i;%i;%im", bg.ordered.red, bg.ordered.green, bg.ordered.blue);
        // dprintf (tty->out_fd, "\x1b[48;2;%i;%i;%im", bg.ordered.red, bg.ordered.green, bg.ordered.blue);
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
    // tty->printf ((const char *)(glyph_mapping->mapped (glyph->inner)));
    dprintf (tty->out_fd, (const char *)(glyph_mapping->mapped (glyph->inner)));
    // printf (
    //     "%i -> [%s] (%i %i %i / %i %i %i)\n",
    //     glyph->inner,
    //     glyph_mapping->mapped (glyph->inner),
    //     glyph->coloration.foreground.ordered.red,
    //     glyph->coloration.foreground.ordered.green,
    //     glyph->coloration.foreground.ordered.blue,
    //     glyph->coloration.background.ordered.red,
    //     glyph->coloration.background.ordered.green,
    //     glyph->coloration.background.ordered.blue);
}

void TTYRenderer::init ()
{
    tty->measure ();

    tty->escape ("?1049h");
    tty->escape ("?25l");

    tty->enter_raw_state ();

    is_alt = true;
}

void TTYRenderer::close ()
{
    tty->escape ("?25h");
    tty->escape ("?1049l");

    tty->restore_state ();

    is_alt = false;
}

void TTYRenderer::render_full (Display * display)
{
    tty->escape ("H");

    const DisplayCoord _offset_final = display->box.size.x * display->box.size.y;
    for (DisplayCoord i = 0; i < _offset_final; i++) {

        LayerIndex li = display->layer_index_buffer[i];
        if (li == -1) {
            this->render_glyph (nullptr);
        } else {
            this->render_glyph (
                &display->layers[li]
                     ->layer_glyph_buffer[_offset_final]);
        }

        // this->render_glyph (
        // &display->layers[display->layer_index_buffer[i]]
        // ->layer_glyph_buffer[_offset_final]);
    }
}

void TTYRenderer::render_deltas (Display * display)
{
    memset (delta_cache.inner, 0, display->box.size.x * display->box.size.y);
    while (!display->deltas.empty ()) {
        DisplayDelta delta = display->deltas.back ();
        display->deltas.pop_back ();
        if (!delta_cache[delta.where.x + delta.where.y * display->box.size.x]) {
            tty->escape ("%i;%iH", delta.where.y + 1, delta.where.x + 1);
            this->render_glyph (delta.what);
        }
    }
}

TTY::TTY (vnz::math::_i32 ofd, vnz::math::_i32 ifd)
{
    out_fd   = ofd;
    bl_in_fd = ifd;
    nb_in_fd = dup (ifd);
    if (nb_in_fd == -1) {
        // problem!
        fprintf (stderr, "Could not duplicate input file descriptor as a non-blocking descriptor");
        abort ();
    }
}

TTY::~TTY ()
{
    // should be impossible, but check anyway.
    if (nb_in_fd != -1) {
        close (nb_in_fd);
    }
}

void TTY::enter_raw_state ()
{
    tcgetattr (out_fd, &tty_initial_state);

    tcgetattr (out_fd, &tty_current_state);
    // ^C no longer sends an interrupt, but rather the sequence \377 \0 \0
    tty_current_state.c_iflag &= ~(IGNBRK | BRKINT | PARMRK);
    // Don't strip the eighth bit
    tty_current_state.c_iflag &= ~(ISTRIP);
    // Don't do LF -> CR translation, don't ignore CR, don't turn CR -> LF
    tty_current_state.c_iflag &= ~(INLCR | IGNCR | ICRNL);
    // // Disable XON/XOFF
    tty_current_state.c_iflag &= ~(IXON);

    // NOTE: This screws up some stuff on macOS
    // Turn off implementation-defined output processing
    // tty_current_state.c_oflag &= ~(OPOST);

    // zero the CSIZE mask.
    tty_current_state.c_cflag &= ~(CSIZE);
    // Set the character size mask
    tty_current_state.c_cflag |= CS8;
    // Disable parity generation
    tty_current_state.c_cflag &= ~(PARENB);
    // Bunch of random input processing stuff
    tty_current_state.c_lflag &= ~(ISIG | ICANON | IEXTEN);
    // Disable echoing, etc.
    tty_current_state.c_lflag &= ~(ECHOE | ECHO | ECHONL);

    tcsetattr (out_fd, TCSANOW, &tty_current_state);
}

void TTY::restore_state ()
{
    tcsetattr (out_fd, TCSANOW, &tty_initial_state);
}

Key TTY::read_nb ()
{
    char c = 0;
    if (1 == ::read (nb_in_fd, &c, 1)) {
        if (_PWT_IS_VALID_INPUT (c)) {
            return (Key)c;
        } else {
            return VkUnknown;
        }
    } else {
        return VkNull;
    }
}

Key TTY::read ()
{
    char c = 0;
    if (1 == ::read (bl_in_fd, &c, 1)) {
        if (_PWT_IS_VALID_INPUT (c)) {
            return (Key)c;
        } else {
            return VkUnknown;
        }
    } else {
        return VkNull;
    }
}

void TTY::escape (const char * fmt, ...)
{
    va_list argl;
    va_start (argl, fmt);

    dprintf (out_fd, "\x1b[");
    // write (out_fd, "\x1b[", 2);
    vdprintf (out_fd, fmt, argl);

    va_end (argl);
}

Point<DisplayCoord> TTY::measure ()
{
    struct winsize ws;

    ioctl (out_fd, TIOCGWINSZ, &ws);

    Point<DisplayCoord> tmp (0, 0);

    tmp.x = ws.ws_col;
    tmp.y = ws.ws_row;

    return tmp;
}
