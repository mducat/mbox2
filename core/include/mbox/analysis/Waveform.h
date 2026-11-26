#pragma once

#include <cstddef>
#include <span>

/** Oscilloscope helpers (no JUCE dependency). */
namespace mbox::analysis
{

/**
    Start index of a stable oscilloscope window of `windowLength` samples taken
    as late as possible in `samples`.

    Looks for rising zero crossings among the `searchLength` positions just
    before the latest possible start, and keeps the steepest one: for a
    periodic signal it lands on the same point of the cycle from one call to
    the next, so the drawn wave stands still. Pass about one period as
    searchLength. Falls back to the latest start if there is no crossing.
*/
std::size_t findTriggerIndex (std::span<const float> samples, std::size_t windowLength, std::size_t searchLength) noexcept;

} // namespace mbox::analysis
