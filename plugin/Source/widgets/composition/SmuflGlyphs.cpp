#include "SmuflGlyphs.h"

#include "BinaryData.h"

namespace mbox::composition
{

namespace smufl
{
    Glyph accidentalFor (int alter) noexcept
    {
        switch (alter)
        {
            case -2: return accidentalDoubleFlat;
            case -1: return accidentalFlat;
            case 1:  return accidentalSharp;
            case 2:  return accidentalDoubleSharp;
            default: return accidentalNatural;
        }
    }

    float advanceOf (Glyph g) noexcept
    {
        switch (g)
        {
            case brace:                 return 0.28f;
            case gClef:                 return 2.684f;
            case fClef:                 return 2.736f;
            case tabClef6String:        return 1.636f;
            case noteheadDoubleWhole:   return 2.528f;
            case noteheadWhole:         return 1.688f;
            case noteheadHalf:          return 1.18f;
            case noteheadBlack:         return 1.18f;
            case augmentationDot:       return 0.4f;
            case accidentalFlat:        return 0.904f;
            case accidentalNatural:     return 0.672f;
            case accidentalSharp:       return 0.996f;
            case accidentalDoubleSharp: return 1.0f;
            case accidentalDoubleFlat:  return 1.652f;
        }
        return 1.0f;
    }
} // namespace smufl

namespace
{
    constexpr float kReferenceHeight = 400.0f;
}

SmuflFont& SmuflFont::instance()
{
    static SmuflFont font;
    return font;
}

SmuflFont::SmuflFont()
    : typeface (juce::Typeface::createSystemTypefaceFor (BinaryData::Bravura_otf, BinaryData::Bravura_otfSize))
{
    // Calibrate against the whole notehead, which is exactly one staff space tall
    // in SMuFL, rather than relying on how the font size maps to the em square.
    cache.clear();
    unitsPerSpace = 1.0f;
    const auto head = outline (smufl::noteheadWhole).getBounds();
    unitsPerSpace = head.getHeight() > 0.0f ? head.getHeight() : 1.0f;
    cache.clear();
}

const juce::Path& SmuflFont::outline (juce::juce_wchar codepoint)
{
    if (auto it = cache.find (codepoint); it != cache.end())
        return it->second;

    juce::Path p;
    if (typeface != nullptr)
    {
        juce::GlyphArrangement arrangement;
        arrangement.addLineOfText (juce::Font (juce::FontOptions (typeface).withHeight (kReferenceHeight)),
                                   juce::String::charToString (codepoint), 0.0f, 0.0f);
        arrangement.createPath (p);
        p.applyTransform (juce::AffineTransform::scale (1.0f / unitsPerSpace));
    }
    return cache.emplace (codepoint, std::move (p)).first->second;
}

void SmuflFont::draw (juce::Graphics& g, juce::juce_wchar codepoint, float x, float y, float space, float scaleY)
{
    g.fillPath (outline (codepoint), juce::AffineTransform::scale (space, space * scaleY).translated (x, y));
}

} // namespace mbox::composition
