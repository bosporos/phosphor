//
// project pewter
// author Maximilien M. Cura
//

#include <Pewter/Glyph>

using namespace pewter;

Glyph::Glyph ()
    : inner { default_glyphset ().glyph_default.inner }
    , properties { default_glyphset ().glyph_default.properties }
    , coloration { default_glyphset ().glyph_default.coloration }
{}

Glyph::Glyph (Glyph const & _glyph)
    : inner { _glyph.inner }
    , properties { _glyph.properties }
    , coloration { _glyph.coloration }
{}

Glyph::Glyph (GlyphInternal _gi, GlyphProperties _gp)
    : inner { _gi }
    , properties { _gp }
    , coloration { default_glyphset ().glyph_default.coloration }
{}

Glyph::Glyph (GlyphInternal _gi, Pair _cp, GlyphProperties _gp)
    : inner { _gi }
    , properties { _gp }
    , coloration { _cp }
{}

void Glyph::set (GlyphInternal _gi)
{
    inner = _gi;
}

void Glyph::set (GlyphInternal _gi, GlyphProperties _gp)
{
    inner      = _gi;
    properties = _gp;
}

void Glyph::set (GlyphInternal _gi, Pair _cp)
{
    inner      = _gi;
    coloration = _cp;
}

void Glyph::set (GlyphInternal _gi, Pair _cp, GlyphProperties _gp)
{
    inner      = _gi;
    coloration = _cp;
    properties = _gp;
}

void Glyph::set (Pair _cp, GlyphProperties _gp)
{
    coloration = _cp;
    properties = _gp;
}

Glyph & Glyph::operator= (Glyph const & rhs)
{
    inner      = rhs.inner;
    properties = rhs.properties;
    coloration = rhs.coloration;
    return *this;
}

GlyphSet::GlyphSet (Glyph _gd, Pair _cd)
    : glyph_default { _gd }
    , coloration_default { _cd }
{}

GlyphSet::GlyphSet (GlyphSet const & _gs)
    : glyph_default { _gs.glyph_default }
    , coloration_default { _gs.coloration_default }
{}

GlyphSet & GlyphSet::operator= (GlyphSet const & rhs)
{
    glyph_default      = rhs.glyph_default;
    coloration_default = rhs.coloration_default;

    return *this;
}

GlyphSet pewter::_glyphset_default (0, Pair (0, 0));

GlyphSet & pewter::default_glyphset ()
{
    return pewter::_glyphset_default;
}

void pewter::set_default_glyphset (GlyphSet _gs)
{
    pewter::_glyphset_default = _gs;
}
