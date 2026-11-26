#pragma once

#include "mbox/theory/Chord.h"
#include "mbox/theory/PitchClass.h"
#include "mbox/theory/Scale.h"

#include <optional>
#include <string>
#include <vector>

namespace mbox::theory
{

/**
    Functional harmony: keys, Roman-numeral chords and their tonal function
    (tonic / subdominant / dominant). This is the model behind the "tonal map"
    widget: pick a key, get every chord of the key grouped by function.
*/

enum class Mode
{
    Major,
    Minor
};

/** A tonal key: tonic + mode (e.g. A minor). */
struct Key
{
    PitchClass tonic;
    Mode mode = Mode::Major;

    /** Natural scale of the key (major, or natural minor). */
    Scale scale() const noexcept;

    /** Pitch class of a diatonic degree (0 = tonic .. 6), in the natural scale. */
    PitchClass degree (int index) const noexcept { return scale().degree (index); }

    /** True when the key signature uses flats (F major, D minor, Bb major...). */
    bool prefersFlats() const noexcept;

    /** "C major", "F# minor", "Bb major". */
    std::string name() const;

    /** The parallel key (C major <-> C minor). */
    Key parallel() const noexcept { return { tonic, mode == Mode::Major ? Mode::Minor : Mode::Major }; }

    friend bool operator== (const Key&, const Key&) noexcept = default;
};

/** The three tonal functions of the Riemann / Kostka model. */
enum class HarmonicFunction
{
    Tonic,
    Subdominant,
    Dominant
};

/** "Tonic", "Subdominant", "Dominant" (short: "T", "SD", "D"). */
const char* functionName (HarmonicFunction f, bool shortName = false) noexcept;

/** Where a chord comes from, relative to the key. */
enum class ChordOrigin
{
    Diatonic,   // built on the key's own scale (minor: natural + harmonic)
    Secondary,  // applied dominant / leading-tone chord (V/V, vii°7/ii...)
    Borrowed,   // modal mixture from the parallel key (iv, bVI in major; Picardy I in minor)
    Chromatic,  // Neapolitan, augmented sixths
    Cadential   // cadential 6/4
};

/**
    A chord seen from a key: Roman numeral, function and inversion.

    The root is stored as a diatonic degree (0..6) plus a chromatic alteration
    relative to the key's natural scale, which is what Roman numerals encode
    (bVI = degree 5, alteration -1 in a major key).
*/
struct RomanChord
{
    Key key;
    Chord chord;                    // root + quality, root position
    int degree = 0;                 // 0..6, diatonic degree of the root (or of the bass for aug 6ths)
    int alteration = 0;             // -1 = flat, +1 = sharp, relative to the natural scale
    int inversion = 0;              // 0 = root position, 1 = first, 2 = second, 3 = third (7ths)
    HarmonicFunction function = HarmonicFunction::Tonic;
    std::optional<HarmonicFunction> alternateFunction; // e.g. iii: Tonic, also Dominant
    ChordOrigin origin = ChordOrigin::Diatonic;
    std::optional<int> targetDegree; // for secondary chords: degree they resolve to (V/V -> 4)

    /** Special labels that are not "degree + quality" (N, It+6, Fr+6, Ger+6). */
    enum class Special { None, Neapolitan, ItalianSixth, FrenchSixth, GermanSixth };
    Special special = Special::None;

    /** Number of notes in the chord (3 for triads, 4 for sevenths). */
    int size() const noexcept { return static_cast<int> (chord.quality().intervals.size()); }

    /** Lowest note: the root moved by the inversion. */
    PitchClass bass() const noexcept;

    /** Same chord in another inversion (clamped to the available ones). */
    RomanChord withInversion (int newInversion) const noexcept;

    /** Inversion figure: "", "6", "6/4" (triads) / "7", "6/5", "4/3", "4/2" (sevenths). */
    std::string figure() const;

    /** Full Roman numeral: "ii°7", "V6/5/V", "bVI", "i6/4", "N6", "Ger+6". */
    std::string numeral() const;

    /** Chord symbol with slash bass: "G7", "C/E", "Bbmaj7", "F#dim7/C". */
    std::string symbol() const;

    /**
        MIDI notes in close position, bass first. The bass is placed in the octave
        starting at bassOctaveStart (e.g. 48 = C3), the other notes stacked above it.
    */
    std::vector<int> voicing (int bassOctaveStart = 48) const;

    /** Identity comparison (same key, same chord, same inversion, same label). */
    friend bool operator== (const RomanChord& a, const RomanChord& b) noexcept
    {
        return a.key == b.key && a.chord == b.chord && a.inversion == b.inversion
            && a.degree == b.degree && a.alteration == b.alteration
            && a.targetDegree == b.targetDegree && a.special == b.special && a.origin == b.origin;
    }
};

/** Which families of chords to generate for a key. */
struct HarmonyOptions
{
    bool triads = true;
    bool sevenths = true;
    bool secondaryDominants = true;   // V/x, V7/x, vii°7/x
    bool borrowedChords = true;       // modal mixture
    bool chromaticChords = true;      // Neapolitan, augmented sixths
    bool cadentialSixFour = true;
};

/**
    Every chord of the key in root position (except the cadential 6/4 and the
    Neapolitan sixth, which are defined by their inversion), ordered by
    function, then by importance within the function.
*/
std::vector<RomanChord> chordsInKey (const Key& key, const HarmonyOptions& options = {});

/** Only the chords of one function (convenience filter over chordsInKey). */
std::vector<RomanChord> chordsWithFunction (const Key& key, HarmonicFunction function,
                                            const HarmonyOptions& options = {});

namespace chords
{
    /** Augmented-sixth sonorities, rooted on the flat 6th degree (their bass). */
    extern const ChordQuality italianSixth; // b6 1 #4      -> 0 4 10
    extern const ChordQuality frenchSixth;  // b6 1 2 #4    -> 0 4 6 10
    extern const ChordQuality germanSixth;  // b6 1 b3 #4   -> 0 4 7 10
} // namespace chords

} // namespace mbox::theory
