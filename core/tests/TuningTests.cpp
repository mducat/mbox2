#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "mbox/theory/Tuning.h"

using namespace mbox::theory;
using Catch::Approx;

TEST_CASE ("Default tuning is 12-TET with MIDI note numbers")
{
    const Tuning t;
    CHECK (t.isTwelveTet());
    CHECK (t.divisions() == 12);
    CHECK (t.middleC() == 60);
    CHECK (t.frequencyOf (69) == Approx (440.0));
    CHECK (t.frequencyOf (60) == Approx (261.6256).epsilon (1e-6));
    CHECK (t.frequencyOf (81) == Approx (880.0));
    CHECK (t.octaveOf (60) == 4);
    CHECK (t.octaveOf (59) == 3);
    CHECK (t.octaveOf (0) == -1);
    CHECK (t.pitchClassOf (64) == PitchClass (4));
    CHECK (t.noteOf (PitchClass (9), 4) == 69);
    CHECK (t.noteName (60) == "C4");
    CHECK (t.noteName (61, true) == "Db4");
    CHECK (t.nearestNote (440.0) == 69);
    CHECK (t.stepsFromRatio (1.5) == 7);
    CHECK (t.stepsFromRatio (1.25) == 4);
    CHECK (t.convertNote (64, Tuning {}) == 64);
}

TEST_CASE ("Equal tunings other than 12")
{
    const auto t19 = Tuning::equal (19);
    CHECK (t19.name() == "19-TET");
    CHECK (t19.isEqual());
    CHECK_FALSE (t19.isTwelveTet());
    CHECK (t19.middleC() == 95);
    CHECK (t19.frequencyOf (95) == Approx (261.6256).epsilon (1e-6));
    CHECK (t19.frequencyOf (95 + 19) == Approx (2 * 261.6256).epsilon (1e-6));
    CHECK (t19.intervalCents (6) == Approx (1200.0 * 6 / 19));
    CHECK (t19.stepsFromRatio (1.5) == 11);
    CHECK (t19.stepsFromSemitones (4) == 6);

    // A 12-TET E4 played on a MIDI keyboard lands on 19-TET E4 (6 steps above C4)
    CHECK (t19.convertNote (64, Tuning {}) == 101);
    CHECK (t19.noteName (101) == "E-21 4");

    double offset = 0.0;
    CHECK (t19.nearestNote (440.0, &offset) == 95 + 14);
    CHECK (offset == Approx (900.0 - 1200.0 * 14 / 19));

    const auto t31 = Tuning::equal (31);
    CHECK (t31.stepsFromRatio (5.0 / 4.0) == 10);
    CHECK (t31.stepsFromRatio (3.0 / 2.0) == 18);
}

TEST_CASE ("Non-equal tuning from a cents table")
{
    // 5-limit just intonation on C
    const auto just = Tuning::fromCents ("Just C", { 0.0, 111.73, 203.91, 315.64, 386.31, 498.04,
                                                     590.22, 701.96, 813.69, 884.36, 1017.6, 1088.27 });
    CHECK_FALSE (just.isEqual());
    CHECK (just.divisions() == 12);
    CHECK (just.frequencyOf (67) == Approx (just.frequencyOf (60) * 1.5).epsilon (1e-4));
    CHECK (just.frequencyOf (72) == Approx (just.frequencyOf (60) * 2.0));
    CHECK (just.stepsFromRatio (1.25) == 4);
    CHECK (just.stepsFromRatio (3.0) == 19);
    CHECK (just.nearestNote (just.frequencyOf (52)) == 52);
}

TEST_CASE ("Concert pitch and presets")
{
    CHECK (Tuning {}.withConcertA (432.0).frequencyOf (69) == Approx (432.0));
    const auto presets = tunings::presets();
    REQUIRE_FALSE (presets.empty());
    CHECK (presets.front().isTwelveTet());
}
