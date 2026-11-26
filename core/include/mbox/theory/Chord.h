#pragma once

#include "mbox/theory/PitchClass.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mbox::theory
{

/** A chord quality, defined by its intervals above the root. */
struct ChordQuality
{
    std::string_view name;    // "Minor seventh"
    std::string_view symbol;  // "m7"  (appended to the root name: "Am7")
    std::span<const int> intervals;
};

namespace chords
{
    extern const ChordQuality major;
    extern const ChordQuality minor;
    extern const ChordQuality diminished;
    extern const ChordQuality augmented;
    extern const ChordQuality sus2;
    extern const ChordQuality sus4;
    extern const ChordQuality dominant7;
    extern const ChordQuality major7;
    extern const ChordQuality minor7;
    extern const ChordQuality minorMajor7;
    extern const ChordQuality halfDiminished7;
    extern const ChordQuality diminished7;
    extern const ChordQuality augmented7;
    extern const ChordQuality major6;
    extern const ChordQuality minor6;
    extern const ChordQuality power5;

    /** All built-in chord qualities, in recognition priority order. */
    std::span<const ChordQuality* const> all();
} // namespace chords

/** A concrete chord: root + quality (e.g. A minor seventh). */
class Chord
{
public:
    Chord (PitchClass root, const ChordQuality& quality) noexcept : chordRoot (root), chordQuality (&quality) {}

    PitchClass root() const noexcept { return chordRoot; }
    const ChordQuality& quality() const noexcept { return *chordQuality; }

    PitchClassSet pitchClasses() const noexcept;

    /** MIDI notes of the chord in root position, starting at rootMidiNote's octave. */
    std::vector<int> voicing (int rootMidiNote) const;

    /** Short symbol, e.g. "C", "F#m7", "Bbdim". */
    std::string symbol (bool preferFlats = false) const;

    friend bool operator== (const Chord& a, const Chord& b) noexcept
    {
        return a.chordRoot == b.chordRoot && a.chordQuality == b.chordQuality;
    }

private:
    PitchClass chordRoot;
    const ChordQuality* chordQuality;
};

/**
    Finds every chord whose pitch-class set is exactly `notes`.
    If `bass` is given, the chord rooted on the bass comes first (root-position reading).
    Symmetric chords (aug, dim7) return several candidates.
*/
std::vector<Chord> identifyChords (PitchClassSet notes, std::optional<PitchClass> bass = std::nullopt);

} // namespace mbox::theory
