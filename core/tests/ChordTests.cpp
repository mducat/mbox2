#include <catch2/catch_test_macros.hpp>

#include "mbox/theory/Chord.h"

using namespace mbox::theory;

TEST_CASE ("Chord pitch classes and symbols")
{
    const Chord am7 (PitchClass (9), chords::minor7);
    CHECK (am7.pitchClasses() == PitchClassSet ({ 9, 0, 4, 7 }));
    CHECK (am7.symbol() == "Am7");
    CHECK (Chord (PitchClass (10), chords::major).symbol (true) == "Bb");
    CHECK (am7.voicing (57) == std::vector<int> { 57, 60, 64, 67 });
}

TEST_CASE ("identifyChords finds triads in any inversion")
{
    // E G C (first inversion C major)
    const auto found = identifyChords (PitchClassSet::fromMidiNotes ({ 64, 67, 72 }));
    REQUIRE (found.size() == 1);
    CHECK (found[0] == Chord (PitchClass (0), chords::major));

    CHECK (identifyChords (PitchClassSet ({ 2, 5, 9 }))[0].symbol() == "Dm");
    CHECK (identifyChords (PitchClassSet ({ 11, 2, 5 }))[0].symbol() == "Bdim");
}

TEST_CASE ("identifyChords uses the bass to disambiguate")
{
    // C E G A = C6 or Am7
    const PitchClassSet notes { 0, 4, 7, 9 };

    const auto rootC = identifyChords (notes, PitchClass (0));
    REQUIRE (rootC.size() == 2);
    CHECK (rootC[0].symbol() == "C6");

    const auto rootA = identifyChords (notes, PitchClass (9));
    REQUIRE (rootA.size() == 2);
    CHECK (rootA[0].symbol() == "Am7");
}

TEST_CASE ("Symmetric chords give several readings")
{
    CHECK (identifyChords (PitchClassSet ({ 0, 4, 8 })).size() == 3);      // augmented
    CHECK (identifyChords (PitchClassSet ({ 0, 3, 6, 9 })).size() == 4);   // dim7
}

TEST_CASE ("Unknown sets return nothing")
{
    CHECK (identifyChords (PitchClassSet {}).empty());
    CHECK (identifyChords (PitchClassSet ({ 0, 1, 2 })).empty());
}
