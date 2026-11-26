#pragma once

#include <optional>
#include <span>
#include <vector>

/**
    Harmonic analysis helpers (no JUCE dependency).

    Everything here works on frequencies in Hz, never on note numbers or
    pitch classes, so it applies unchanged to any tuning system (12-TET,
    N-TET, just intonation...). Converting notes to frequencies is the job of
    the tuning layer.
*/
namespace mbox::analysis
{

/** Reference pitch used by the 12-TET helpers below. */
inline constexpr double kDefaultA4Hz = 440.0;

/** 12-TET frequency of a (possibly fractional) MIDI note number. */
double midiToFrequency (double midiNote, double a4Hz = kDefaultA4Hz) noexcept;

/** Inverse of midiToFrequency: fractional 12-TET MIDI note number of a frequency. */
double frequencyToMidi (double frequencyHz, double a4Hz = kDefaultA4Hz) noexcept;

/** Signed distance from a to b in cents (1200 per octave). */
double centsBetween (double aHz, double bHz) noexcept;

/** One sinusoidal component of a harmonic tone. */
struct Partial
{
    double frequency = 0.0; // Hz
    double amplitude = 0.0; // linear, fundamental of each tone = 1
    int source = 0;         // index of the tone it belongs to
    int harmonic = 1;       // 1 = fundamental, 2 = octave, 3 = twelfth...
};

/** Amplitude of the n-th harmonic for a 1 / n^rolloff spectrum (rolloff 1 = sawtooth-like). */
double harmonicAmplitude (int harmonic, double rolloff = 1.0) noexcept;

/**
    Ideal harmonic partials of a set of tones (fundamentals in Hz), sorted by
    frequency. Partials at or above maxFrequencyHz are dropped.
*/
std::vector<Partial> harmonicPartials (std::span<const double> fundamentalsHz,
                                       int numHarmonics,
                                       double rolloff = 1.0,
                                       double maxFrequencyHz = 20000.0);

/** Two partials from different tones that (nearly) share a frequency. */
struct Coincidence
{
    Partial a;
    Partial b;
    double cents = 0.0; // a -> b
};

/** Pairs of partials from different tones lying within toleranceCents of each other. */
std::vector<Coincidence> findCoincidences (std::span<const Partial> partials, double toleranceCents = 15.0);

/**
    Sensory dissonance (roughness) between two pure tones, after Plomp & Levelt
    as parameterised by Sethares. 0 = smooth; maximum around a quarter of a
    critical band.
*/
double pairRoughness (double f1, double a1, double f2, double a2) noexcept;

/** Total roughness of a spectrum (sum over all pairs of partials). */
double roughness (std::span<const Partial> partials) noexcept;

/**
    Virtual (missing) fundamental: the highest frequency f0 >= minFrequencyHz
    such that every tone is within toleranceCents of an integer multiple
    k * f0 with k <= maxHarmonic. For C-E-G this is a C two octaves below.
    Returns nullopt if no such frequency exists.
*/
std::optional<double> virtualFundamental (std::span<const double> fundamentalsHz,
                                          double toleranceCents = 20.0,
                                          int maxHarmonic = 32,
                                          double minFrequencyHz = 8.0);

/** A local maximum found in a magnitude spectrum. */
struct SpectralPeak
{
    double frequency = 0.0; // Hz, refined by parabolic interpolation
    double magnitude = 0.0; // same unit as the input, interpolated
};

/**
    Local maxima of a magnitude spectrum (bin k = k * binWidthHz) above
    `threshold`, strongest first, at most maxPeaks. Use dB magnitudes for the
    best interpolation accuracy.
*/
std::vector<SpectralPeak> findPeaks (std::span<const float> magnitudes,
                                     double binWidthHz,
                                     float threshold,
                                     std::size_t maxPeaks = 32);

} // namespace mbox::analysis
