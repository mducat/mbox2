#include "mbox/analysis/Interference.h"

#include <algorithm>
#include <cmath>

namespace mbox::analysis
{

std::vector<BeatingPair> findBeatingPairs (std::span<const Partial> partials, std::size_t maxPairs, double minBeatHz, double maxBeatHz)
{
    std::vector<BeatingPair> pairs;

    for (std::size_t i = 0; i < partials.size(); ++i)
    {
        for (std::size_t j = i + 1; j < partials.size(); ++j)
        {
            const auto& p = partials[i];
            const auto& q = partials[j];
            if (p.source == q.source)
                continue;

            const auto beat = std::abs (q.frequency - p.frequency);
            if (beat < minBeatHz || beat > maxBeatHz)
                continue;

            const bool ordered = p.frequency <= q.frequency;
            pairs.push_back ({ ordered ? p : q, ordered ? q : p, beat, std::min (p.amplitude, q.amplitude),
                               pairRoughness (p.frequency, p.amplitude, q.frequency, q.amplitude) });
        }
    }

    std::sort (pairs.begin(), pairs.end(), [] (const BeatingPair& x, const BeatingPair& y)
    {
        return x.depth != y.depth ? x.depth > y.depth : x.beatHz < y.beatHz;
    });
    if (pairs.size() > maxPairs)
        pairs.resize (maxPairs);
    return pairs;
}

double pairSumAmplitude (const BeatingPair& pair, double t) noexcept
{
    const auto a1 = pair.a.amplitude, a2 = pair.b.amplitude;
    const auto c  = std::cos (2.0 * 3.14159265358979323846 * pair.beatHz * t);
    return std::sqrt (std::max (0.0, a1 * a1 + a2 * a2 + 2.0 * a1 * a2 * c));
}

std::vector<float> sumAmplitudeEnvelope (std::span<const Partial> partials,
                                         double durationSeconds,
                                         double windowSeconds,
                                         int points)
{
    std::vector<float> envelope;
    if (partials.empty() || points <= 0 || durationSeconds <= 0.0 || windowSeconds <= 0.0)
        return envelope;

    double maxFrequency = 0.0;
    for (const auto& p : partials)
        maxFrequency = std::max (maxFrequency, p.frequency);

    const auto sampleRate = std::clamp (8.0 * maxFrequency, 8000.0, 48000.0);
    const auto window     = std::max<std::size_t> (1, static_cast<std::size_t> (std::lround (windowSeconds * sampleRate)));
    const auto numSamples = static_cast<std::size_t> (durationSeconds * sampleRate) + window + 1;

    struct Phasor { double re, im, c, s, a; };
    std::vector<Phasor> osc;
    osc.reserve (partials.size());
    for (const auto& p : partials)
    {
        const auto w = 2.0 * 3.14159265358979323846 * p.frequency / sampleRate;
        osc.push_back ({ 1.0, 0.0, std::cos (w), std::sin (w), p.amplitude });
    }

    std::vector<float> magnitude (numSamples);
    for (std::size_t n = 0; n < numSamples; ++n)
    {
        double x = 0.0;
        for (auto& o : osc)
        {
            x += o.a * o.im;
            const auto re = o.re * o.c - o.im * o.s;
            o.im          = o.re * o.s + o.im * o.c;
            o.re          = re;
        }
        magnitude[n] = static_cast<float> (std::abs (x));

        if ((n & 1023) == 1023) // keep the phasors on the unit circle
            for (auto& o : osc)
            {
                const auto m = std::hypot (o.re, o.im);
                o.re /= m;
                o.im /= m;
            }
    }

    envelope.resize (static_cast<std::size_t> (points));
    for (int k = 0; k < points; ++k)
    {
        const auto t     = durationSeconds * k / std::max (1, points - 1);
        const auto start = std::min (static_cast<std::size_t> (t * sampleRate), numSamples - window);
        envelope[static_cast<std::size_t> (k)] = *std::max_element (magnitude.begin() + static_cast<std::ptrdiff_t> (start),
                                                                    magnitude.begin() + static_cast<std::ptrdiff_t> (start + window));
    }
    return envelope;
}

double beatViewDuration (std::span<const BeatingPair> pairs, double cycles, double minSeconds, double maxSeconds) noexcept
{
    if (pairs.empty())
        return minSeconds;

    const auto slowest = std::min_element (pairs.begin(), pairs.end(),
                                           [] (const BeatingPair& x, const BeatingPair& y) { return x.beatHz < y.beatHz; });
    return std::clamp (cycles / slowest->beatHz, minSeconds, maxSeconds);
}

} // namespace mbox::analysis
