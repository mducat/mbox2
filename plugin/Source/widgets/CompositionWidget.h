#pragma once

#include "Widget.h"
#include "composition/ScoreComponent.h"

namespace mbox
{

/**
    Composition widget: the progression built on the tonal map, written on a
    grand staff (chord symbols above, roman numerals below) with an optional
    guitar tablature. Clicking a bar selects that chord for the other widgets.
*/
class CompositionWidget final : public Widget
{
public:
    explicit CompositionWidget (AppState&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void appStateChanged (AppState&, int changes) override;
    void refreshScore();
    void updateScoreSize();

    juce::Label title, notice;
    juce::ToggleButton voiceLeadingToggle { juce::String::fromUTF8 ("Conduite des voix") };
    juce::ToggleButton tabToggle { "Tablature" };
    juce::ComboBox tuningBox;
    juce::Viewport viewport;
    composition::ScoreComponent score;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompositionWidget)
};

} // namespace mbox
