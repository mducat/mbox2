#pragma once

#include "ScoreModel.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <mbox/instrument/GuitarVoicing.h>
#include <mbox/theory/Theory.h>

#include <functional>
#include <optional>

namespace mbox::composition
{

/**
    Engraves a chord sequence: grand staff (one whole-note chord per bar, with
    chord symbols above and roman numerals below) and an optional guitar
    tablature underneath. The component sizes its own width to the content.
*/
class ScoreComponent final : public juce::Component
{
public:
    ScoreComponent();

    void setInput (const ScoreInput& input);
    void setShowTablature (bool shouldShow);
    void setTuning (const instrument::TuningPreset& preset);

    /** true: re-voice the chords with smooth voice leading; false: write the progression's own voicings. */
    void setVoiceLeading (bool shouldVoiceLead);

    void setSelectedIndex (int index);

    /** Staff space in pixels (distance between two staff lines). */
    void setStaffSpace (float pixels);

    /** Height needed for the current settings, at the current staff space. */
    int preferredHeight() const;
    int preferredWidth() const;

    /** Called when the user clicks a bar (index into the sequence). */
    std::function<void (int)> onChordClicked;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    struct EngravedChord
    {
        std::vector<theory::SpelledPitchClass> tones;
        std::vector<theory::SpelledNote> trebleNotes, bassNotes;
        std::optional<instrument::GuitarVoicing> fingering;
        juce::String symbol;
        juce::String roman;
    };

    struct Metrics
    {
        float sp, trebleTop, bassTop, romanBaseline, tabTop, tabLineGap, height;
        float staffLeft, clefX, keySigX, firstBarX, barWidth, right;
    };

    void rebuild();
    Metrics metrics() const;
    int barAt (juce::Point<float> p) const;

    void drawStaffLines (juce::Graphics&, const Metrics&, float top, int numLines, float gap) const;
    void drawKeySignature (juce::Graphics&, const Metrics&, float staffBottom, int stepOffset) const;
    void drawNotes (juce::Graphics&, const Metrics&, float x, float staffBottom, int bottomLineIndex,
                    const std::vector<theory::SpelledNote>& notes) const;
    void rebuildFretboard();
    void drawTablature (juce::Graphics&, const Metrics&) const;

    ScoreInput input;
    const instrument::TuningPreset* tuningPreset = nullptr;
    bool voiceLeading = false;
    std::vector<EngravedChord> engraved;
    instrument::Fretboard fretboard;
    bool showTab = true;
    float staffSpace = 9.0f;
    int hoveredBar = -1;
    int selectedBar = -1;
};

} // namespace mbox::composition
