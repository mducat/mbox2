#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "mbox/analysis/Harmonics.h"

#include <array>
#include <cmath>
#include <numbers>

using namespace mbox::analysis;
using Catch::Approx;

TEST_CASE ("12-TET frequency conversions")
{
    CHECK (midiToFrequency (69) == Approx (440.0));
    CHECK (midiToFrequency (57) == Approx (220.0));
    CHECK (midiToFrequency (60) == Approx (261.6256).epsilon (1e-6));
    CHECK (frequencyToMidi (880.0) == Approx (81.0));
    CHECK (centsBetween (440.0, 880.0) == Approx (1200.0));
    CHECK (centsBetween (440.0, 440.0 * std::exp2 (1.0 / 24.0)) == Approx (50.0)); // quarter tone
}

TEST_CASE ("harmonicPartials builds a sorted harmonic series")
{
    const std::array f { 200.0, 100.0 };
    const auto partials = harmonicPartials (f, 4, 1.0, 20000.0);

    REQUIRE (partials.size() == 8);
    CHECK (partials.front().frequency == Approx (100.0));
    CHECK (partials.front().source == 1);
    CHECK (partials.back().frequency == Approx (800.0));
    CHECK (partials.back().harmonic == 4);

    for (const auto& p : partials)
        CHECK (p.amplitude == Approx (1.0 / p.harmonic));

    CHECK (harmonicPartials (f, 10, 1.0, 450.0).back().frequency < 450.0);
}

TEST_CASE ("findCoincidences detects shared partials of a just fifth")
{
    const std::array f { 200.0, 300.0 }; // 3:2
    const auto partials = harmonicPartials (f, 6);
    const auto shared   = findCoincidences (partials, 1.0);

    // 600 (3rd of 200 / 2nd of 300), 1200 (6th / 4th)
    REQUIRE (shared.size() == 2);
    CHECK (shared[0].a.frequency == Approx (600.0));
    CHECK (shared[1].a.frequency == Approx (1200.0));
    CHECK (shared[0].a.source != shared[0].b.source);
}

TEST_CASE ("roughness ranks consonance as expected")
{
    CHECK (pairRoughness (440.0, 1.0, 440.0, 1.0) == Approx (0.0));
    // A semitone is rougher than a fifth, which is rougher than an octave.
    auto chordRoughness = [] (double ratio)
    {
        const std::array f { 261.63, 261.63 * ratio };
        const auto p = harmonicPartials (f, 8);
        return roughness (p);
    };
    const auto semitone = chordRoughness (std::exp2 (1.0 / 12.0));
    const auto fifth    = chordRoughness (1.5);
    const auto octave   = chordRoughness (2.0);
    CHECK (semitone > fifth);
    CHECK (fifth > octave);
}

TEST_CASE ("virtualFundamental of a major triad")
{
    const std::array cMajor { midiToFrequency (60), midiToFrequency (64), midiToFrequency (67) };
    const auto f0 = virtualFundamental (cMajor);
    REQUIRE (f0.has_value());
    CHECK (frequencyToMidi (*f0) == Approx (36.0).margin (0.01)); // C two octaves down

    // Just intonation 4:5:6 is exact.
    const std::array just { 400.0, 500.0, 600.0 };
    CHECK (*virtualFundamental (just, 0.5) == Approx (100.0));

    // A 12-TET tritone + semitone cluster has no low-order common fundamental.
    const std::array cluster { 400.0, 400.0 * std::exp2 (1.0 / 12.0) };
    CHECK_FALSE (virtualFundamental (cluster, 5.0, 8).has_value());
}

TEST_CASE ("findPeaks locates sinusoids with sub-bin accuracy")
{
    constexpr int n = 1024;
    constexpr double fs = 8000.0, bin = fs / n;
    const double freqs[] = { 440.0, 1234.5 };

    // Magnitude spectrum of a Hann-windowed two-tone signal (naive DFT is fine for a test).
    std::array<double, n> x {};
    for (int i = 0; i < n; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * std::numbers::pi * i / n);
        x[static_cast<std::size_t> (i)] = w * (std::sin (2.0 * std::numbers::pi * freqs[0] * i / fs)
                                               + 0.5 * std::sin (2.0 * std::numbers::pi * freqs[1] * i / fs));
    }

    std::array<float, n / 2> magDb {};
    for (int k = 0; k < n / 2; ++k)
    {
        double re = 0.0, im = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double ph = 2.0 * std::numbers::pi * k * i / n;
            re += x[static_cast<std::size_t> (i)] * std::cos (ph);
            im -= x[static_cast<std::size_t> (i)] * std::sin (ph);
        }
        magDb[static_cast<std::size_t> (k)] = static_cast<float> (20.0 * std::log10 (std::hypot (re, im) + 1e-12));
    }

    const auto peaks = findPeaks (magDb, bin, 0.0f, 2);
    REQUIRE (peaks.size() == 2);
    CHECK (peaks[0].frequency == Approx (freqs[0]).margin (bin * 0.1));
    CHECK (peaks[1].frequency == Approx (freqs[1]).margin (bin * 0.1));
}
