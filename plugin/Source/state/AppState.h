#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>

#include <mbox/theory/Theory.h>

#include <string>
#include <vector>

namespace mbox
{

/**
    One chord of the user's progression: written by the tonal map, read by the
    notation / tablature view, highlighted by the instrument view...
*/
struct ProgressionChord
{
    theory::Chord chord;
    std::vector<int> notes;   // voicing as note numbers of the current tuning (= MIDI notes in 12-TET), ascending
    std::string degree;       // roman numeral label in the current key: "V7/V", "ii°7", "i6/4" (may be empty)
    std::string symbol;       // chord symbol: "D7", "Bdim7/Ab"... (chord.symbol() if left empty)

    friend bool operator== (const ProgressionChord& a, const ProgressionChord& b)
    {
        return a.chord == b.chord && a.notes == b.notes && a.degree == b.degree && a.symbol == b.symbol;
    }
};

/**
    State shared by every widget of the editor. It lives in the processor (so
    it survives closing the editor and is saved with the project) and is only
    touched from the message thread.

    Four pieces:
      - tuning:       current Tuning (12-TET by default). Every note number and
                      pitch class below is expressed in it.
      - key:          current key as a theory::Scale (tonic + scale type).
      - progression:  ordered list of ProgressionChord + a selected index.
      - active notes: notes currently sounding = notes held on the MIDI input
                      (set by the editor) + notes toggled by hand in widgets.

    Widgets listen with AppState::Listener; `changes` is a mask of Change bits
    so a widget can ignore what it does not display.
*/
class AppState
{
public:
    enum Change
    {
        tuningChanged      = 1 << 0,
        keyChanged         = 1 << 1,
        progressionChanged = 1 << 2,  // chords added / removed / edited
        selectionChanged   = 1 << 3,  // selected progression index
        activeNotesChanged = 1 << 4,  // held or manual notes
        allChanged         = 0xff
    };

    struct Listener
    {
        virtual ~Listener() = default;
        virtual void appStateChanged (AppState& state, int changes) = 0;
    };

    AppState();

    void addListener (Listener* l)    { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

    // -- Tuning --------------------------------------------------------------

    const theory::Tuning& getTuning() const noexcept { return tuning; }
    int getDivisions() const noexcept { return tuning.divisions(); }

    /**
        Changes the tuning. The key, the progression and the active notes are
        converted to the closest pitches of the new tuning.
    */
    void setTuning (theory::Tuning newTuning);

    // -- Key -----------------------------------------------------------------

    const theory::Scale& getKey() const noexcept { return key; }

    /** Sets the key; the tonic is converted to the current tuning if needed. */
    void setKey (theory::PitchClass tonic, const theory::ScaleType& type);
    void setKey (const theory::Scale& newKey) { setKey (newKey.tonic(), newKey.type()); }

    // -- Progression ---------------------------------------------------------

    const std::vector<ProgressionChord>& getProgression() const noexcept { return progression; }

    /** Adds a chord at the end and returns its index. Empty notes -> root-position voicing around middle C. */
    int appendChord (ProgressionChord chord);
    void insertChord (int index, ProgressionChord chord);
    void replaceChord (int index, ProgressionChord chord);
    void removeChord (int index);
    void moveChord (int fromIndex, int toIndex);
    void clearProgression();

    /** Index of the selected chord, -1 if none. */
    int getSelectedChordIndex() const noexcept { return selectedIndex; }
    void setSelectedChordIndex (int index);
    const ProgressionChord* getSelectedChord() const noexcept;

    // -- Active notes --------------------------------------------------------

    /** Notes held on the MIDI input, already converted to the current tuning. */
    const std::vector<int>& getHeldNotes() const noexcept { return heldNotes; }

    /** Called by the editor with the raw 12-TET MIDI notes held on the input. */
    void setHeldMidiNotes (const std::vector<int>& midiNotes);

    /** Notes toggled by hand in a widget (click on a key, a fret...). */
    const std::vector<int>& getManualNotes() const noexcept { return manualNotes; }
    void setManualNotes (std::vector<int> notes);
    void toggleManualNote (int note);
    void clearManualNotes();

    /** Held + manual notes, ascending, without duplicates. */
    std::vector<int> getActiveNotes() const;
    theory::PitchClassSet getActivePitchClasses() const;

    /** Readings of the active notes, the one rooted on the lowest note first (empty if none). */
    std::vector<theory::Chord> getRecognisedChords() const;

    /** Best-effort name of the active notes, even when no chord is recognised (empty if none). */
    std::string describeActiveChord() const;

    // -- Persistence ---------------------------------------------------------

    juce::ValueTree toValueTree() const;
    void restoreFromValueTree (const juce::ValueTree& tree);

private:
    void notify (int changes);
    ProgressionChord normalised (ProgressionChord c) const;

    theory::Tuning tuning;
    theory::Scale key;
    std::vector<ProgressionChord> progression;
    int selectedIndex = -1;
    std::vector<int> heldNotes, manualNotes;

    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AppState)
};

} // namespace mbox
