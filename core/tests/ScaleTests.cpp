#include <catch2/catch_test_macros.hpp>

#include "mbox/theory/Scale.h"

using namespace mbox::theory;

TEST_CASE ("Scale pitch classes")
{
    const Scale cMajor (PitchClass (0), scales::major);
    CHECK (cMajor.pitchClasses() == PitchClassSet ({ 0, 2, 4, 5, 7, 9, 11 }));
    CHECK (cMajor.name() == "C Major");

    // A natural minor is the relative minor of C major
    const Scale aMinor (PitchClass (9), scales::naturalMinor);
    CHECK (aMinor.pitchClasses() == cMajor.pitchClasses());

    // D Dorian too
    CHECK (Scale (PitchClass (2), scales::dorian).pitchClasses() == cMajor.pitchClasses());
}

TEST_CASE ("Scale degrees wrap")
{
    const Scale gMajor (PitchClass (7), scales::major);
    CHECK (gMajor.degree (0).value() == 7);
    CHECK (gMajor.degree (6).value() == 6);   // F#
    CHECK (gMajor.degree (7).value() == 7);   // wraps to tonic
    CHECK (gMajor.degree (-1).value() == 6);
}

TEST_CASE ("Scale quantize snaps to nearest note")
{
    const Scale cMajor (PitchClass (0), scales::major);
    CHECK (cMajor.quantize (60) == 60);  // C stays
    CHECK (cMajor.quantize (61) == 60);  // C# -> C (tie resolves down)
    CHECK (cMajor.quantize (66) == 65);  // F# -> F

    const Scale cPenta (PitchClass (0), scales::majorPentatonic);
    CHECK (cPenta.quantize (65) == 64);  // F -> E
    CHECK (cPenta.quantize (71) == 72);  // B -> C
}

TEST_CASE ("All scale types are well-formed")
{
    for (const auto* t : scales::all())
    {
        REQUIRE_FALSE (t->intervals.empty());
        CHECK (t->intervals.front() == 0);
        for (std::size_t i = 1; i < t->intervals.size(); ++i)
            CHECK (t->intervals[i] > t->intervals[i - 1]);
        CHECK (t->intervals.back() < 12);
    }
}
