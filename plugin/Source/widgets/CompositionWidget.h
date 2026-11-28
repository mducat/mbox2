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

    The selected chord can be deleted ("Supprimer", Delete / Backspace, or the
    right-click menu) and its duration changed ("Durée", keys 1-4 and 8, or the
    right-click menu): 1 to 4 beats or 2 bars, written with the matching note
    value; bar lines fall every 4 beats.

    The arrows next to "Voicing" cycle the selected chord through alternative
    voicings over the same bass (close, drop 2, drop 3, in several registers),
    write it back to the progression and play it.

    "Play" plays the progression (each chord for its duration at the chosen
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

    void cycleVoicing (int delta);
    void updateVoicingControls();

    void setChordBeats (int index, int beats);
    void deleteChord (int index);
    void showChordMenu (int index);
    void updateEditControls();

    void startPlayback();
    void stopPlayback();
    void timerCallback() override;
    double secondsPerBeat() const;

    juce::Label title, notice;
    juce::ToggleButton voiceLeadingToggle { juce::String::fromUTF8 ("Conduite des voix") };
    juce::ToggleButton tabToggle { "Tablature" };
    juce::ComboBox tuningBox;

    juce::TextButton playButton { "Play" };
    juce::Slider tempoSlider { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    juce::Label tempoLabel;
    juce::ToggleButton loopToggle { "Boucle" };
    SoundSelector soundBox;
    juce::Label voicingLabel;
    juce::Label durationLabel;
    juce::ComboBox durationBox;
    juce::TextButton deleteButton { "Supprimer" };
    juce::TextButton previousVoicing { juce::String::fromUTF8 ("\xe2\x80\xb9") }, nextVoicing { juce::String::fromUTF8 ("\xe2\x80\xba") };
    bool playing = false;
    double playStartMs = 0.0;
    std::vector<double> playEnds; // end of each scheduled chord, in seconds from the start
    int playStartIndex = 0, playingIndex = -1;
    juce::Viewport viewport;
    composition::ScoreComponent score;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompositionWidget)
};

} // namespace mbox
