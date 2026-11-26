#pragma once

#include "mbox/instrument/Fretboard.h"

#include <optional>
#include <vector>

namespace mbox::instrument
{

/** One fingering: a fret per string (lowest string first), -1 = muted. */
struct GuitarVoicing
{
    std::vector<int> frets;

    /** Sounding pitches, lowest string first (muted strings skipped). */
    std::vector<int> pitches (const Fretboard& board) const;

    /** Lowest fretted (non-open) fret, or 0 if everything is open/muted. */
    int lowestFret() const noexcept;
    int highestFret() const noexcept;
    int numSounding() const noexcept;

    friend bool operator== (const GuitarVoicing&, const GuitarVoicing&) = default;
};

struct VoicingOptions
{
    int maxSpan = 4;          // fretted span, counted in 12-TET frets (rescaled for N-TET)
    int maxFret = 12;         // highest starting position, in 12-TET frets
    int maxFingers = 4;       // a barre on the lowest fret counts as one finger
    int minSounding = 3;      // at least this many strings (capped by the string count)
    bool allowOpenStrings = true;
    bool contiguous = true;   // sounding strings form one block (no muted string in the middle)
    std::size_t maxResults = 48;
};

/**
    Finds playable fingerings of a chord on a fretted instrument.

    pitchClasses: the chord's pitch classes (mod board.stepsPerOctave()); each one must sound.
    bassClass:    if given, the lowest sounding pitch must have this class (root position or slash chord).
    Results are ranked: low positions and open shapes first, then more strings, then fewer fingers.
*/
std::vector<GuitarVoicing> findVoicings (const Fretboard& board,
                                         const std::vector<int>& pitchClasses,
                                         std::optional<int> bassClass = std::nullopt,
                                         const VoicingOptions& options = {});

} // namespace mbox::instrument
