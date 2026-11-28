#pragma once

#include <juce_graphics/juce_graphics.h>

#include <map>

namespace mbox::composition
{

/** SMuFL code points used by the composition view (https://w3c.github.io/smufl/). */
namespace smufl
{
    enum Glyph : juce::juce_wchar
    {
        brace                 = 0xE000,
        gClef                 = 0xE050,
        fClef                 = 0xE062,
        tabClef6String        = 0xE06D,
        noteheadDoubleWhole   = 0xE0A0,
        noteheadWhole         = 0xE0A2,
        noteheadHalf          = 0xE0A3,
        noteheadBlack         = 0xE0A4,
        augmentationDot       = 0xE1E7,
        accidentalFlat        = 0xE260,
        accidentalNatural     = 0xE261,
        accidentalSharp       = 0xE262,
        accidentalDoubleSharp = 0xE263,
        accidentalDoubleFlat  = 0xE264,
    };

    /**
        Accidental glyph for an alteration in 12-TET semitones (0 = natural).
        Microtonal alterations (quarter tones, HEJI, N-TET arrows) map to the
        SMuFL ranges U+E280 (Stein-Zimmermann) and U+E2C0 (Helmholtz-Ellis) here.
    */
    Glyph accidentalFor (int alter) noexcept;

    /** Advance width of a glyph, in staff spaces (from Bravura's metadata). */
    float advanceOf (Glyph g) noexcept;
} // namespace smufl

/**
    Bravura glyph outlines, scaled so that 1 unit = 1 staff space, with the
    origin on the glyph's SMuFL baseline (y grows downwards, as in JUCE).
*/
class SmuflFont
{
public:
    static SmuflFont& instance();

    /** Outline of a glyph in staff-space units. */
    const juce::Path& outline (juce::juce_wchar codepoint);

    /** Fills a glyph whose baseline origin is at (x, y), with a staff space of `space` pixels. */
    void draw (juce::Graphics& g, juce::juce_wchar codepoint, float x, float y, float space, float scaleY = 1.0f);

private:
    SmuflFont();

    juce::Typeface::Ptr typeface;
    float unitsPerSpace = 1.0f;
    std::map<juce::juce_wchar, juce::Path> cache;
};

} // namespace mbox::composition
