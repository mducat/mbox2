#pragma once

#include "mbox/analysis/Harmonics.h"

#include <cstddef>
#include <span>
#include <vector>

/**
    Interference between the oscillators of a chord (no JUCE dependency).

    Two partials of different notes add up constructively when they are in
    phase and destructively when they are in opposition. When their
    frequencies differ slightly, they drift in and out of phase: the
    amplitude of their sum swells and fades at the beat frequency |f1 - f2|.
*/
namespace mbox::analysis
{

/** Two partials of different notes close enough in frequency to beat. */
struct BeatingPair
{
    Partial a, b;            // a.frequency <= b.frequency
    double beatHz = 0.0;     // |f1 - f2|: rate of the constructive / destructive alternation
    double depth = 0.0;      // min (a1, a2): amplitude that comes and goes (full cancellation when equal)
    double roughness = 0.0;  // pairRoughness() of the two
};

/**
    Pairs of partials from different tones whose beat lies between minBeatHz
    and maxBeatHz, deepest first. Up to ~20 Hz beats are heard as a pulsation
    (above, they merge into roughness). Exact coincidences (beat < minBeatHz)
    are left to findCoincidences().
*/
std::vector<BeatingPair> findBeatingPairs (std::span<const Partial> partials,
                                           std::size_t maxPairs = 8,
                                           double minBeatHz = 0.05,
                                           double maxBeatHz = 20.0);

/**
    Amplitude of the sum of the two partials of a pair at time t (seconds),
    both starting in phase: sqrt (a1^2 + a2^2 + 2 a1 a2 cos (2 pi beat t)).
    It swings between a1 + a2 (in phase: constructive) and |a1 - a2|
    (opposition: destructive).
*/
double pairSumAmplitude (const BeatingPair& pair, double t) noexcept;

/**
    Amplitude envelope of the sum of all `partials` (all starting in phase,
    sine phase 0) over durationSeconds, at `points` evenly spaced times: the
    peak |sum| over a window starting at each time. Use about one period of
    the lowest note as the window, so the peak of every cycle is caught.
    Its maximum possible value is the sum of the amplitudes (everything in
    phase).
*/
std::vector<float> sumAmplitudeEnvelope (std::span<const Partial> partials,
                                         double durationSeconds,
                                         double windowSeconds,
                                         int points);

/**
    A duration that shows `cycles` cycles of the slowest of these beats,
    clamped to [minSeconds, maxSeconds]; minSeconds when there is no beat.
*/
double beatViewDuration (std::span<const BeatingPair> pairs, double cycles = 2.0,
                         double minSeconds = 0.1, double maxSeconds = 2.0) noexcept;

} // namespace mbox::analysis
