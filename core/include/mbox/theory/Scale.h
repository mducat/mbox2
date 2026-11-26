#pragma once

#include "mbox/theory/PitchClass.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mbox::theory
{

/**
    A scale type, defined by its intervals from the tonic, in steps of a
    `divisions`-step octave (semitones for the built-in 12-TET types).
    A type can be used in any division count: intervals are mapped to the
    closest steps (stepsIn). A microtonal type (e.g. a 24-TET maqam) just sets
    divisions = 24.
*/
struct ScaleType
{
    std::string_view name;
    std::span<const int> intervals; // ascending, starts with 0
    int divisions = kDefaultDivisions;

    /** The intervals realised in an N-division octave. */
    std::vector<int> stepsIn (int targetDivisions) const;
};

namespace scales
{
    extern const ScaleType major;          // Ionian
    extern const ScaleType naturalMinor;   // Aeolian
    extern const ScaleType harmonicMinor;
    extern const ScaleType melodicMinor;   // ascending
    extern const ScaleType dorian;
    extern const ScaleType phrygian;
    extern const ScaleType lydian;
    extern const ScaleType mixolydian;
    extern const ScaleType locrian;
    extern const ScaleType majorPentatonic;
    extern const ScaleType minorPentatonic;
    extern const ScaleType blues;
    extern const ScaleType chromatic;

    /** All built-in scale types, for UI lists. */
    std::span<const ScaleType* const> all();
} // namespace scales

/**
    A concrete scale: a tonic plus a scale type (e.g. D Dorian).
    The scale lives in the tonic's division count: Scale (PitchClass (0, 19), scales::major)
    is C major in 19-TET.
*/
class Scale
{
public:
    Scale (PitchClass tonic, const ScaleType& type) noexcept : root (tonic), scaleType (&type) {}

    PitchClass tonic() const noexcept { return root; }
    const ScaleType& type() const noexcept { return *scaleType; }

    int divisions() const noexcept { return root.divisions(); }
    int numDegrees() const noexcept { return static_cast<int> (scaleType->intervals.size()); }

    /** Intervals of the scale from the tonic, in steps of divisions(). */
    std::vector<int> steps() const { return scaleType->stepsIn (divisions()); }

    /** Pitch class of a degree (0-based). Wraps around: degree 7 of a heptatonic scale = tonic. */
    PitchClass degree (int index) const noexcept;

    PitchClassSet pitchClasses() const noexcept;

    bool contains (PitchClass pc) const noexcept { return pitchClasses().contains (pc); }

    /** Snaps a note number (MIDI in 12-TET) to the nearest scale note (ties resolve downwards). */
    int quantize (int note) const noexcept;

    std::string name (bool preferFlats = false) const;

private:
    PitchClass root;
    const ScaleType* scaleType;
};

} // namespace mbox::theory
