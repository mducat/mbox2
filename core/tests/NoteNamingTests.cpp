#include <catch2/catch_test_macros.hpp>

#include "mbox/theory/Theory.h"

using namespace mbox::theory;

namespace
{
    const std::string re = "R\xC3\xA9";
}

TEST_CASE ("English naming is the default")
{
    CHECK (noteNaming() == NoteNaming::English);
    CHECK (PitchClass (1).name() == "C#");
}

TEST_CASE ("Solfege pitch class and spelled names")
{
    ScopedNoteNaming solfege (NoteNaming::Solfege);

    CHECK (PitchClass (0).name() == "Do");
    CHECK (PitchClass (1).name() == "Do#");
    CHECK (PitchClass (1).name (true) == re + "b");
    CHECK (PitchClass (7).name() == "Sol");
    CHECK (PitchClass (10).name (true) == "Sib");
    CHECK (PitchClass (2, 24).name() == "Do#");

    CHECK (SpelledPitchClass { Letter::F, 2 }.name() == "Fax");
    CHECK (SpelledPitchClass { Letter::B, -1 }.name() == "Sib");

    CHECK (Tuning::equal (12).noteName (60) == "Do4");
    CHECK (Tuning::equal (12).noteName (69) == "La4");
}

TEST_CASE ("Solfege chord symbols")
{
    ScopedNoteNaming solfege (NoteNaming::Solfege);

    CHECK (Chord (PitchClass (0), chords::dominant7).symbol() == "Do7");
    CHECK (Chord (PitchClass (6), chords::minor7).symbol() == "Fa#m7");
    CHECK (describeChord (PitchClassSet { 4, 7, 10, 0 }, PitchClass (4)) == "Do7/Mi");
}

TEST_CASE ("Converting stored labels between namings")
{
    CHECK (convertNoteNames ("Bbm7/Ab", NoteNaming::English, NoteNaming::Solfege) == "Sibm7/Lab");
    CHECK (convertNoteNames ("C7(b9)/E", NoteNaming::English, NoteNaming::Solfege) == "Do7(b9)/Mi");
    CHECK (convertNoteNames ("Do7(b9)/Mi", NoteNaming::Solfege, NoteNaming::English) == "C7(b9)/E");
    CHECK (convertNoteNames (re + "dim", NoteNaming::Solfege, NoteNaming::English) == "Ddim");
    CHECK (convertNoteNames ("Solm", NoteNaming::Solfege, NoteNaming::English) == "Gm");
    CHECK (convertNoteNames ("Gm", NoteNaming::English, NoteNaming::English) == "Gm");
    CHECK (convertNoteNames ("", NoteNaming::English, NoteNaming::Solfege).empty());

    CHECK (letterIndexOf ("Sol#") == 4);
    CHECK (letterIndexOf ("Bb") == 6);
    CHECK (letterIndexOf ("x") == -1);
}
