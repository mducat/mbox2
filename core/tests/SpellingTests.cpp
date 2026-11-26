#include <catch2/catch_test_macros.hpp>

#include "mbox/theory/Spelling.h"

using namespace mbox::theory;

namespace
{
    std::string names (const std::vector<SpelledPitchClass>& v)
    {
        std::string s;
        for (const auto& p : v)
            s += (s.empty() ? "" : " ") + p.name();
        return s;
    }

    Key key (const char* tonic, bool minor)
    {
        return Key::fromPitchClass (*PitchClass::fromName (tonic), minor);
    }
} // namespace

TEST_CASE ("Spelled pitch classes and notes")
{
    const SpelledPitchClass cb { Letter::C, -1 };
    CHECK (cb.pitchClass() == PitchClass (11));
    CHECK (cb.name() == "Cb");
    CHECK ((SpelledPitchClass { Letter::F, 2 }).name() == "Fx");
    CHECK ((SpelledPitchClass { Letter::B, -2 }).name() == "Bbb");

    // Octave follows the letter: Cb4 sounds like B3.
    CHECK ((SpelledNote { cb, 4 }).midi() == 59);
    CHECK (SpelledNote::fromMidi (59, cb).octave == 4);
    CHECK (SpelledNote::fromMidi (60, { Letter::B, 1 }).octave == 3);
    CHECK ((SpelledNote { { Letter::E, 0 }, 4 }).diatonicIndex() == 30);
}

TEST_CASE ("Key signatures")
{
    CHECK (key ("C", false).signature().fifths() == 0);
    CHECK (key ("A", true).signature().fifths() == 0);
    CHECK (key ("D", false).signature().fifths() == 2);
    CHECK (key ("F", false).signature().fifths() == -1);
    CHECK (key ("C", true).signature().fifths() == -3);
    CHECK (key ("F#", false).tonic.name() == "Gb");   // tie: flats
    CHECK (key ("C#", true).tonic.name() == "C#");    // 4 sharps beats 8 flats
    CHECK (key ("Ab", false).tonic.name() == "Ab");

    const KeySignature twoSharps (2);
    CHECK (twoSharps.alterationOf (Letter::F) == 1);
    CHECK (twoSharps.alterationOf (Letter::C) == 1);
    CHECK (twoSharps.alterationOf (Letter::G) == 0);
    CHECK (twoSharps.accidentalLetters() == std::vector { Letter::F, Letter::C });

    const KeySignature threeFlats (-3);
    CHECK (threeFlats.alterationOf (Letter::B) == -1);
    CHECK (threeFlats.alterationOf (Letter::A) == -1);
    CHECK (threeFlats.alterationOf (Letter::D) == 0);
    CHECK (threeFlats.accidentalLetters() == std::vector { Letter::B, Letter::E, Letter::A });
}

TEST_CASE ("Chord spelling follows the key and stacks thirds")
{
    const auto C = key ("C", false);
    CHECK (names (spellChord (Chord (PitchClass (7), chords::dominant7), C)) == "G B D F");
    CHECK (names (spellChord (Chord (PitchClass (1), chords::major), C)) == "Db F Ab");     // Neapolitan
    CHECK (names (spellChord (Chord (PitchClass (8), chords::major), C)) == "Ab C Eb");     // bVI
    CHECK (names (spellChord (Chord (PitchClass (2), chords::major), C)) == "D F# A");      // V/V
    CHECK (names (spellChord (Chord (PitchClass (6), chords::diminished7), C)) == "F# A C Eb"); // vii°7/V

    const auto cm = key ("C", true);
    CHECK (names (spellChord (Chord (PitchClass (11), chords::diminished7), cm)) == "B D F Ab");
    CHECK (names (spellChord (Chord (PitchClass (2), chords::halfDiminished7), cm)) == "D F Ab C");

    const auto dm = key ("D", true);
    CHECK (names (spellChord (Chord (PitchClass (9), chords::major), dm)) == "A C# E");
    CHECK (names (spellChord (Chord (PitchClass (1), chords::diminished), dm)) == "C# E G");

    const auto gb = key ("Gb", false);
    CHECK (names (spellChord (Chord (PitchClass (11), chords::major), gb)) == "Cb Eb Gb");  // IV of Gb

    CHECK (names (spellChord (Chord (PitchClass (0), chords::diminished7), C)) == "C Eb Gb Bbb");
    CHECK (names (spellChord (Chord (PitchClass (0), chords::major6), C)) == "C E G A");
    CHECK (names (spellChord (Chord (PitchClass (0), chords::augmented), C)) == "C E G#");
    CHECK (names (spellChord (Chord (PitchClass (0), chords::sus4), C)) == "C F G");
}
