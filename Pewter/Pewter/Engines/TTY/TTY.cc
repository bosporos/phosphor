//
// project pewter
// author Maximilien M. Cura
//

#include <Pewter/Engines/TTY/TTY> /* already provides termios for tc?etattr */

#include <sys/ioctl.h> /* tiocgwinsz, ioctl */
#include <unistd.h> /* write */
#include <stdarg.h> /* va_* */
#include <stdio.h> /* dprintf */
#include <string.h> /* memset */

using namespace pewter::engines::tty;

void TTYGlyphMapping::map (pewter::GlyphInternal gi, const char8_t * sutf8)
{
    glyph_mapping[gi] = sutf8;
}

const char8_t * TTYGlyphMapping::mapped (pewter::GlyphInternal gi)
{
    return glyph_mapping[gi];
}

TTYOutputManager::TTYOutputManager (TTYGlyphMapping * ttgm)
    : fd { -1 }
    , flags { 0 }
    , glyph_mapping { ttgm }
    , initial_ttystate {}
    , current_ttystate {}
{}

int TTYOutputManager::init (vnz::math::_i32 _fd, bool _256override)
{
    fd = _fd;
    if (this->color_support () == TTYColorSupport::Truecolor)
        flags = (TTOM_24COLOR);
    else {
        if (_256override)
            flags = 0;
        else
            return -1;
    }
    tcgetattr (fd, &initial_ttystate);
    // this->escape ("?1049h");
    tcgetattr (fd, &current_ttystate);

    return 0;
}

void TTYOutputManager::close ()
{
    // this->escape ("?1049l");
    tcsetattr (fd, TCSADRAIN, &initial_ttystate);
}

void TTYOutputManager::enter_raw_state ()
{
    tcgetattr (fd, &current_ttystate);

    // ^C no longer sends an interrupt, but rather the sequence \377 \0 \0
    current_ttystate.c_iflag &= ~(IGNBRK | BRKINT | PARMRK);
    // Don't strip the eighth bit
    current_ttystate.c_iflag &= ~(ISTRIP);
    // Don't do LF -> CR translation, don't ignore CR, don't turn CR -> LF
    current_ttystate.c_iflag &= ~(INLCR | IGNCR);
    // Disable XON/XOFF
    current_ttystate.c_iflag &= ~(IXON);
    // Turn off implementation-defined output processing
    current_ttystate.c_oflag &= ~(OPOST);
    // zero the CSIZE mask.
    current_ttystate.c_cflag &= ~(CSIZE);
    // Set the character size mask
    current_ttystate.c_cflag |= CS8;
    // Disable parity generation
    current_ttystate.c_cflag &= ~(PARENB);
    // Bunch of random input processing stuff
    current_ttystate.c_lflag &= ~(ISIG | ICANON | IEXTEN);

    tcsetattr (fd, TCSANOW, &current_ttystate);

    this->echo (false, false);
}

void TTYOutputManager::echo (bool normal, bool lineedit)
{
    tcgetattr (fd, &current_ttystate);

    if (normal) {
        if (lineedit) {
            current_ttystate.c_lflag |= (ICANON | ECHO | ECHOE);
        } else {
            current_ttystate.c_lflag &= ~(ICANON | ECHOE);
            current_ttystate.c_lflag |= ECHO;
        }
    } else {
        current_ttystate.c_lflag &= ~(ICANON | ECHOE | ECHO);
    }

    tcsetattr (fd, TCSANOW, &current_ttystate);
}

pewter::math::Point<pewter::display::DisplayCoordinate> TTYOutputManager::size ()
{
    struct winsize ws;
    ioctl (fd, TIOCGWINSZ, &ws);
    return { ws.ws_col, ws.ws_row };
}

void TTYOutputManager::escape (const char * fmt, ...)
{
    va_list vargs;
    va_start (vargs, fmt);
    write (fd, "\e[", 2);
    vdprintf (fd, fmt, vargs);
    va_end (vargs);
}

void TTYOutputManager::express (pewter::Glyph * glyph)
{
    // First, color
    if (glyph->properties & Glyph::PROPERTY_DEFAULT_COLOR_MASK) {
        this->escape ("0m");
    }
    color::Color fg = glyph->coloration.foreground, bg = glyph->coloration.background;
    if (flags & TTOM_24COLOR) {
        if (!(glyph->properties & Glyph::PROPERTY_DEFAULT_FORE)) {
            this->escape ("38;2;%i;%i;%im", fg.ordered.red, fg.ordered.green, fg.ordered.blue);
        }
        if (!(glyph->properties & Glyph::PROPERTY_DEFAULT_BACK)) {
            this->escape ("48;2;%i;%i;%im", bg.ordered.red, bg.ordered.green, bg.ordered.blue);
        }
    } else {
        if (!(glyph->properties & Glyph::PROPERTY_DEFAULT_FORE)) {
            // well, 256/216/240color then
            if (fg.ordered.red == fg.ordered.green && fg.ordered.green == fg.ordered.blue) {
                // greyscale starts at 232, but 232~233 and 254!~255
                this->escape ("38;5;%im", 233 + (fg.ordered.red / 11));
            } else {
                this->escape ("38;5;%im", 16 + (36 * (fg.ordered.red / 42)) + (6 * (fg.ordered.green / 42)) + (fg.ordered.blue / 42));
            }
        }
        if (!(glyph->properties & Glyph::PROPERTY_DEFAULT_BACK)) {
            if (bg.ordered.red == bg.ordered.green && bg.ordered.green == bg.ordered.blue) {
                // greyscale starts at 232, but 232~233 and 254!~255
                this->escape ("38;5;%im", 233 + (bg.ordered.red / 11));
            } else {
                this->escape ("38;5;%im", 16 + (36 * (bg.ordered.red / 42)) + (6 * (bg.ordered.green / 42)) + (bg.ordered.blue / 42));
            }
        }
    }
    dprintf (fd, (const char *)(glyph_mapping->mapped (glyph->inner)));
}

