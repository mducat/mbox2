#pragma once

#include <array>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mbox::theory
{

/**
    Pitch space model
    -----------------
    Every pitch-class type in this library lives in a cyclic space of N
    "steps" per period (octave). N is the number of *divisions*: 12 for the
    usual chromatic scale (the default everywhere), 19, 24, 31... for
    microtonal systems.

    This header is only about *discrete* steps (which note, modulo the period).
    How a step sounds (cents, Hz, equal or not) is the job of Tuning (Tuning.h).
    A non-equal 12-note tuning (just intonation, meantone...) therefore still
    uses divisions = 12 here.
*/

/** Default number of divisions per octave (12-TET). */
inline constexpr int kDefaultDivisions = 12;

/** Largest supported number of divisions per octave (size of PitchClassSet). */
inline constexpr int kMaxDivisions = 128;

/** Kept for compatibility: number of pitch classes in 12-TET. */
inline constexpr int kNumPitchClasses = kDefaultDivisions;

/** Euclidean modulo (always returns 0..n-1, also for negative input). */
constexpr int modN (int v, int n) noexcept
{
    const int r = v % n;
    return r < 0 ? r + n : r;
}

/** Euclidean modulo 12. */
constexpr int mod12 (int v) noexcept { return modN (v, kDefaultDivisions); }

/** Clamps a division count into the supported range [1, kMaxDivisions]. */
constexpr int clampDivisions (int n) noexcept { return n < 1 ? 1 : (n > kMaxDivisions ? kMaxDivisions : n); }

/** Equal-division helpers (interval mapping between step systems). */
namespace edo
{
    /** Number of steps of N-EDO closest to an interval of `cents`. */
    int stepsFromCents (double cents, int divisions) noexcept;

    /**
        Maps an interval expressed in `fromDivisions` steps to the closest
        interval in `toDivisions` steps (by size in cents, ties go down).
        This is how 12-TET chord/scale definitions are realised in 19-, 24-,
        31-TET... e.g. a major third (4 semitones) becomes 6 steps of 19-TET.
    */
    int convert (int steps, int fromDivisions, int toDivisions) noexcept;

    /** Shorthand for convert (semitones, 12, divisions). */
    inline int fromSemitones (int semitones, int divisions) noexcept { return convert (semitones, kDefaultDivisions, divisions); }

    /** Steps of the best approximation of the perfect fifth (3/2) in N-EDO: 7 in 12, 11 in 19, 18 in 31. */
    int fifth (int divisions) noexcept;
} // namespace edo

/**
    A pitch class: a note independent of its octave, as a step 0..N-1 of an
    N-division octave. In the default 12-division space: C = 0, C#/Db = 1 ... B = 11.
    Arithmetic is always modulo N.
*/
class PitchClass
{
public:
    constexpr PitchClass() noexcept = default;

    /** Step `value` (wrapped modulo `divisions`) of an octave divided in `divisions` steps. */
    constexpr explicit PitchClass (int value, int divisions = kDefaultDivisions) noexcept
        : n (static_cast<std::uint8_t> (clampDivisions (divisions) - 1)),
          v (static_cast<std::uint8_t> (modN (value, clampDivisions (divisions))))
    {
    }

    /** Pitch class of a MIDI note number in 12-TET (60 = middle C -> C). */
    static constexpr PitchClass fromMidi (int midiNote) noexcept { return PitchClass (midiNote); }

    /** Pitch class of an absolute note number in an N-division space (note 0 = step 0). */
    static constexpr PitchClass fromNote (int note, int divisions) noexcept { return PitchClass (note, divisions); }

    /**
        Parses names like "C", "F#", "Bb", "e", "Cb", "B#", "Ebb".
        With divisions != 12 the 12-TET pitch is mapped to the closest step
        (so "E" is step 6 of 19-TET, step 10 of 31-TET).
    */
    static std::optional<PitchClass> fromName (std::string_view name, int divisions = kDefaultDivisions);

    constexpr int value() const noexcept { return v; }
    constexpr int divisions() const noexcept { return n + 1; }

    constexpr PitchClass transposed (int steps) const noexcept { return PitchClass (v + steps, divisions()); }

    /** Ascending interval in steps from this to other (0..N-1). Both must share the same divisions. */
    constexpr int intervalTo (PitchClass other) const noexcept { return modN (other.v - v, divisions()); }

    /** Inversion around step 0 (I0): pc -> -pc. */
    constexpr PitchClass inverted() const noexcept { return PitchClass (-v, divisions()); }

    /** Same pitch, re-expressed in another division count (closest step). */
    PitchClass convertedTo (int newDivisions) const noexcept;

    /** Size of this pitch class above step 0 in cents, assuming an equal division of the octave. */
    double equalCents() const noexcept { return 1200.0 * v / divisions(); }

    /**
        Name using sharps ("C#") or flats ("Db") for black keys.
        In other division counts, steps that coincide with a 12-TET note get
        its name, others get the nearest name plus a cents offset ("D-37", "C+50").
    */
    std::string name (bool preferFlats = false) const;

    friend constexpr bool operator== (PitchClass, PitchClass) noexcept = default;

private:
    std::uint8_t n = kDefaultDivisions - 1; // divisions - 1, so 128 fits in a byte
    std::uint8_t v = 0;
};

/**
    An unordered set of pitch classes of one N-division space, stored as a bit
    mask (bit k = step k). This is the basic object for chord/scale geometry:
    polygons on the chromatic circle or the circle of fifths, Tonnetz regions, etc.
    Inserting a pitch class from another division count is a precondition
    violation (it is converted to the closest step).
*/
class PitchClassSet
{
public:
    /** Empty 12-TET set. */
    constexpr PitchClassSet() noexcept = default;

    /** 12-TET set from a 12-bit mask (bit n = pitch class n). */
    constexpr explicit PitchClassSet (std::uint16_t mask) noexcept : words { mask & 0x0FFFu, 0 } {}

    /** 12-TET set from pitch class numbers. */
    PitchClassSet (std::initializer_list<int> pcs) noexcept;

    /** N-division set from step numbers, e.g. PitchClassSet (19, { 0, 6, 11 }). */
    PitchClassSet (int divisions, std::initializer_list<int> steps) noexcept;

    /** Empty set of an N-division space. */
    static PitchClassSet withDivisions (int divisions) noexcept;

    /** 12-TET set of the pitch classes of MIDI notes. */
    static PitchClassSet fromMidiNotes (const std::vector<int>& midiNotes) noexcept;

    /** N-division set of the pitch classes of absolute note numbers (note 0 = step 0). */
    static PitchClassSet fromNotes (const std::vector<int>& notes, int divisions) noexcept;

    constexpr int divisions() const noexcept { return n + 1; }

    /** Low 16 bits of the mask: the whole set in 12-TET. Prefer contains()/toVector() in generic code. */
    constexpr std::uint16_t mask() const noexcept { return static_cast<std::uint16_t> (words[0] & 0xFFFFu); }

    constexpr bool empty() const noexcept { return words[0] == 0 && words[1] == 0; }
    int size() const noexcept;

    bool contains (PitchClass pc) const noexcept;
    void insert (PitchClass pc) noexcept;
    void erase (PitchClass pc) noexcept;
    void toggle (PitchClass pc) noexcept;

    /** Rotates the whole set by n steps (T_n). */
    PitchClassSet transposed (int steps) const noexcept;

    /** Inversion around step 0 (I_0). */
    PitchClassSet inverted() const noexcept;

    /** Pitch classes in ascending order, starting from step 0. */
    std::vector<PitchClass> toVector() const;

    /**
        Pitch classes ordered by their position on the circle of fifths, starting
        from step 0. When the fifth does not generate every step (e.g. 24-TET),
        pitch classes off the C chain follow in ascending order.
    */
    std::vector<PitchClass> toFifthsOrder() const;

    friend constexpr bool operator== (const PitchClassSet&, const PitchClassSet&) noexcept = default;

private:
    int index (PitchClass pc) const noexcept;

    std::array<std::uint64_t, 2> words {};
    std::uint8_t n = kDefaultDivisions - 1;
};

/**
    Circle of fifths helpers, generalised to N divisions: one step on the circle
    is the division's best fifth (edo::fifth). In 12-TET: position 0 = C,
    1 = G, 2 = D, ... 11 = F.
    When the fifth does not generate all steps (gcd (fifth, N) > 1, e.g. 24-TET)
    positionOf() returns -1 for pitch classes off the chain through step 0.
*/
namespace fifths
{
    /** Number of positions on the circle (= N when the fifth generates every step). */
    int size (int divisions = kDefaultDivisions) noexcept;

    int positionOf (PitchClass pc) noexcept;
    PitchClass atPosition (int position, int divisions = kDefaultDivisions) noexcept;

    /** Shortest signed distance on the circle (-size/2..size/2), 0 if either note is off the chain. */
    int distance (PitchClass a, PitchClass b) noexcept;
} // namespace fifths

} // namespace mbox::theory
