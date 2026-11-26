#pragma once

#include "mbox/instrument/Keyboard.h"

#include <span>
#include <string_view>
#include <vector>

namespace mbox::instrument
{

/** A named string tuning, written as 12-TET MIDI notes from the lowest string to the highest. */
struct TuningPreset
{
    std::string_view name;
    std::span<const int> openNotes;
};

namespace stringTunings
{
    extern const TuningPreset guitarStandard; // E A D G B E
    extern const TuningPreset guitarDropD;    // D A D G B E
    extern const TuningPreset guitarDADGAD;
    extern const TuningPreset guitarOpenG;    // D G D G B D
    extern const TuningPreset guitarOpenD;    // D A D F# A D
    extern const TuningPreset guitar7String;  // B E A D G B E
    extern const TuningPreset bass4;          // E A D G
    extern const TuningPreset ukulele;        // G C E A (re-entrant)

    std::span<const TuningPreset* const> presets();
} // namespace stringTunings

struct FretPosition
{
    int string = 0; // 0 = lowest string
    int fret = 0;   // 0 = open string

    friend bool operator== (FretPosition, FretPosition) noexcept = default;
};

/**
    A fretted instrument in any equal temperament: one fret per step, so an
    N-TET neck has N frets per octave.
*/
class Fretboard
{
public:
    /** openPitches: one pitch per string (lowest first), already in N-TET steps. */
    Fretboard (std::vector<int> openPitches, int numFrets, int stepsPerOctave = 12);

    /** Builds a neck from a 12-TET preset, rescaling the open strings and the fret count to N-TET. */
    static Fretboard fromPreset (const TuningPreset& preset, int numFrets12 = 22, int stepsPerOctave = 12);

    int numStrings() const noexcept { return static_cast<int> (open.size()); }
    int numFrets() const noexcept { return frets; }
    int stepsPerOctave() const noexcept { return steps; }
    const std::vector<int>& openPitches() const noexcept { return open; }

    int pitchAt (int string, int fret) const noexcept { return open[static_cast<std::size_t> (string)] + fret; }
    int pitchAt (FretPosition p) const noexcept { return pitchAt (p.string, p.fret); }

    /** Every place where this exact pitch can be played. */
    std::vector<FretPosition> positionsOf (int pitch) const;

    /** Every place where this pitch class (0..N-1) can be played. */
    std::vector<FretPosition> positionsOfClass (int pitchClass) const;

    /** Distance of a fret from the nut, as a fraction of the scale length (1 - 2^(-fret/N)). */
    double fretDistance (int fret) const noexcept;

    /** Frets that usually carry an inlay dot (3 5 7 9 12...), rescaled to N-TET. */
    std::vector<int> inlayFrets() const;

private:
    std::vector<int> open;
    int frets = 22;
    int steps = 12;
};

} // namespace mbox::instrument
