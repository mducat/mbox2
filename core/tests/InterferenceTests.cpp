#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "mbox/analysis/Interference.h"

#include <algorithm>
#include <array>
#include <cmath>

using namespace mbox::analysis;
using Catch::Approx;

namespace
{
    std::vector<Partial> sines (std::initializer_list<double> freqs)
    {
        std::vector<Partial> p;
        int source = 0;
        for (const auto f : freqs)
            p.push_back ({ f, 1.0, source++, 1 });
        return p;
    }
} // namespace

TEST_CASE ("Two close sines beat between constructive and destructive")
{
    // 440 and 442 Hz: beat at 2 Hz, in phase at t = 0, in opposition at t = 0.25 s.
    const auto p     = sines ({ 440.0, 442.0 });
    const auto pairs = findBeatingPairs (p);
    REQUIRE (pairs.size() == 1);
    CHECK (pairs[0].beatHz == Approx (2.0));
    CHECK (pairs[0].depth == Approx (1.0));

    CHECK (pairSumAmplitude (pairs[0], 0.0) == Approx (2.0));
    CHECK (pairSumAmplitude (pairs[0], 0.25) == Approx (0.0).margin (1e-9));
    CHECK (pairSumAmplitude (pairs[0], 0.5) == Approx (2.0));
    CHECK (pairSumAmplitude (pairs[0], 0.125) == Approx (std::sqrt (2.0))); // quadrature: no net interference

    CHECK (beatViewDuration (pairs) == Approx (1.0)); // 2 cycles of 2 Hz
}

TEST_CASE ("Unequal amplitudes never cancel completely")
{
    std::vector<Partial> p { { 300.0, 1.0, 0, 1 }, { 301.0, 0.25, 1, 4 } };
    const auto pairs = findBeatingPairs (p);
    REQUIRE (pairs.size() == 1);
    CHECK (pairs[0].depth == Approx (0.25));
    CHECK (pairSumAmplitude (pairs[0], 0.0) == Approx (1.25));
    CHECK (pairSumAmplitude (pairs[0], 0.5) == Approx (0.75));
}

TEST_CASE ("Distant or harmonic partials do not beat")
{
    CHECK (findBeatingPairs (sines ({ 220.0, 440.0 })).empty());
    CHECK (findBeatingPairs (sines ({ 440.0, 470.0 })).empty()); // 30 Hz: roughness, not a beat
    CHECK (beatViewDuration ({}) == Approx (0.1));
}

TEST_CASE ("findBeatingPairs ranks the deepest beats of a chord first")
{
    // 12-TET C major: C's 3rd harmonic vs G's 2nd beat slowly (~0.9 Hz at C4),
    // E's 4th vs C's 5th faster (~10 Hz).
    const std::array chord { midiToFrequency (60), midiToFrequency (64), midiToFrequency (67) };
    const auto partials = harmonicPartials (chord, 8);
    const auto pairs    = findBeatingPairs (partials, 5);

    REQUIRE_FALSE (pairs.empty());
    CHECK (std::is_sorted (pairs.begin(), pairs.end(), [] (const BeatingPair& x, const BeatingPair& y) { return x.depth > y.depth; }));
    CHECK (pairs[0].beatHz == Approx (3.0 * midiToFrequency (60) - 2.0 * midiToFrequency (67)).margin (1e-9));
    for (const auto& pair : pairs)
    {
        CHECK (pair.a.source != pair.b.source);
        CHECK (pair.a.frequency <= pair.b.frequency);
        CHECK (pair.beatHz == Approx (pair.b.frequency - pair.a.frequency));
        CHECK (pair.beatHz <= 20.0);
    }

    // Just intonation 4:5:6 has exact coincidences instead of beats.
    const std::array just { 400.0, 500.0, 600.0 };
    CHECK (findBeatingPairs (harmonicPartials (just, 8), 20).empty());
}
