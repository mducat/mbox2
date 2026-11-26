#pragma once

#include "SoundSupport.h"
#include "Widget.h"
#include "composition/ScoreComponent.h"

namespace mbox
{

/**
    Composition widget: the progression built on the tonal map, written on a
    grand staff (chord symbols above, roman numerals below) with an optional
    guitar tablature. Clicking a bar selects that chord for the other widgets.

    "Play" plays the progression (one bar of 4 beats per chord at the chosen
    tempo) with the piano or guitar of the "Son" selector, in the real tuning
    (N-TET included), following along by selecting the chord being played.
*/
class CompositionWidget final : public Widget,
                                private juce::Timer
{
public:
    explicit CompositionWidget (AppState&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void appStateChanged (AppState&, int changes) override;
    void refreshScore();
    void updateScoreSize();

    void startPlayback();
    void stopPlayback();
    void timerCallback() override;
    double secondsPerChord() const;

    juce::Label title, notice;
    juce::ToggleButton voiceLeadingToggle { juce::String::fromUTF8 ("Conduite des voix") };
    juce::ToggleButton tabToggle { "Tablature" };
    juce::ComboBox tuningBox;

    juce::TextButton playButton { "Play" };
    juce::Slider tempoSlider { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    juce::Label tempoLabel;
    juce::ToggleButton loopToggle { "Boucle" };
    SoundSelector soundBox;
    bool playing = false;
    double playStartMs = 0.0, playChordSeconds = 2.0;
    int playStartIndex = 0, playCount = 0, playingIndex = -1;
    juce::Viewport viewport;
    composition::ScoreComponent score;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompositionWidget)
};

} // namespace mbox
