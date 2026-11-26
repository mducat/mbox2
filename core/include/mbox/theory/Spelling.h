#pragma once

#include "mbox/theory/Chord.h"
#include "mbox/theory/PitchClass.h"

#include <string>
#include <vector>

namespace mbox::theory
{

/**
    Note spelling for staff notation: a pitch class is not enough to write a
    note, we also need its letter (C..B) and its alteration (G# vs Ab).

    Alterations are stored in 12-TET semitones (-2 = double flat .. +2 = double
    sharp). Microtonal systems (quarter tones, N-TET) will add a finer
    alteration on top of the letter; everything that places notes on a staff
    only depends on the letter and octave, so it stays valid.
*/

/** Diatonic letters, in staff order. */
enum class Letter : int { C = 0, D, E, F, G, A, B };

inline constexpr int kNumLetters = 7;

/** Pitch class of a natural letter (C = 0, D = 2, E = 4, F = 5, G = 7, A = 9, B = 11). */
constexpr int naturalPitchClass (Letter l) noexcept
{
    constexpr int table[] { 0, 2, 4, 5, 7, 9, 11 };
    return table[static_cast<int> (l)];
}

constexpr Letter letterAt (int index) noexcept
{
    const int r = index % kNumLetters;
    return static_cast<Letter> (r < 0 ? r + kNumLetters : r);
}

/** A spelled pitch class, e.g. F#, Bb, Cb, Ex (E double sharp). */
struct SpelledPitchClass
{
    Letter letter = Letter::C;
    int alter = 0; // semitones

    PitchClass pitchClass() const noexcept { return PitchClass (naturalPitchClass (letter) + alter); }

    /** Position on the line of fifths (C = 0, G = 1, F = -1, F# = 6, Bb = -2...). */
    int lineOfFifths() const noexcept;

    /** "C", "F#", "Bb", "Cbb", "Fx". */
    std::string name() const;

    /** Spells `pc` with the given letter (alteration taken in -6..+5). */
    static SpelledPitchClass withLetter (Letter letter, PitchClass pc) noexcept;

    friend bool operator== (const SpelledPitchClass&, const SpelledPitchClass&) noexcept = default;
};

/** A spelled note with an octave (scientific pitch: C4 = MIDI 60, octave follows the letter so Cb4 = MIDI 59). */
struct SpelledNote
{
    SpelledPitchClass pitch;
    int octave = 4;

    int midi() const noexcept { return (octave + 1) * 12 + naturalPitchClass (pitch.letter) + pitch.alter; }

    /** Diatonic step count from C0 (one per letter): what decides the vertical staff position. */
    int diatonicIndex() const noexcept { return octave * kNumLetters + static_cast<int> (pitch.letter); }

    /** The spelled note sounding as `midiNote`, with the given spelling of its pitch class. */
    static SpelledNote fromMidi (int midiNote, SpelledPitchClass spelling) noexcept;

    friend bool operator== (const SpelledNote&, const SpelledNote&) noexcept = default;
};

/** A key signature as a number of fifths: +n = n sharps, -n = n flats (range -7..7). */
class KeySignature
{
public:
    constexpr KeySignature() noexcept = default;
    constexpr explicit KeySignature (int numFifths) noexcept : fifthsCount (numFifths) {}

    constexpr int fifths() const noexcept { return fifthsCount; }

    /** Alteration the signature applies to a letter (+1, -1 or 0). */
    int alterationOf (Letter letter) const noexcept;

    /** Letters carrying an accidental, in the order they are written (F C G D A E B, or B E A D G C F). */
    std::vector<Letter> accidentalLetters() const;

    friend constexpr bool operator== (KeySignature, KeySignature) noexcept = default;

private:
    int fifthsCount = 0;
};

/** A tonal key: spelled tonic + mode (major / minor). */
struct Key
{
    SpelledPitchClass tonic;
    bool minor = false;

    /** Key with the simplest spelling of `tonic` (fewest accidentals, ties go to flats: Gb major, Eb minor). */
    static Key fromPitchClass (PitchClass tonic, bool minor);

    KeySignature signature() const noexcept;

    /**
        Spells a chord root by its scale degree from the tonic, using the usual
        chromatic degrees (b2, b3, #4, b6, b7): so the Neapolitan in C is Db,
        bVI is Ab, the leading tone in D minor is C# and vii°7 of C minor is B.
    */
    SpelledPitchClass spellDegree (PitchClass pc) const noexcept;
};

/** Number of letters spanned by a chord interval (0 = unison, 2 = third, 4 = fifth, 6 = seventh...). */
int genericInterval (const ChordQuality& quality, int intervalIndex) noexcept;

/** Spells every chord tone (in quality interval order, root first) by stacking letters on the spelled root. */
std::vector<SpelledPitchClass> spellChord (const Chord& chord, const Key& key);

/** Same with an explicitly spelled root. */
std::vector<SpelledPitchClass> spellChord (const Chord& chord, SpelledPitchClass root);

} // namespace mbox::theory
