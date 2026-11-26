#include <catch2/catch_test_macros.hpp>

#include "mbox/theory/PitchClass.h"

using namespace mbox::theory;

TEST_CASE ("mod12 handles negatives")
{
    CHECK (mod12 (-1) == 11);
    CHECK (mod12 (-12) == 0);
    CHECK (mod12 (25) == 1);
}

TEST_CASE ("PitchClass from MIDI and arithmetic")
{
    CHECK (PitchClass::fromMidi (60).value() == 0);  // middle C
    CHECK (PitchClass::fromMidi (69).value() == 9);  // A4
    CHECK (PitchClass (11).transposed (2).value() == 1);
    CHECK (PitchClass (0).intervalTo (PitchClass (7)) == 7);
    CHECK (PitchClass (7).intervalTo (PitchClass (0)) == 5);
    CHECK (PitchClass (4).inverted().value() == 8);
}

TEST_CASE ("PitchClass names round-trip")
{
    CHECK (PitchClass (1).name() == "C#");
    CHECK (PitchClass (1).name (true) == "Db");
    CHECK (PitchClass::fromName ("Bb")->value() == 10);
    CHECK (PitchClass::fromName ("f#")->value() == 6);
    CHECK (PitchClass::fromName ("Cb")->value() == 11);
    CHECK (PitchClass::fromName ("B#")->value() == 0);
    CHECK_FALSE (PitchClass::fromName ("H").has_value());
    CHECK_FALSE (PitchClass::fromName ("").has_value());

    for (int i = 0; i < 12; ++i)
    {
        CHECK (PitchClass::fromName (PitchClass (i).name())->value() == i);
        CHECK (PitchClass::fromName (PitchClass (i).name (true))->value() == i);
    }
}

TEST_CASE ("PitchClassSet basics")
{
    const PitchClassSet cMajor { 0, 4, 7 };
    CHECK (cMajor.size() == 3);
    CHECK (cMajor.contains (PitchClass (4)));
    CHECK_FALSE (cMajor.contains (PitchClass (5)));

    CHECK (PitchClassSet::fromMidiNotes ({ 48, 64, 67, 72 }) == cMajor);

    // T7: C major -> G major (G B D)
    CHECK (cMajor.transposed (7) == PitchClassSet ({ 7, 11, 2 }));
    CHECK (cMajor.transposed (-5) == cMajor.transposed (7));

    // I0 of C major (0,4,7) -> (0,8,5) = F minor
    CHECK (cMajor.inverted() == PitchClassSet ({ 0, 5, 8 }));
}

TEST_CASE ("Circle of fifths")
{
    CHECK (fifths::positionOf (PitchClass (0)) == 0);   // C
    CHECK (fifths::positionOf (PitchClass (7)) == 1);   // G
    CHECK (fifths::positionOf (PitchClass (5)) == 11);  // F
    CHECK (fifths::atPosition (2).value() == 2);        // D
    CHECK (fifths::distance (PitchClass (0), PitchClass (7)) == 1);
    CHECK (fifths::distance (PitchClass (0), PitchClass (5)) == -1);
    CHECK (fifths::distance (PitchClass (0), PitchClass (6)) == 6);

    // C major triad in fifths order: C G E -> positions 0, 1, 4
    const auto order = PitchClassSet ({ 0, 4, 7 }).toFifthsOrder();
    REQUIRE (order.size() == 3);
    CHECK (order[0].value() == 0);
    CHECK (order[1].value() == 7);
    CHECK (order[2].value() == 4);
}
