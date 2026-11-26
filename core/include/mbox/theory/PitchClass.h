#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mbox::theory
{

/** Number of pitch classes in 12-TET. */
inline constexpr int kNumPitchClasses = 12;

/** Euclidean modulo 12 (always returns 0..11, also for negative input). */
constexpr int mod12 (int v) noexcept
{
    const int r = v % kNumPitchClasses;
    return r < 0 ? r + kNumPitchClasses : r;
}

/**
    A pitch class: a note independent of its octave (C = 0, C#/Db = 1, ... B = 11).
    Arithmetic is always modulo 12.
*/
class PitchClass
{
public:
    constexpr PitchClass() noexcept = default;
    constexpr explicit PitchClass (int value) noexcept : v (static_cast<std::uint8_t> (mod12 (value))) {}

    /** Pitch class of a MIDI note number (60 = middle C -> C). */
    static constexpr PitchClass fromMidi (int midiNote) noexcept { return PitchClass (midiNote); }

    /** Parses names like "C", "F#", "Bb", "e", "Cb", "B#", "Ebb". */
    static std::optional<PitchClass> fromName (std::string_view name);

    constexpr int value() const noexcept { return v; }

    constexpr PitchClass transposed (int semitones) const noexcept { return PitchClass (v + semitones); }

    /** Ascending interval in semitones from this to other (0..11). */
    constexpr int intervalTo (PitchClass other) const noexcept { return mod12 (other.v - v); }

    /** Inversion around C (I0): pc -> -pc. */
    constexpr PitchClass inverted() const noexcept { return PitchClass (-v); }

    /** Name using sharps ("C#") or flats ("Db") for black keys. */
    std::string name (bool preferFlats = false) const;

    friend constexpr bool operator== (PitchClass, PitchClass) noexcept = default;

private:
    std::uint8_t v = 0;
};

/**
    An unordered set of pitch classes stored as a 12-bit mask (bit n = pitch class n).
    This is the basic object for chord/scale geometry: polygons on the
    chromatic circle or the circle of fifths, Tonnetz regions, etc.
*/
class PitchClassSet
{
public:
    constexpr PitchClassSet() noexcept = default;
    constexpr explicit PitchClassSet (std::uint16_t mask) noexcept : bits (mask & 0x0FFFu) {}
    PitchClassSet (std::initializer_list<int> pcs) noexcept;

    static PitchClassSet fromMidiNotes (const std::vector<int>& midiNotes) noexcept;

    constexpr std::uint16_t mask() const noexcept { return bits; }
    constexpr bool empty() const noexcept { return bits == 0; }
    int size() const noexcept;

    constexpr bool contains (PitchClass pc) const noexcept { return (bits >> pc.value()) & 1u; }
    constexpr void insert (PitchClass pc) noexcept { bits = static_cast<std::uint16_t> (bits | (1u << pc.value())); }
    constexpr void erase (PitchClass pc) noexcept { bits = static_cast<std::uint16_t> (bits & ~(1u << pc.value())); }

    /** Rotates the whole set by n semitones (T_n). */
    PitchClassSet transposed (int semitones) const noexcept;

    /** Inversion around C (I_0). */
    PitchClassSet inverted() const noexcept;

    /** Pitch classes in ascending order, starting from C. */
    std::vector<PitchClass> toVector() const;

    /** Pitch classes ordered by their position on the circle of fifths, starting from C. */
    std::vector<PitchClass> toFifthsOrder() const;

    friend constexpr bool operator== (PitchClassSet, PitchClassSet) noexcept = default;

private:
    std::uint16_t bits = 0;
};

/**
    Circle of fifths helpers. Position 0 = C, 1 = G, 2 = D, ... 11 = F.
    Moving by a fifth (7 semitones) advances one step on the circle.
*/
namespace fifths
{
    constexpr int positionOf (PitchClass pc) noexcept { return mod12 (pc.value() * 7); }
    constexpr PitchClass atPosition (int position) noexcept { return PitchClass (position * 7); }

    /** Shortest signed distance on the circle (-6..6). */
    constexpr int distance (PitchClass a, PitchClass b) noexcept
    {
        const int d = mod12 (positionOf (b) - positionOf (a));
        return d > 6 ? d - 12 : d;
    }
} // namespace fifths

} // namespace mbox::theory
