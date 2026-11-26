#pragma once

#include "mbox/theory/Chord.h"
#include "mbox/theory/Spelling.h"

#include <vector>

namespace mbox::theory
{

/**
    A chord written on a grand staff: one bass note (bass clef) and the upper
    voices in close position (treble clef).
*/
struct GrandStaffVoicing
{
    SpelledNote bass;
    std::vector<SpelledNote> upper; // ascending

    std::vector<int> midiNotes() const;
};

/** Ranges used by KeyboardVoicer, as MIDI notes (inclusive). */
struct KeyboardVoicingRanges
{
    int bassLow = 43;    // G2: bottom line of the bass staff
    int bassHigh = 57;   // A3
    int upperLow = 60;   // C4: lowest note of the upper voices (one ledger line)
    int upperHigh = 71;  // B4: highest allowed *lowest* upper note
    int upperCentre = 67; // where the first chord is centred
};

/**
    Voices a chord progression for keyboard / grand staff with simple voice
    leading: the bass takes the requested inversion as close as possible to the
    previous bass, and the upper voices pick the close-position rotation that
    moves the least from the previous chord.
*/
class KeyboardVoicer
{
public:
    explicit KeyboardVoicer (KeyboardVoicingRanges r = {}) : ranges (r) {}

    /**
        `tones` are the spelled chord tones, root first (as from spellChord()).
        `inversion` picks the bass: 0 = root, 1 = third, 2 = fifth, 3 = seventh.
    */
    GrandStaffVoicing next (const std::vector<SpelledPitchClass>& tones, int inversion);

    void reset() noexcept { previous = {}; hasPrevious = false; }

private:
    KeyboardVoicingRanges ranges;
    GrandStaffVoicing previous;
    bool hasPrevious = false;
};

/**
    Alternative keyboard voicings of a chord over a fixed bass note, for the
    user to cycle through. Notes are note numbers of an N-division tuning
    (octave * N + step; MIDI notes in 12-TET), so this works in any N-TET.

    The bass stays (it carries the inversion: i6/4 stays i6/4); the upper
    voices take every chord tone (the bass tone is doubled) in:
      - close position, each rotation (which tone on top),
      - drop 2 and drop 3 (open positions), for each rotation,
    in every register where the lowest upper note lies between G3 and C5 and
    the top note stays below C6 (scaled to N). Results are sorted from low to
    high (by average pitch) without duplicates.
*/
std::vector<std::vector<int>> keyboardVoicings (const Chord& chord, int bassNote);

/** Index in `voicings` of the voicing closest to `notes` (same notes first, else least total movement), or -1 if empty. */
int closestVoicingIndex (const std::vector<std::vector<int>>& voicings, const std::vector<int>& notes);

} // namespace mbox::theory
