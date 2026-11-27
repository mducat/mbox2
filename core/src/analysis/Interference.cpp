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

double beatViewDuration (std::span<const BeatingPair> pairs, double cycles, double minSeconds, double maxSeconds) noexcept
{
    if (pairs.empty())
        return minSeconds;

    const auto slowest = std::min_element (pairs.begin(), pairs.end(),
                                           [] (const BeatingPair& x, const BeatingPair& y) { return x.beatHz < y.beatHz; });
    return std::clamp (cycles / slowest->beatHz, minSeconds, maxSeconds);
}

} // namespace mbox::analysis
