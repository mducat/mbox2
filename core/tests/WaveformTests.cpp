#include <catch2/catch_test_macros.hpp>

#include "mbox/analysis/Waveform.h"

#include <cmath>
#include <numbers>
#include <vector>

using namespace mbox::analysis;

namespace
{
    // Two harmonics, so there are several zero crossings per period.
    std::vector<float> wave (std::size_t n, double period, double phase)
    {
        std::vector<float> x (n);
        for (std::size_t i = 0; i < n; ++i)
        {
            const double t = 2.0 * std::numbers::pi * (static_cast<double> (i) + phase) / period;
            x[i] = static_cast<float> (std::sin (t) + 0.8 * std::sin (3.0 * t + 1.0));
        }
        return x;
    }
} // namespace

TEST_CASE ("findTriggerIndex lands on the same point of the cycle")
{
    constexpr double period = 100.0;
    for (double shift : { 0.0, 13.0, 57.0, 81.0 })
    {
        const auto x     = wave (2000, period, shift);
        const auto start = findTriggerIndex (x, 400, 100);

        REQUIRE (start + 400 <= x.size());
        CHECK (x[start - 1] < 0.0f);
        CHECK (x[start] >= 0.0f);

        // Same phase whatever the shift of the input.
        const auto phase      = std::fmod (static_cast<double> (start) + shift, period);
        const auto reference  = std::fmod (static_cast<double> (findTriggerIndex (wave (2000, period, 0.0), 400, 100)), period);
        CHECK (std::abs (phase - reference) <= 1.0);
    }
}

TEST_CASE ("findTriggerIndex falls back without crossings")
{
    const std::vector<float> silence (500, 0.0f);
    CHECK (findTriggerIndex (silence, 100, 50) == 400);
    CHECK (findTriggerIndex (silence, 600, 50) == 0);
}
