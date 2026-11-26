#include "mbox/analysis/Waveform.h"

#include <algorithm>

namespace mbox::analysis
{

std::size_t findTriggerIndex (std::span<const float> samples, std::size_t windowLength, std::size_t searchLength) noexcept
{
    if (windowLength >= samples.size())
        return 0;

    const auto latest   = samples.size() - windowLength;
    const auto earliest = latest > searchLength ? latest - searchLength : std::size_t { 1 };

    std::size_t best = latest;
    float bestSlope  = 0.0f;
    for (auto i = std::max<std::size_t> (earliest, 1); i <= latest; ++i)
    {
        const auto before = samples[i - 1], after = samples[i];
        if (before < 0.0f && after >= 0.0f && after - before > bestSlope)
        {
            bestSlope = after - before;
            best      = i;
        }
    }
    return best;
}

} // namespace mbox::analysis
