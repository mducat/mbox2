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
    Engraves a chord sequence: grand staff (one chord per slot, written with
    the note value of its duration, chord symbols above and roman numerals
    below) and an optional guitar tablature underneath. Bar lines fall every
    4 beats (4/4). The component sizes its own width to the content.
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

    /** Called on a right click on a chord (instead of onChordClicked), to show its context menu. */
    std::function<void (int)> onChordMenuRequested;

    /** Called when Delete / Backspace is pressed while a chord is selected. */
    std::function<void (int)> onDeleteRequested;

    /** Called when 1-4 or 8 is pressed while a chord is selected: new duration in beats. */
    std::function<void (int index, int beats)> onBeatsRequested;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    struct EngravedChord
    {
        std::vector<theory::SpelledPitchClass> tones;
        std::vector<theory::SpelledNote> trebleNotes, bassNotes;
        std::optional<instrument::GuitarVoicing> fingering;
        juce::String symbol;
        juce::String roman;
        int beats = 4;
        float x = 0.0f, width = 0.0f; // slot, in staff spaces from the first bar
        bool barLineAfter = false;    // a bar line closes this slot (every 4 beats, and at the end)
    };

    struct Metrics
    {
        float sp, trebleTop, bassTop, romanBaseline, tabTop, tabLineGap, height;
        float staffLeft, clefX, keySigX, firstBarX, right;

        float slotX (const EngravedChord& e) const noexcept     { return firstBarX + e.x * sp; }
        float slotWidth (const EngravedChord& e) const noexcept { return e.width * sp; }
    };

    void rebuild();
    Metrics metrics() const;
    int barAt (juce::Point<float> p) const;

    void drawStaffLines (juce::Graphics&, const Metrics&, float top, int numLines, float gap) const;
    void drawKeySignature (juce::Graphics&, const Metrics&, float staffBottom, int stepOffset) const;
    void drawNotes (juce::Graphics&, const Metrics&, float x, float staffBottom, int bottomLineIndex,
                    const std::vector<theory::SpelledNote>& notes, int beats) const;
    void rebuildFretboard();
    void drawTablature (juce::Graphics&, const Metrics&) const;

    ScoreInput input;
    const instrument::TuningPreset* tuningPreset = nullptr;
    bool voiceLeading = false;
    std::vector<EngravedChord> engraved;
    float contentWidth = 0.0f; // sum of the slot widths, in staff spaces
    instrument::Fretboard fretboard;
    bool showTab = true;
    float staffSpace = 9.0f;
    int hoveredBar = -1;
    int selectedBar = -1;
};

} // namespace mbox::composition
