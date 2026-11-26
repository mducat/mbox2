#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "mbox/sound/Instruments.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

using namespace mbox::sound;
using Catch::Approx;

namespace
{
    constexpr double kRate = 48000.0;

    template <typename Model>
    std::vector<float> renderModel (Model& m, int numSamples)
    {
        std::vector<float> out (static_cast<std::size_t> (numSamples));
        for (auto& s : out)
            s = m.next();
        return out;
    }

    /** Frequency of the strongest spectral peak between lo and hi Hz (Hann-windowed DFT scan + refinement). */
    double peakFrequency (const std::vector<float>& x, double lo, double hi)
    {
        const auto n = x.size();
        auto power = [&] (double f)
        {
            double re = 0.0, im = 0.0;
            for (std::size_t i = 0; i < n; ++i)
            {
                const double w = 0.5 - 0.5 * std::cos (2.0 * std::numbers::pi * static_cast<double> (i) / static_cast<double> (n - 1));
                const double ph = 2.0 * std::numbers::pi * f * static_cast<double> (i) / kRate;
                re += w * x[i] * std::cos (ph);
                im -= w * x[i] * std::sin (ph);
            }
            return re * re + im * im;
        };

        double best = 0.5 * (lo + hi), bestPower = -1.0;
        for (double step = (hi - lo) / 200.0, span = hi - lo; step > 0.002; span = 20.0 * step, step /= 10.0)
        {
            const double from = std::max (lo, best - 0.5 * span), to = std::min (hi, best + 0.5 * span);
            for (double f = from; f <= to; f += step)
                if (const double p = power (f); p > bestPower)
                {
                    bestPower = p;
                    best = f;
                }
        }
        return best;
    }

    double rms (const std::vector<float>& x, std::size_t from, std::size_t count)
    {
        double sum = 0.0;
        for (std::size_t i = from; i < from + count; ++i)
            sum += static_cast<double> (x[i]) * x[i];
        return std::sqrt (sum / static_cast<double> (count));
    }

    double cents (double a, double b) { return 1200.0 * std::log2 (b / a); }

    bool finiteAndBounded (const std::vector<float>& x)
    {
        return std::all_of (x.begin(), x.end(), [] (float s) { return std::isfinite (s) && std::abs (s) < 1.5f; });
    }
} // namespace

TEST_CASE ("Plucked string is tuned to the requested frequency, N-TET included")
{
    PluckedString s;
    // A4, a 19-TET step above it, a 31-TET low note and a high note (fractional delay matters).
    for (const double f : { 440.0, 440.0 * std::exp2 (1.0 / 19.0), 110.0 * std::exp2 (7.0 / 31.0), 1318.5 })
    {
        s.start (f, kRate, 0.8f, 1234);
        CHECK (s.periodSamples() == Approx (kRate / f));
        const auto out = renderModel (s, 24000);
        CHECK (finiteAndBounded (out));
        CHECK (std::abs (cents (f, peakFrequency (out, f * 0.97, f * 1.03))) < 3.0);
    }
}

TEST_CASE ("Plucked string decays and release damps it quickly")
{
    PluckedString s;
    s.start (196.0, kRate, 0.9f, 7);
    const auto ring = renderModel (s, static_cast<int> (kRate * 2));
    CHECK (rms (ring, 0, 4800) > 0.05);
    CHECK (rms (ring, ring.size() - 4800, 4800) < rms (ring, 0, 4800) * 0.5);
    CHECK (s.isActive());

    s.release();
    renderModel (s, static_cast<int> (kRate)); // 1 s after release: 60 dB down in 0.15 s
    CHECK_FALSE (s.isActive());
}

TEST_CASE ("Piano partials are inharmonic but the fundamental stays exact")
{
    const double f0 = 261.63;
    const double B = PianoTone::inharmonicityFor (f0);
    CHECK (B > 0.0);
    CHECK (PianoTone::partialFrequency (f0, 1, B) == Approx (f0));
    CHECK (PianoTone::partialFrequency (f0, 8, B) > 8.0 * f0); // stretched upper partials
    CHECK (PianoTone::inharmonicityFor (2000.0) > PianoTone::inharmonicityFor (100.0));

    PianoTone p;
    for (const double f : { f0, 440.0 * std::exp2 (5.0 / 31.0), 2093.0 })
    {
        p.start (f, kRate, 0.7f, 99);
        const auto out = renderModel (p, 24000);
        CHECK (finiteAndBounded (out));
        CHECK (std::abs (cents (f, peakFrequency (out, f * 0.97, f * 1.03))) < 2.0);
    }
}

TEST_CASE ("Piano tone is louder when struck harder, decays, and the damper stops it")
{
    PianoTone soft, hard;
    soft.start (220.0, kRate, 0.3f, 1);
    hard.start (220.0, kRate, 1.0f, 1);
    const auto a = renderModel (soft, 9600);
    const auto b = renderModel (hard, 9600);
    CHECK (rms (b, 0, 4800) > 2.0 * rms (a, 0, 4800));
    CHECK (a.front() == Approx (0.0).margin (0.02)); // no click at the onset

    const auto later = renderModel (hard, static_cast<int> (kRate * 3));
    CHECK (rms (later, later.size() - 4800, 4800) < rms (b, 0, 4800) * 0.5);

    hard.release();
    renderModel (hard, static_cast<int> (kRate));
    CHECK_FALSE (hard.isActive());
}

TEST_CASE ("VoicePool starts scheduled notes on time and releases them")
{
    VoicePool pool;
    pool.prepare (kRate);

    NoteEvent e;
    e.frequencyHz = 330.0;
    e.instrument = Instrument::guitar;
    e.startSample = 1000;
    e.lengthSamples = 4800;
    REQUIRE (pool.queue (e));
    CHECK (pool.numPending() == 1);

    std::vector<float> out (2048, 0.0f);
    pool.render (out.data(), 512); // rendered in several blocks
    pool.render (out.data() + 512, 1536);

    CHECK (std::all_of (out.begin(), out.begin() + 1000, [] (float s) { return s == 0.0f; }));
    CHECK (std::any_of (out.begin() + 1000, out.end(), [] (float s) { return s != 0.0f; }));
    CHECK (pool.numPending() == 0);
    CHECK (pool.numActiveVoices() == 1);

    // Released after 0.1 s, silent well before 1 s.
    std::vector<float> tail (static_cast<std::size_t> (kRate), 0.0f);
    pool.render (tail.data(), static_cast<int> (tail.size()));
    CHECK (pool.numActiveVoices() == 0);
    CHECK (finiteAndBounded (tail));
}

TEST_CASE ("VoicePool steals voices when full and stopAll cancels pending notes")
{
    VoicePool pool;
    pool.prepare (kRate);

    for (int i = 0; i < VoicePool::kNumVoices + 4; ++i)
        pool.queue ({ 110.0 * std::exp2 (i / 12.0), 0.5f, i % 2 == 0 ? Instrument::piano : Instrument::guitar, 0, 0 });
    pool.queue ({ 440.0, 0.5f, Instrument::piano, 100000, 0 });

    std::vector<float> out (256, 0.0f);
    pool.render (out.data(), static_cast<int> (out.size()));
    CHECK (pool.numActiveVoices() == VoicePool::kNumVoices);
    CHECK (pool.numPending() == 1);
    CHECK (finiteAndBounded (out));

    pool.stopAll();
    CHECK (pool.numPending() == 0);
    std::vector<float> tail (static_cast<std::size_t> (kRate), 0.0f);
    pool.render (tail.data(), static_cast<int> (tail.size()));
    CHECK (pool.numActiveVoices() == 0);
}
