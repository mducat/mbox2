#include <catch2/catch_test_macros.hpp>

#include "mbox/theory/Chord.h"

using namespace mbox::theory;

TEST_CASE ("describeChord keeps recognised chords")
{
    CHECK (describeChord (PitchClassSet ({ 0, 4, 7 })) == "C");
    CHECK (describeChord (PitchClassSet ({ 9, 0, 4, 7 }), PitchClass (9)) == "Am7");
    CHECK (describeChord (PitchClassSet ({ 0, 4, 7 }), PitchClass (4)) == "C/E");
}

TEST_CASE ("describeChord names single notes and intervals")
{
    CHECK (describeChord (PitchClassSet ({ 2 })) == "D");
    CHECK (describeChord (PitchClassSet ({ 0, 4 }), PitchClass (0)) == "C(M3)");
    CHECK (describeChord (PitchClassSet ({ 0, 3 }), PitchClass (0)) == "C(m3)");
}

TEST_CASE ("describeChord builds tensions, adds and alterations")
{
    const auto at = [] (std::initializer_list<int> pcs, int bass) { return describeChord (PitchClassSet (pcs), PitchClass (bass)); };

    CHECK (at ({ 0, 4, 7, 2 }, 0) == "C(add9)");
    CHECK (at ({ 0, 4, 7, 10, 2 }, 0) == "C9");
    CHECK (at ({ 0, 4, 7, 11, 2 }, 0) == "Cmaj9");
    CHECK (at ({ 0, 3, 7, 10, 2, 5 }, 0) == "Cm11");
    CHECK (at ({ 0, 4, 7, 10, 2, 9 }, 0) == "C13");
    CHECK (at ({ 0, 4, 7, 10, 3 }, 0) == "C7(#9)");
    CHECK (at ({ 0, 4, 7, 10, 1 }, 0) == "C7(b9)");
    CHECK (at ({ 0, 4, 7, 11, 6 }, 0) == "Cmaj7(#11)");
    CHECK (at ({ 0, 4, 7, 10, 3, 8 }, 0) == "C7(#9,b13)");
    CHECK (at ({ 0, 5, 7, 10 }, 0) == "C7sus4");
    CHECK (at ({ 0, 4, 6 }, 0) == "C(b5)");
    CHECK (at ({ 0, 4, 7, 10, 1 }, 4) == "C7(b9)/E");
    CHECK (at ({ 0, 3, 6, 10, 2 }, 0) == "Cm9b5");
}

TEST_CASE ("describeChord labels microtonal steps with cents")
{
    // 24-TET: C, a neutral third (7 steps = 350 c), G
    CHECK (describeChord (PitchClassSet (24, { 0, 7, 14 }), PitchClass (0, 24)) == "C(m3+50)");

    // 31-TET: C, E+26 (11 steps), G (18 steps)
    CHECK (describeChord (PitchClassSet (31, { 0, 11, 18 }), PitchClass (0, 31)) == "C(M3+26)");
}
