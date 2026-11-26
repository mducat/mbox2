#pragma once

#include "../state/AppState.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace mbox
{

/**
    Horizontal strip showing the chords of the shared progression, in order.
    Click a chord to select it, right-click to remove it; "Effacer" empties
    the progression. Call refresh() when the progression or selection changes.
*/
class ProgressionStrip final : public juce::Component,
                               public juce::SettableTooltipClient
{
public:
    explicit ProgressionStrip (AppState&);

    /** Optional: colour of a chord chip (e.g. by tonal function). */
    std::function<juce::Colour (const ProgressionChord&)> chipColour;

    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    juce::Rectangle<int> chipBounds (int index) const;
    int chipAt (juce::Point<int>) const;

    AppState& state;
    juce::TextButton clearButton;
    juce::Rectangle<int> chipsArea;
    int hovered = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgressionStrip)
};

} // namespace mbox
