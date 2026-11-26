#include "PluginEditor.h"

namespace theory = mbox::theory;

namespace
{
    struct HeldNotes
    {
        theory::PitchClassSet pitchClasses;
        std::optional<theory::PitchClass> bass;
    };

    HeldNotes decode (const std::array<std::uint64_t, 2>& mask)
    {
        HeldNotes h;
        for (int n = 0; n < 128; ++n)
        {
            if ((mask[static_cast<std::size_t> (n / 64)] >> (n % 64)) & 1u)
            {
                const auto pc = theory::PitchClass::fromMidi (n);
                if (! h.bass)
                    h.bass = pc;
                h.pitchClasses.insert (pc);
            }
        }
        return h;
    }
} // namespace

MusicBox2Editor::MusicBox2Editor (MusicBox2Processor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (480, 520);
    startTimerHz (30);
}

void MusicBox2Editor::timerCallback()
{
    const auto held = processor.getHeldNotes();
    if (held != lastHeld)
    {
        lastHeld = held;
        repaint();
    }
}

void MusicBox2Editor::paint (juce::Graphics& g)
{
    const auto background = juce::Colour (0xff15171c);
    const auto accent     = juce::Colour (0xff4fc3f7);
    const auto dim        = juce::Colour (0xff3a3f4b);

    g.fillAll (background);

    auto area = getLocalBounds().reduced (20);

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("Music Box 2", area.removeFromTop (30), juce::Justification::centredLeft);

    g.setColour (juce::Colours::grey);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("Circle of fifths - play some notes", area.removeFromTop (20), juce::Justification::centredLeft);

    const auto held = decode (lastHeld);

    // Chord name
    auto chordArea = area.removeFromBottom (50);
    juce::String chordText = "-";
    if (! held.pitchClasses.empty())
    {
        const auto candidates = theory::identifyChords (held.pitchClasses, held.bass);
        chordText = candidates.empty() ? juce::String ("?") : juce::String (candidates.front().symbol());
    }
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (32.0f, juce::Font::bold));
    g.drawText (chordText, chordArea, juce::Justification::centred);

    // Circle of fifths
    const auto side   = static_cast<float> (juce::jmin (area.getWidth(), area.getHeight()));
    const auto centre = area.getCentre().toFloat();
    const auto radius = side * 0.5f - 24.0f;

    auto pointAt = [&] (int position, float r)
    {
        const auto angle = juce::MathConstants<float>::twoPi * static_cast<float> (position) / 12.0f;
        return centre + juce::Point<float> (std::sin (angle), -std::cos (angle)) * r;
    };

    g.setColour (dim);
    g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre), 1.5f);

    // Polygon joining the held pitch classes in fifths order
    const auto ordered = held.pitchClasses.toFifthsOrder();
    if (ordered.size() >= 2)
    {
        juce::Path poly;
        for (std::size_t i = 0; i < ordered.size(); ++i)
        {
            const auto pt = pointAt (theory::fifths::positionOf (ordered[i]), radius);
            if (i == 0) poly.startNewSubPath (pt);
            else        poly.lineTo (pt);
        }
        poly.closeSubPath();

        g.setColour (accent.withAlpha (0.25f));
        g.fillPath (poly);
        g.setColour (accent);
        g.strokePath (poly, juce::PathStrokeType (2.0f));
    }

    // Nodes + labels
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    for (int pos = 0; pos < 12; ++pos)
    {
        const auto pc     = theory::fifths::atPosition (pos);
        const bool active = held.pitchClasses.contains (pc);
        const auto node   = juce::Rectangle<float> (34.0f, 34.0f).withCentre (pointAt (pos, radius));

        g.setColour (active ? accent : background);
        g.fillEllipse (node);
        g.setColour (active ? accent : dim);
        g.drawEllipse (node, 1.5f);

        g.setColour (active ? background : juce::Colours::lightgrey);
        g.drawText (pc.name (pos > 6), node, juce::Justification::centred);
    }
}
