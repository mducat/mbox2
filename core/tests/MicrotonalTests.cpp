#include <catch2/catch_test_macros.hpp>

#include "mbox/theory/Theory.h"

using namespace mbox::theory;

TEST_CASE ("Interval mapping between division counts")
{
    CHECK (edo::fifth (12) == 7);
    CHECK (edo::fifth (19) == 11);
    CHECK (edo::fifth (24) == 14);
    CHECK (edo::fifth (31) == 18);
    CHECK (edo::fifth (53) == 31);

    CHECK (edo::fromSemitones (4, 12) == 4);
    CHECK (edo::fromSemitones (4, 19) == 6);
    CHECK (edo::fromSemitones (3, 19) == 5);
    CHECK (edo::fromSemitones (1, 19) == 2);  // diatonic semitone
    CHECK (edo::fromSemitones (4, 24) == 8);  // exact
    CHECK (edo::fromSemitones (4, 31) == 10);
    CHECK (edo::fromSemitones (7, 20) == 12);
    CHECK (edo::convert (8, 24, 12) == 4);
    CHECK (edo::convert (1, 24, 12) == 0);    // quarter tone: tie goes down
}

TEST_CASE ("PitchClass in N divisions")
{
    const PitchClass e19 (6, 19);
    CHECK (e19.divisions() == 19);
    CHECK (e19.value() == 6);
    CHECK (PitchClass (25, 19).value() == 6);
    CHECK (PitchClass (-1, 19).value() == 18);
    CHECK (e19.transposed (13).value() == 0);
    CHECK (e19.intervalTo (PitchClass (0, 19)) == 13);
    CHECK (e19 != PitchClass (6));                 // different spaces
    CHECK (PitchClass (128, 128).divisions() == 128);
    CHECK (PitchClass (127, 128).value() == 127);

    CHECK (PitchClass (0, 19).name() == "C");
    CHECK (e19.name() == "E-21");
    CHECK (PitchClass (1, 24).name() == "C+50");
    CHECK (PitchClass (2, 24).name() == "C#");
    CHECK (PitchClass (2, 24).name (true) == "Db");

    CHECK (PitchClass::fromName ("E", 19)->value() == 6);
    CHECK (PitchClass::fromName ("E", 31)->value() == 10);
    CHECK (PitchClass::fromName ("G", 24)->value() == 14);
    CHECK (PitchClass (4).convertedTo (19) == e19);
}

TEST_CASE ("PitchClassSet in N divisions")
{
    PitchClassSet s (31, { 0, 10, 18, 30 });
    CHECK (s.divisions() == 31);
    CHECK (s.size() == 4);
    CHECK (s.contains (PitchClass (30, 31)));
    CHECK (s.transposed (1) == PitchClassSet (31, { 1, 11, 19, 0 }));
    CHECK (s.inverted() == PitchClassSet (31, { 0, 21, 13, 1 }));
    s.toggle (PitchClass (30, 31));
    CHECK_FALSE (s.contains (PitchClass (30, 31)));

    // Sets above 64 divisions use the second word
    PitchClassSet big (72, { 0, 70, 71 });
    CHECK (big.size() == 3);
    CHECK (big.transposed (2) == PitchClassSet (72, { 2, 0, 1 }));
    CHECK (PitchClassSet::fromNotes ({ 0, 72, 143 }, 72) == PitchClassSet (72, { 0, 71 }));

    CHECK (PitchClassSet::withDivisions (19) != PitchClassSet {}); // empty sets of different spaces
}

TEST_CASE ("Circle of fifths in N divisions")
{
    CHECK (fifths::size (19) == 19);
    CHECK (fifths::atPosition (1, 19).value() == 11);
    CHECK (fifths::positionOf (PitchClass (11, 19)) == 1);
    CHECK (fifths::distance (PitchClass (0, 19), PitchClass (8, 19)) == -1); // F = one fifth down

    // 24-TET: the fifth (14 steps) only reaches the 12 "ordinary" notes
    CHECK (fifths::size (24) == 12);
    CHECK (fifths::positionOf (PitchClass (14, 24)) == 1);
    CHECK (fifths::positionOf (PitchClass (1, 24)) == -1);
    const auto order = PitchClassSet (24, { 0, 1, 8, 14 }).toFifthsOrder();
    REQUIRE (order.size() == 4);
    CHECK (order[0].value() == 0);
    CHECK (order[1].value() == 14);
    CHECK (order[2].value() == 8);
    CHECK (order[3].value() == 1); // off-chain notes come last
}

TEST_CASE ("Scales in 19-TET and 31-TET")
{
    const Scale c19 (PitchClass (0, 19), scales::major);
    CHECK (c19.divisions() == 19);
    CHECK (c19.steps() == std::vector<int> { 0, 3, 6, 8, 11, 14, 17 });
    CHECK (c19.pitchClasses() == PitchClassSet (19, { 0, 3, 6, 8, 11, 14, 17 }));
    CHECK (c19.degree (4).value() == 11);
    CHECK (c19.degree (7).value() == 0);
    CHECK (c19.quantize (95 + 5) == 95 + 6);
    CHECK (c19.name() == "C Major");

    const Scale a31 (*PitchClass::fromName ("A", 31), scales::naturalMinor);
    CHECK (a31.tonic().value() == 23);
    CHECK (a31.contains (PitchClass (0, 31))); // C is in A minor
}

TEST_CASE ("Chords in N-TET")
{
    const Chord c19 (PitchClass (0, 19), chords::major);
    CHECK (c19.steps() == std::vector<int> { 0, 6, 11 });
    CHECK (c19.voicing (95) == std::vector<int> { 95, 101, 106 });
    CHECK (c19.symbol() == "C");

    const auto found = identifyChords (PitchClassSet (19, { 6, 11, 0 }));
    REQUIRE_FALSE (found.empty());
    CHECK (found[0] == c19);

    const auto am7 = identifyChords (PitchClassSet (24, { 18, 0, 8, 14 }), PitchClass (18, 24));
    REQUIRE_FALSE (am7.empty());
    CHECK (am7[0].symbol() == "Am7");
}

TEST_CASE ("Microtonal chord qualities")
{
    // A neutral triad, native to 24-TET (root, neutral third, fifth)
    static constexpr int kNeutral[] { 0, 7, 14 };
    const ChordQuality neutral { "Neutral", "n", kNeutral, 24 };

    const Chord dNeutral (PitchClass (4, 24), neutral);
    CHECK (dNeutral.pitchClasses() == PitchClassSet (24, { 4, 11, 18 }));
    CHECK (dNeutral.symbol() == "Dn");

    // ...realised in other division counts
    CHECK (neutral.stepsIn (24) == std::vector<int> { 0, 7, 14 });
    CHECK (neutral.stepsIn (31) == std::vector<int> { 0, 9, 18 });
}
