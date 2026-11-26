#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "mbox/instrument/GuitarVoicing.h"

#include <algorithm>

using namespace mbox::instrument;

TEST_CASE ("12-TET keyboard layout is a piano")
{
    const auto kb = KeyboardLayout::forEqualTemperament (12);
    CHECK (kb.lowerKeysPerOctave() == 7);

    for (const int black : { 1, 3, 6, 8, 10 })
        CHECK (kb.key (black).upper);
    for (const int white : { 0, 2, 4, 5, 7, 9, 11 })
        CHECK_FALSE (kb.key (white).upper);

    CHECK (kb.key (5).x == 3.0);                    // F is the 4th white key
    CHECK_THAT (kb.key (1).x + kb.key (1).width / 2, Catch::Matchers::WithinAbs (1.0, 1e-9)); // C# straddles C|D
    CHECK (kb.keyForPitch (60).x == 35.0);          // middle C = 5 octaves * 7 white keys
    CHECK (kb.pitchAt (35.5, 0.9, 0.6, 48, 72) == 60);
    CHECK (kb.pitchAt (36.0, 0.3, 0.6, 48, 72) == 61);
    CHECK (kb.pitchAt (36.0, 0.9, 0.6, 48, 72) == 62);
}

TEST_CASE ("Keyboard layout adapts to other equal temperaments")
{
    const auto kb24 = KeyboardLayout::forEqualTemperament (24);
    CHECK (kb24.lowerKeysPerOctave() == 7);
    CHECK_FALSE (kb24.key (4).upper); // D
    CHECK (kb24.key (1).upper);
    CHECK (kb24.key (2).upper);
    CHECK (kb24.key (9).upper);       // quarter tone between E (8) and F (10)

    const auto kb19 = KeyboardLayout::forEqualTemperament (19);
    int upper = 0;
    for (int s = 0; s < 19; ++s)
        upper += kb19.key (s).upper ? 1 : 0;
    CHECK (upper == 19 - kb19.lowerKeysPerOctave());

    // Raised keys never overlap each other.
    for (int n : { 12, 19, 22, 24, 31 })
    {
        const auto kb = KeyboardLayout::forEqualTemperament (n);
        for (int a = 0; a < n; ++a)
            for (int b = a + 1; b < n; ++b)
                if (kb.key (a).upper && kb.key (b).upper)
                {
                    const auto& ka = kb.key (a);
                    const auto& kb2 = kb.key (b);
                    CHECK ((ka.x + ka.width <= kb2.x + 1e-9 || kb2.x + kb2.width <= ka.x + 1e-9));
                }
    }
}

TEST_CASE ("Fretboard pitches and positions")
{
    const auto board = Fretboard::fromPreset (stringTunings::guitarStandard);
    CHECK (board.numStrings() == 6);
    CHECK (board.numFrets() == 22);
    CHECK (board.pitchAt (0, 5) == 45);     // A on the low E string
    CHECK (board.pitchAt (5, 0) == 64);

    const auto middleC = board.positionsOf (60);
    CHECK (std::find (middleC.begin(), middleC.end(), FretPosition { 4, 1 }) != middleC.end());
    CHECK (middleC.size() == 5);            // every string but the high E

    CHECK_THAT (board.fretDistance (12), Catch::Matchers::WithinAbs (0.5, 1e-9));
    CHECK (board.inlayFrets().front() == 3);

    const auto board24 = Fretboard::fromPreset (stringTunings::guitarStandard, 22, 24);
    CHECK (board24.numFrets() == 44);
    CHECK (board24.pitchAt (0, 0) == 80);
    CHECK_THAT (board24.fretDistance (24), Catch::Matchers::WithinAbs (0.5, 1e-9));
}

TEST_CASE ("Voicing finder: open C and G shapes come first")
{
    const auto board = Fretboard::fromPreset (stringTunings::guitarStandard);

    const auto c = findVoicings (board, { 0, 4, 7 }, 0);
    REQUIRE_FALSE (c.empty());
    CHECK (c.front().frets == std::vector<int> { -1, 3, 2, 0, 1, 0 });

    const auto g = findVoicings (board, { 7, 11, 2 }, 7);
    REQUIRE_FALSE (g.empty());
    CHECK (g.front().highestFret() == 3);
    CHECK (g.front().numSounding() == 6);

    // Every result sounds every chord tone and has the right bass.
    const auto am7 = findVoicings (board, { 9, 0, 4, 7 }, 9);
    REQUIRE_FALSE (am7.empty());
    for (const auto& v : am7)
    {
        const auto p = v.pitches (board);
        CHECK (*std::min_element (p.begin(), p.end()) % 12 == 9);
        for (int pc : { 9, 0, 4, 7 })
            CHECK (std::any_of (p.begin(), p.end(), [pc] (int x) { return x % 12 == pc; }));
    }
}

TEST_CASE ("Voicing finder works in other temperaments")
{
    // 24-TET major triad: 0, 8, 14 steps.
    const auto board = Fretboard::fromPreset (stringTunings::guitarStandard, 22, 24);
    const auto v = findVoicings (board, { 0, 8, 14 }, 0);
    REQUIRE_FALSE (v.empty());
    for (const auto& voicing : v)
        CHECK (voicing.pitches (board).size() >= 3);
}
