#pragma once

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

} // namespace mbox::theory
