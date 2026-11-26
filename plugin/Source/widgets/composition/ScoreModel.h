#pragma once

#include <mbox/theory/Theory.h>

#include <optional>
#include <string>
#include <vector>

namespace mbox::composition
{

/** One chord to engrave (built by CompositionWidget from the shared progression). */
struct ScoreChord
{
    // Staff notation is written in 12-TET: in other tunings the chord is
    // approximated (see ScoreInput::approximated).
    theory::Chord chord;
    std::vector<int> notes;   // voicing as 12-TET MIDI notes, ascending (empty = let the voicer choose)
    int inversion = 0;        // chord tone in the bass: 0 = root, 1 = third...
    std::string roman;        // "V7/V", "ii6"... (may be empty)
    std::string symbol;       // overrides the computed chord symbol when not empty

    // Tablature, in the *current* tuning (frets follow its steps).
    std::vector<int> tabClasses;
    std::optional<int> tabBass;
};

struct ScoreInput
{
    theory::Key key = theory::Key::fromPitchClass (theory::PitchClass (0), false);
    std::vector<ScoreChord> chords;
    int divisions = 12;        // steps per octave of the current tuning (frets of the tablature)
    bool approximated = false; // notation rounded to 12-TET
};

} // namespace mbox::composition
