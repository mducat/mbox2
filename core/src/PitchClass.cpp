#include "mbox/theory/PitchClass.h"

#include <array>
#include <bit>
#include <cctype>

namespace mbox::theory
{

namespace
{
    constexpr std::array<const char*, 12> kSharpNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    constexpr std::array<const char*, 12> kFlatNames  { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
} // namespace

std::optional<PitchClass> PitchClass::fromName (std::string_view name)
{
    if (name.empty())
        return std::nullopt;

    int base = 0;
    switch (std::toupper (static_cast<unsigned char> (name.front())))
    {
        case 'C': base = 0;  break;
        case 'D': base = 2;  break;
        case 'E': base = 4;  break;
        case 'F': base = 5;  break;
        case 'G': base = 7;  break;
        case 'A': base = 9;  break;
        case 'B': base = 11; break;
        default:  return std::nullopt;
    }

    for (const char c : name.substr (1))
    {
        if (c == '#')       ++base;
        else if (c == 'b')  --base;
        else                return std::nullopt;
    }

    return PitchClass (base);
}

std::string PitchClass::name (bool preferFlats) const
{
    return preferFlats ? kFlatNames[v] : kSharpNames[v];
}

PitchClassSet::PitchClassSet (std::initializer_list<int> pcs) noexcept
{
    for (const int pc : pcs)
        insert (PitchClass (pc));
}

PitchClassSet PitchClassSet::fromMidiNotes (const std::vector<int>& midiNotes) noexcept
{
    PitchClassSet s;
    for (const int n : midiNotes)
        s.insert (PitchClass::fromMidi (n));
    return s;
}

int PitchClassSet::size() const noexcept
{
    return std::popcount (bits);
}

PitchClassSet PitchClassSet::transposed (int semitones) const noexcept
{
    const int n = mod12 (semitones);
    const unsigned m = bits;
    return PitchClassSet (static_cast<std::uint16_t> (((m << n) | (m >> (12 - n))) & 0x0FFFu));
}

PitchClassSet PitchClassSet::inverted() const noexcept
{
    PitchClassSet s;
    for (int i = 0; i < kNumPitchClasses; ++i)
        if (contains (PitchClass (i)))
            s.insert (PitchClass (i).inverted());
    return s;
}

std::vector<PitchClass> PitchClassSet::toVector() const
{
    std::vector<PitchClass> out;
    for (int i = 0; i < kNumPitchClasses; ++i)
        if (contains (PitchClass (i)))
            out.emplace_back (i);
    return out;
}

std::vector<PitchClass> PitchClassSet::toFifthsOrder() const
{
    std::vector<PitchClass> out;
    for (int pos = 0; pos < kNumPitchClasses; ++pos)
        if (const auto pc = fifths::atPosition (pos); contains (pc))
            out.push_back (pc);
    return out;
}

} // namespace mbox::theory
