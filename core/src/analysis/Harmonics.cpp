#include "mbox/analysis/Harmonics.h"

#include <algorithm>
#include <cmath>

namespace mbox::analysis
{

double midiToFrequency (double midiNote, double a4Hz) noexcept
{
    return a4Hz * std::exp2 ((midiNote - 69.0) / 12.0);
}

double frequencyToMidi (double frequencyHz, double a4Hz) noexcept
{
    return 69.0 + 12.0 * std::log2 (frequencyHz / a4Hz);
}

double centsBetween (double aHz, double bHz) noexcept
{
    return 1200.0 * std::log2 (bHz / aHz);
}

double harmonicAmplitude (int harmonic, double rolloff) noexcept
{
    return harmonic < 1 ? 0.0 : 1.0 / std::pow (static_cast<double> (harmonic), rolloff);
}

std::vector<Partial> harmonicPartials (std::span<const double> fundamentalsHz,
                                       int numHarmonics,
                                       double rolloff,
                                       double maxFrequencyHz)
{
    std::vector<Partial> result;
    result.reserve (fundamentalsHz.size() * static_cast<std::size_t> (std::max (numHarmonics, 0)));

    for (std::size_t s = 0; s < fundamentalsHz.size(); ++s)
    {
        const auto f0 = fundamentalsHz[s];
        if (f0 <= 0.0)
            continue;

        for (int h = 1; h <= numHarmonics; ++h)
        {
            const auto f = f0 * h;
            if (f >= maxFrequencyHz)
                break;
            result.push_back ({ f, harmonicAmplitude (h, rolloff), static_cast<int> (s), h });
        }
    }

    std::sort (result.begin(), result.end(), [] (const Partial& a, const Partial& b) { return a.frequency < b.frequency; });
    return result;
}

std::vector<Coincidence> findCoincidences (std::span<const Partial> partials, double toleranceCents)
{
    // Works on any order, but is fastest on frequency-sorted input (early break).
    std::vector<Partial> sorted (partials.begin(), partials.end());
    std::sort (sorted.begin(), sorted.end(), [] (const Partial& a, const Partial& b) { return a.frequency < b.frequency; });

    std::vector<Coincidence> result;
    for (std::size_t i = 0; i < sorted.size(); ++i)
    {
        for (std::size_t j = i + 1; j < sorted.size(); ++j)
        {
            const auto cents = centsBetween (sorted[i].frequency, sorted[j].frequency);
            if (cents > toleranceCents)
                break;
            if (sorted[i].source != sorted[j].source)
                result.push_back ({ sorted[i], sorted[j], cents });
        }
    }
    return result;
}

double pairRoughness (double f1, double a1, double f2, double a2) noexcept
{
    // Sethares, "Tuning, Timbre, Spectrum, Scale" (2nd ed.), appendix E.
    constexpr double dStar = 0.24, s1 = 0.0207, s2 = 18.96, b1 = 3.51, b2 = 5.75;

    const auto fMin = std::min (f1, f2);
    const auto df   = std::abs (f2 - f1);
    const auto s    = dStar / (s1 * fMin + s2);

    return std::min (a1, a2) * (std::exp (-b1 * s * df) - std::exp (-b2 * s * df));
}

double roughness (std::span<const Partial> partials) noexcept
{
    double total = 0.0;
    for (std::size_t i = 0; i < partials.size(); ++i)
        for (std::size_t j = i + 1; j < partials.size(); ++j)
            total += pairRoughness (partials[i].frequency, partials[i].amplitude,
                                    partials[j].frequency, partials[j].amplitude);
    return total;
}

std::optional<double> virtualFundamental (std::span<const double> fundamentalsHz,
                                          double toleranceCents,
                                          int maxHarmonic,
                                          double minFrequencyHz)
{
    if (fundamentalsHz.empty())
        return std::nullopt;

    const auto lowest = *std::min_element (fundamentalsHz.begin(), fundamentalsHz.end());
    if (lowest <= 0.0)
        return std::nullopt;

    // Candidates are subharmonics of the lowest tone, highest first.
    for (int k = 1; k <= maxHarmonic; ++k)
    {
        const auto f0 = lowest / k;
        if (f0 < minFrequencyHz)
            break;

        const bool fits = std::all_of (fundamentalsHz.begin(), fundamentalsHz.end(), [&] (double f)
        {
            const auto m = std::round (f / f0);
            return m >= 1.0 && m <= maxHarmonic && std::abs (centsBetween (m * f0, f)) <= toleranceCents;
        });

        if (fits)
            return f0;
    }
    return std::nullopt;
}

std::vector<SpectralPeak> findPeaks (std::span<const float> magnitudes,
                                     double binWidthHz,
                                     float threshold,
                                     std::size_t maxPeaks)
{
    std::vector<SpectralPeak> peaks;

    for (std::size_t k = 1; k + 1 < magnitudes.size(); ++k)
    {
        const double a = magnitudes[k - 1], b = magnitudes[k], c = magnitudes[k + 1];
        if (b < threshold || b <= a || b < c)
            continue;

        const auto denom  = a - 2.0 * b + c;
        const auto offset = denom != 0.0 ? 0.5 * (a - c) / denom : 0.0;
        peaks.push_back ({ (static_cast<double> (k) + offset) * binWidthHz, b - 0.25 * (a - c) * offset });
    }

    std::sort (peaks.begin(), peaks.end(), [] (const SpectralPeak& x, const SpectralPeak& y) { return x.magnitude > y.magnitude; });
    if (peaks.size() > maxPeaks)
        peaks.resize (maxPeaks);
    return peaks;
}

} // namespace mbox::analysis
