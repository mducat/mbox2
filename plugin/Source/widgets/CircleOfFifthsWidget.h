#pragma once

#include "Widget.h"

namespace mbox
{

/**
    Circle of fifths of the current tuning: highlights the active pitch classes,
    joins them into a polygon, marks the key and shows the recognised chord.
    Click a node to toggle that pitch class (as a manual note around middle C).
*/
class CircleOfFifthsWidget final : public Widget
{
public:
    explicit CircleOfFifthsWidget (AppState&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    struct Geometry
    {
        juce::Point<float> centre;
        float radius = 0.0f, nodeSize = 0.0f;
        int positions = 12;
        juce::Point<float> pointAt (int position) const;
    };

    Geometry geometry() const;
    std::vector<theory::PitchClass> nodes() const; // pitch classes drawn, in circle order

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CircleOfFifthsWidget)
};

} // namespace mbox