TTYColorSupport TTYOutputManager::color_support ()
{
    // I can't find a consistent way to detect 256-color support, so I'm leaving
    // that as a user-only option.

    // note: do not repeat NOT want to try to free ``colorterm''
    char * colorterm = getenv ("COLORTERM");
    if (colorterm == NULL) {
        // Problematic
        return TTYColorSupport::Unknown;
    } else {
        if (!strcmp (colorterm, "truecolor") || !strcmp (colorterm, "24bit")) {
            return TTYColorSupport::Truecolor;
        } else {
            return TTYColorSupport::Unknown;
        }
    }
}

TTYFrame::TTYFrame (display::View * _view, TTYOutputManager * om)
    : Frame (_view)
    , output_manager { om }
{}

inline vnz::math::_u64 fast_brev (vnz::math::_u64 n)
{
    // https://stackoverflow.com/a/16994674/3743152
    __asm__("bswapq %0"
            : "=r"(n)
            : "0"(n));
    //\\//\\ EDIT: bytes = 8
    // n >>= ((sizeof (size_t) - bytes) * 8);
    n = ((n & 0xaaaaaaaaaaaaaaaa) >> 1) | ((n & 0x5555555555555555) << 1);
    n = ((n & 0xcccccccccccccccc) >> 2) | ((n & 0x3333333333333333) << 2);
    n = ((n & 0xf0f0f0f0f0f0f0f0) >> 4) | ((n & 0x0f0f0f0f0f0f0f0f) << 4);
    return n;
}

void TTYFrame::update ()
{
    // Well, this'll be a bit complicated
    math::Point<display::DisplayCoordinate> size = output_manager->size ();
    if ((output_manager->flags & TTOM_SIZE_CHANGE) || !_draw_cache_allocated) {
        if (_draw_cache_allocated) {
            delete[] draw_cache;
        }
        draw_cache = new bool[size.x * size.y];
    }
    memset (draw_cache, 0, size.x * size.y);

    // basically:
    for (display::Buffer * cbuf : view->view_buffers) {
        math::Rect<display::DisplayCoordinate> bbox = cbuf->bounding_box;
        display::DisplayCoordinate far_y = bbox.origin.y + bbox.size.y, far_x = bbox.origin.x + bbox.size.x, near_y = bbox.origin.y, near_x = bbox.origin.x;
        for (int y = near_y; y < far_y; y++) {
            display::DisplayCoordinate nx_offset  = y * size.x + near_x;
            display::DisplayCoordinate nxi_offset = nx_offset % 64;
            vnz::math::_u64 * dbuf_pointer        = &cbuf->dirty_buffer[nx_offset / 64];
            vnz::math::_u64 * mbuf_pointer        = &cbuf->mask_buffer[nx_offset / 64];
            vnz::math::_u64 dbuf_c = (*dbuf_pointer++) << nxi_offset, mbuf_c = (*mbuf_pointer++) << nxi_offset;
            dbuf_c = fast_brev (dbuf_c), mbuf_c = fast_brev (mbuf_c);
            bool fastadvance_possible = false;
            for (int x = near_x; x < far_x; x++) {
                // a) It's dirty and opaque
                // b) It's dirty and (newly) transparent
                // c) It's clean and transparent
                // d) It's clean and opaque and IS super-masked
                // e) It's clean and opaque and ISN'T super-masked

                if (!(draw_cache[nx_offset] & 1)) {
                    if (dbuf_c & 1) {
                        if (mbuf_c & 1) {
                            // dirty and opaque
                            goto __draw__;
                            draw_cache[nx_offset] = 1;
                        } else {
                            // CASE B: we just became transparent.
                            // de-super-mask-ify
                            draw_cache[nx_offset] |= 0x80;
                        }
                    } else {
                        if ((mbuf_c & 1)) {
                            // CASE C: clean and transparent
                            // not drawn yet, so !draw_cache[...] is the de-super-mask-ify thing
                            if (!draw_cache[nx_offset]) {
                                // (and not yet de-super-mask-ified + not yet drawn
                                draw_cache[nx_offset] = 0x80;
                            }
                        } else {
                            if (draw_cache[nx_offset] & 0x80) {
                                // CASE E: NOT super-masked
                                goto __draw__;
                                draw_cache[nx_offset] = 1;
                            } else {
                                // CASE D: super-masked
                                // do nothing
                            }
                        }
                    }
                }
                fastadvance_possible = false;
                goto __advance__;
            __draw__:
                if (fastadvance_possible) {
                    output_manager->escape ("C");
                } else {
                    output_manager->escape ("%i;%iH", y + 1, x + 1);
                }
                output_manager->express (&cbuf->glyph_buffer[nx_offset]);
            __advance__:
                dbuf_c >>= 1, mbuf_c >>= 1;
                if (0 == ((++nx_offset) % 64))
                    dbuf_c = fast_brev (*dbuf_pointer++), mbuf_c = fast_brev (*mbuf_pointer++);
            }
        }

        cbuf->dirty_all (false);
    }
}

pewter::engine::Input TTYFrame::wait_for_input ()
{
    return {};
}
