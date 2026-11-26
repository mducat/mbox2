#pragma once

#include "FretboardView.h"
#include "PianoView.h"
#include "SoundSupport.h"
#include "Widget.h"

#include <mbox/instrument/GuitarVoicing.h>

namespace mbox
{

class ManualNoteOutput;

/**
    Piano + fretboard chord viewer.

    - Shows the active notes of the AppState (MIDI input + notes switched on by
      hand) on a keyboard and on a guitar neck, names the chord they form and
      marks every other place where its notes can be played.
    - Click a key or a fret to toggle a note (AppState manual notes, so the
      other widgets see it too). On the neck there is one fret per string, like
      a real guitar; notes added elsewhere are shown at every place they can
      be played.
    - "Voicing" cycles through playable fingerings of the current chord (or of
      the chord selected in the progression when nothing is playing).
    - "MIDI out" switches the playback of manual notes on the plugin's output.
    - Switching a note on sounds it: piano for a key, guitar for a fret (a
      fingering chosen with "<" ">" is strummed). "Son : muet" silences it.

    Everything works in note numbers of the current tuning, so both instruments
    re-lay themselves out for any N-TET (N frets and N keys per octave).
*/
class InstrumentWidget final : public Widget
{
public:
    explicit InstrumentWidget (AppState&);

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;

private:
    enum class ViewMode { both = 1, piano, guitar };

    void appStateChanged (AppState&, int changes) override;

    void togglePitch (int note);
    void togglePosition (instrument::FretPosition pos);
    void stepVoicing (int delta);
    void rebuildForTuning();
    void rebuildFretboard();
    void refresh();
    int maxFretboardHeight() const;

    ManualNoteOutput* findMidiOutput() const;
    juce::String nameOf (int note) const;
    std::vector<int> manualNotes() const;

    // Model (the notes themselves live in AppState)
    int divisions = 0;
    std::vector<int> stringFrets; // fret chosen on each string, -1 = none
    instrument::Fretboard board = instrument::Fretboard::fromPreset (instrument::stringTunings::guitarStandard);
    const instrument::TuningPreset* stringTuning = &instrument::stringTunings::guitarStandard;

    std::vector<instrument::GuitarVoicing> voicings;
    std::pair<std::vector<int>, int> voicingKey { {}, -1 }; // pitch classes + bass the list was built for
    int voicingIndex = -1;

    // Display
    juce::String chordName { "-" };
    juce::String chordDetails;

    juce::ComboBox viewBox, stringTuningBox;
    juce::TextButton prevVoicing { "<" }, nextVoicing { ">" }, clearButton { juce::String::fromUTF8 ("Effacer") };
    juce::Label voicingLabel;
    juce::ToggleButton midiOutButton { "MIDI out" };
    SoundSelector soundBox;
    PianoView piano;
    FretboardView fretboard;
    juce::Rectangle<int> headerArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentWidget)
};

} // namespace mbox
