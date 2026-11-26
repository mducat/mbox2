#pragma once

#include "mbox/theory/PitchClass.h"

#include <span>
#include <string>
#include <string_view>

namespace mbox::theory
{

/** A scale type, defined by its intervals from the tonic (in semitones). */
struct ScaleType
{
    std::string_view name;
    std::span<const int> intervals; // ascending, starts with 0
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

/** A concrete scale: a tonic plus a scale type (e.g. D Dorian). */
class Scale
{
public:
    Scale (PitchClass tonic, const ScaleType& type) noexcept : root (tonic), scaleType (&type) {}

    PitchClass tonic() const noexcept { return root; }
    const ScaleType& type() const noexcept { return *scaleType; }

    int numDegrees() const noexcept { return static_cast<int> (scaleType->intervals.size()); }

    /** Pitch class of a degree (0-based). Wraps around: degree 7 of a heptatonic scale = tonic. */
    PitchClass degree (int index) const noexcept;

    PitchClassSet pitchClasses() const noexcept;

    bool contains (PitchClass pc) const noexcept { return pitchClasses().contains (pc); }

    /** Snaps a MIDI note to the nearest scale note (ties resolve downwards). */
    int quantize (int midiNote) const noexcept;

    std::string name (bool preferFlats = false) const;

private:
    PitchClass root;
    const ScaleType* scaleType;
};

} // namespace mbox::theory
