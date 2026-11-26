#include "mbox/instrument/Fretboard.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace mbox::instrument
{

namespace stringTunings
{
    namespace
    {
        constexpr std::array kStandard { 40, 45, 50, 55, 59, 64 };
        constexpr std::array kDropD    { 38, 45, 50, 55, 59, 64 };
        constexpr std::array kDADGAD   { 38, 45, 50, 55, 57, 62 };
        constexpr std::array kOpenG    { 38, 43, 50, 55, 59, 62 };
        constexpr std::array kOpenD    { 38, 45, 50, 54, 57, 62 };
        constexpr std::array kSeven    { 35, 40, 45, 50, 55, 59, 64 };
        constexpr std::array kBass4    { 28, 33, 38, 43 };
        constexpr std::array kUkulele  { 67, 60, 64, 69 };
    } // namespace

    const TuningPreset guitarStandard { "Guitare - Standard (EADGBE)", kStandard };
    const TuningPreset guitarDropD    { "Guitare - Drop D (DADGBE)",  kDropD };
    const TuningPreset guitarDADGAD   { "Guitare - DADGAD",           kDADGAD };
    const TuningPreset guitarOpenG    { "Guitare - Open G (DGDGBD)",  kOpenG };
    const TuningPreset guitarOpenD    { "Guitare - Open D (DADF#AD)", kOpenD };
    const TuningPreset guitar7String  { "Guitare - 7 cordes (BEADGBE)", kSeven };
    const TuningPreset bass4          { "Basse - 4 cordes (EADG)",    kBass4 };
    const TuningPreset ukulele        { "Ukulélé (GCEA)",             kUkulele };

    std::span<const TuningPreset* const> presets()
    {
        static const std::array<const TuningPreset*, 8> list {
            &guitarStandard, &guitarDropD, &guitarDADGAD, &guitarOpenG,
            &guitarOpenD, &guitar7String, &bass4, &ukulele
        };
        return list;
    }
} // namespace stringTunings

Fretboard::Fretboard (std::vector<int> openPitches, int numFrets, int stepsPerOctave)
    : open (std::move (openPitches)), frets (std::max (0, numFrets)), steps (std::max (1, stepsPerOctave))
{
}

Fretboard Fretboard::fromPreset (const TuningPreset& preset, int numFrets12, int stepsPerOctave)
{
    std::vector<int> pitches;
    pitches.reserve (preset.openNotes.size());
    for (const int midi : preset.openNotes)
        pitches.push_back (pitchFromTwelveTet (midi, stepsPerOctave));

    return { std::move (pitches), pitchFromTwelveTet (numFrets12, stepsPerOctave), stepsPerOctave };
}

std::vector<FretPosition> Fretboard::positionsOf (int pitch) const
{
    std::vector<FretPosition> result;
    for (int s = 0; s < numStrings(); ++s)
        if (const int f = pitch - open[static_cast<std::size_t> (s)]; f >= 0 && f <= frets)
            result.push_back ({ s, f });
    return result;
}

std::vector<FretPosition> Fretboard::positionsOfClass (int pitchClass) const
{
    std::vector<FretPosition> result;
    for (int s = 0; s < numStrings(); ++s)
        for (int f = 0; f <= frets; ++f)
            if (wrap (pitchAt (s, f), steps) == wrap (pitchClass, steps))
                result.push_back ({ s, f });
    return result;
}

double Fretboard::fretDistance (int fret) const noexcept
{
    return 1.0 - std::pow (2.0, -static_cast<double> (fret) / steps);
}

std::vector<int> Fretboard::inlayFrets() const
{
    std::vector<int> result;
    for (int octave = 0; octave * 12 <= 24; ++octave)
        for (const int f : std::array { 3, 5, 7, 9, 12 })
            if (const int scaled = pitchFromTwelveTet (octave * 12 + f, steps); scaled > 0 && scaled <= frets
                && std::find (result.begin(), result.end(), scaled) == result.end())
                result.push_back (scaled);
    return result;
}

} // namespace mbox::instrument
