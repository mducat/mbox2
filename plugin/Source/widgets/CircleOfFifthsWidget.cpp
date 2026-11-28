#include "CircleOfFifthsWidget.h"

namespace mbox
{

CircleOfFifthsWidget::CircleOfFifthsWidget (AppState& s) : Widget (s) {}

juce::Point<float> CircleOfFifthsWidget::Geometry::pointAt (int position) const
{
    const auto angle = juce::MathConstants<float>::twoPi * static_cast<float> (position) / static_cast<float> (positions);
    return centre + juce::Point<float> (std::sin (angle), -std::cos (angle)) * radius;
}

std::vector<theory::PitchClass> CircleOfFifthsWidget::nodes() const
{
    // The chain of fifths through C. In tunings where the fifth does not reach
    // every step (24-TET...), the other steps are not drawn here.
    const int n = state.getDivisions();
    std::vector<theory::PitchClass> out;
    for (int pos = 0, count = theory::fifths::size (n); pos < count; ++pos)
        out.push_back (theory::fifths::atPosition (pos, n));
    return out;
}

CircleOfFifthsWidget::Geometry CircleOfFifthsWidget::geometry() const
{
    auto area = getLocalBounds().reduced (20).withTrimmedBottom (50).toFloat();
    Geometry g;
    g.positions = juce::jmax (1, theory::fifths::size (state.getDivisions()));
    g.nodeSize = juce::jlimit (14.0f, 36.0f, 400.0f / static_cast<float> (g.positions));
    g.centre = area.getCentre();
    g.radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - g.nodeSize;
    return g;
}

void CircleOfFifthsWidget::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    const auto geo = geometry();
    const auto active = state.getActivePitchClasses();
    const auto keySet = state.getKey().pitchClasses();
    const auto circle = nodes();

    g.setColour (colours::dim);
    g.drawEllipse (juce::Rectangle<float> (geo.radius * 2.0f, geo.radius * 2.0f).withCentre (geo.centre), 1.5f);

    // Polygon joining the active pitch classes in fifths order
    std::vector<int> activePositions;
    for (const auto pc : active.toFifthsOrder())
        if (const int pos = theory::fifths::positionOf (pc); pos >= 0)
            activePositions.push_back (pos);

    if (activePositions.size() >= 2)
    {
        juce::Path poly;
        for (std::size_t i = 0; i < activePositions.size(); ++i)
        {
            const auto pt = geo.pointAt (activePositions[i]);
            if (i == 0) poly.startNewSubPath (pt);
            else        poly.lineTo (pt);
        }
        poly.closeSubPath();

        g.setColour (colours::accent.withAlpha (0.25f));
        g.fillPath (poly);
        g.setColour (colours::accent);
        g.strokePath (poly, juce::PathStrokeType (2.0f));
    }

    // Nodes + labels
    g.setFont (juce::FontOptions (juce::jmin (15.0f, geo.nodeSize * 0.42f), juce::Font::bold));
    for (int pos = 0; pos < static_cast<int> (circle.size()); ++pos)
    {
        const auto pc = circle[static_cast<std::size_t> (pos)];
        const bool on = active.contains (pc);
        const bool inKey = keySet.contains (pc);
        const auto node = juce::Rectangle<float> (geo.nodeSize, geo.nodeSize).withCentre (geo.pointAt (pos));

        g.setColour (on ? colours::accent : colours::background);
        g.fillEllipse (node);
        g.setColour (on ? colours::accent : (inKey ? colours::secondary : colours::dim));
        g.drawEllipse (node, inKey ? 2.0f : 1.0f);

        g.setColour (on ? colours::background : (inKey ? colours::text : colours::textDim));
        g.drawFittedText (pc.name (pos > static_cast<int> (circle.size()) / 2), node.toNearestInt(),
                          juce::Justification::centred, 1, 0.6f);
    }

    // Recognised chord
    const auto chords = state.getRecognisedChords();
    juce::String chordText = "-";
    if (! active.empty())
        chordText = chords.empty() ? juce::String::fromUTF8 (state.describeActiveChord().c_str()) : juce::String (chords.front().symbol());

    g.setColour (colours::text);
    g.setFont (juce::FontOptions (32.0f, juce::Font::bold));
    g.drawText (chordText, getLocalBounds().removeFromBottom (60), juce::Justification::centred);
}

void CircleOfFifthsWidget::mouseDown (const juce::MouseEvent& e)
{
    const auto geo = geometry();
    const auto circle = nodes();
    for (int pos = 0; pos < static_cast<int> (circle.size()); ++pos)
    {
        if (geo.pointAt (pos).getDistanceFrom (e.position) <= geo.nodeSize * 0.5f)
        {
            const auto& tuning = state.getTuning();
            state.toggleManualNote (tuning.noteOf (circle[static_cast<std::size_t> (pos)], 4));
            return;
        }
    }
}

} // namespace mbox
