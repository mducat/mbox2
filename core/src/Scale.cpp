#include "mbox/theory/Scale.h"

#include <array>

namespace mbox::theory
{

namespace scales
{
    namespace
    {
        constexpr std::array kMajor          { 0, 2, 4, 5, 7, 9, 11 };
        constexpr std::array kNaturalMinor   { 0, 2, 3, 5, 7, 8, 10 };
        constexpr std::array kHarmonicMinor  { 0, 2, 3, 5, 7, 8, 11 };
        constexpr std::array kMelodicMinor   { 0, 2, 3, 5, 7, 9, 11 };
        constexpr std::array kDorian         { 0, 2, 3, 5, 7, 9, 10 };
        constexpr std::array kPhrygian       { 0, 1, 3, 5, 7, 8, 10 };
        constexpr std::array kLydian         { 0, 2, 4, 6, 7, 9, 11 };
        constexpr std::array kMixolydian     { 0, 2, 4, 5, 7, 9, 10 };
        constexpr std::array kLocrian        { 0, 1, 3, 5, 6, 8, 10 };
        constexpr std::array kMajorPent      { 0, 2, 4, 7, 9 };
        constexpr std::array kMinorPent      { 0, 3, 5, 7, 10 };
        constexpr std::array kBlues          { 0, 3, 5, 6, 7, 10 };
        constexpr std::array kChromatic      { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
    } // namespace

    const ScaleType major           { "Major",            kMajor };
    const ScaleType naturalMinor    { "Natural Minor",    kNaturalMinor };
    const ScaleType harmonicMinor   { "Harmonic Minor",   kHarmonicMinor };
    const ScaleType melodicMinor    { "Melodic Minor",    kMelodicMinor };
    const ScaleType dorian          { "Dorian",           kDorian };
    const ScaleType phrygian        { "Phrygian",         kPhrygian };
    const ScaleType lydian          { "Lydian",           kLydian };
    const ScaleType mixolydian      { "Mixolydian",       kMixolydian };
    const ScaleType locrian         { "Locrian",          kLocrian };
    const ScaleType majorPentatonic { "Major Pentatonic", kMajorPent };
    const ScaleType minorPentatonic { "Minor Pentatonic", kMinorPent };
    const ScaleType blues           { "Blues",            kBlues };
    const ScaleType chromatic       { "Chromatic",        kChromatic };

    std::span<const ScaleType* const> all()
    {
        static const std::array<const ScaleType*, 13> list {
            &major, &naturalMinor, &harmonicMinor, &melodicMinor,
            &dorian, &phrygian, &lydian, &mixolydian, &locrian,
            &majorPentatonic, &minorPentatonic, &blues, &chromatic
        };
        return list;
    }
} // namespace scales

std::vector<int> ScaleType::stepsIn (int targetDivisions) const
{
    std::vector<int> out;
    out.reserve (intervals.size());
    for (const int i : intervals)
        out.push_back (edo::convert (i, divisions, targetDivisions));
    return out;
}

PitchClass Scale::degree (int index) const noexcept
{
    const int n = numDegrees();
    const int wrapped = ((index % n) + n) % n;
    return root.transposed (edo::convert (scaleType->intervals[static_cast<std::size_t> (wrapped)],
                                          scaleType->divisions, divisions()));
}

PitchClassSet Scale::pitchClasses() const noexcept
{
    auto s = PitchClassSet::withDivisions (divisions());
    for (const int i : scaleType->intervals)
        s.insert (root.transposed (edo::convert (i, scaleType->divisions, divisions())));
    return s;
}

int Scale::quantize (int note) const noexcept
{
    const auto set = pitchClasses();
    if (set.empty())
        return note;

    const int d = divisions();
    for (int delta = 0; delta <= d / 2; ++delta)
    {
        if (set.contains (PitchClass::fromNote (note - delta, d))) return note - delta;
        if (set.contains (PitchClass::fromNote (note + delta, d))) return note + delta;
    }
    return note;
}

std::string Scale::name (bool preferFlats) const
{
    return root.name (preferFlats) + " " + std::string (scaleType->name);
}

} // namespace mbox::theory
