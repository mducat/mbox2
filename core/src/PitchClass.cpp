#include "mbox/theory/PitchClass.h"

#include <bit>
#include <cassert>
#include <cctype>
#include <cmath>
#include <numeric>

namespace mbox::theory
{

namespace
{
    constexpr std::array<const char*, 12> kSharpNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    constexpr std::array<const char*, 12> kFlatNames  { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };

    // Cents of the just perfect fifth 3/2.
    constexpr double kJustFifthCents = 701.955000865;
} // namespace

// ---------------------------------------------------------------------------
namespace edo
{
    int stepsFromCents (double cents, int divisions) noexcept
    {
        const double steps = cents * clampDivisions (divisions) / 1200.0;
        // Round half down so that exact ties pick the smaller interval.
        return static_cast<int> (std::ceil (steps - 0.5));
    }

    int convert (int steps, int fromDivisions, int toDivisions) noexcept
    {
        fromDivisions = clampDivisions (fromDivisions);
        toDivisions = clampDivisions (toDivisions);
        if (fromDivisions == toDivisions)
            return steps;
        if (toDivisions % fromDivisions == 0) // exact (12 -> 24, 36, 72...)
            return steps * (toDivisions / fromDivisions);
        return stepsFromCents (1200.0 * steps / fromDivisions, toDivisions);
    }

    int fifth (int divisions) noexcept
    {
        return static_cast<int> (std::lround (kJustFifthCents * clampDivisions (divisions) / 1200.0));
    }
} // namespace edo

// ---------------------------------------------------------------------------
std::optional<PitchClass> PitchClass::fromName (std::string_view name, int divisions)
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

    return PitchClass (edo::fromSemitones (mod12 (base), divisions), divisions);
}

PitchClass PitchClass::convertedTo (int newDivisions) const noexcept
{
    return PitchClass (edo::convert (v, divisions(), newDivisions), newDivisions);
}

std::string PitchClass::name (bool preferFlats) const
{
    const auto& names = preferFlats ? kFlatNames : kSharpNames;

    if (divisions() == kDefaultDivisions)
        return names[v];

    const double cents = equalCents();
    const int nearest = static_cast<int> (std::ceil (cents / 100.0 - 0.5)); // ties go down: "C+50"
    const int offset = static_cast<int> (std::lround (cents - nearest * 100.0));
    std::string s = names[static_cast<std::size_t> (mod12 (nearest))];
    if (offset != 0)
        s += (offset > 0 ? "+" : "") + std::to_string (offset);
    return s;
}

// ---------------------------------------------------------------------------
PitchClassSet::PitchClassSet (std::initializer_list<int> pcs) noexcept
{
    for (const int pc : pcs)
        insert (PitchClass (pc));
}

PitchClassSet::PitchClassSet (int divisions, std::initializer_list<int> steps) noexcept
    : n (static_cast<std::uint8_t> (clampDivisions (divisions) - 1))
{
    for (const int s : steps)
        insert (PitchClass (s, divisions));
}

PitchClassSet PitchClassSet::withDivisions (int divisions) noexcept
{
    PitchClassSet s;
    s.n = static_cast<std::uint8_t> (clampDivisions (divisions) - 1);
    return s;
}

PitchClassSet PitchClassSet::fromMidiNotes (const std::vector<int>& midiNotes) noexcept
{
    return fromNotes (midiNotes, kDefaultDivisions);
}

PitchClassSet PitchClassSet::fromNotes (const std::vector<int>& notes, int divisions) noexcept
{
    auto s = withDivisions (divisions);
    for (const int note : notes)
        s.insert (PitchClass::fromNote (note, divisions));
    return s;
}

int PitchClassSet::index (PitchClass pc) const noexcept
{
    assert (pc.divisions() == divisions() && "PitchClass from another division count");
    if (pc.divisions() != divisions())
        pc = pc.convertedTo (divisions());
    return pc.value();
}

int PitchClassSet::size() const noexcept
{
    return std::popcount (words[0]) + std::popcount (words[1]);
}

bool PitchClassSet::contains (PitchClass pc) const noexcept
{
    const int i = index (pc);
    return (words[static_cast<std::size_t> (i >> 6)] >> (i & 63)) & 1u;
}

void PitchClassSet::insert (PitchClass pc) noexcept
{
    const int i = index (pc);
    words[static_cast<std::size_t> (i >> 6)] |= std::uint64_t { 1 } << (i & 63);
}

void PitchClassSet::erase (PitchClass pc) noexcept
{
    const int i = index (pc);
    words[static_cast<std::size_t> (i >> 6)] &= ~(std::uint64_t { 1 } << (i & 63));
}

void PitchClassSet::toggle (PitchClass pc) noexcept
{
    const int i = index (pc);
    words[static_cast<std::size_t> (i >> 6)] ^= std::uint64_t { 1 } << (i & 63);
}

PitchClassSet PitchClassSet::transposed (int steps) const noexcept
{
    auto s = withDivisions (divisions());
    for (const auto pc : toVector())
        s.insert (pc.transposed (steps));
    return s;
}

PitchClassSet PitchClassSet::inverted() const noexcept
{
    auto s = withDivisions (divisions());
    for (const auto pc : toVector())
        s.insert (pc.inverted());
    return s;
}

std::vector<PitchClass> PitchClassSet::toVector() const
{
    std::vector<PitchClass> out;
    const int d = divisions();
    for (int i = 0; i < d; ++i)
        if ((words[static_cast<std::size_t> (i >> 6)] >> (i & 63)) & 1u)
            out.emplace_back (i, d);
    return out;
}

std::vector<PitchClass> PitchClassSet::toFifthsOrder() const
{
    const int d = divisions();
    std::vector<PitchClass> out;
    auto seen = withDivisions (d);

    for (int pos = 0, count = fifths::size (d); pos < count; ++pos)
    {
        const auto pc = fifths::atPosition (pos, d);
        seen.insert (pc);
        if (contains (pc))
            out.push_back (pc);
    }

    for (const auto pc : toVector())
        if (! seen.contains (pc))
            out.push_back (pc);

    return out;
}

// ---------------------------------------------------------------------------
namespace fifths
{
    int size (int divisions) noexcept
    {
        divisions = clampDivisions (divisions);
        const int g = std::gcd (edo::fifth (divisions), divisions);
        return divisions / (g == 0 ? divisions : g);
    }

    PitchClass atPosition (int position, int divisions) noexcept
    {
        return PitchClass (modN (position, size (divisions)) * edo::fifth (divisions), divisions);
    }

    int positionOf (PitchClass pc) noexcept
    {
        const int d = pc.divisions();
        const int count = size (d);
        const int fifth = edo::fifth (d);
        for (int pos = 0; pos < count; ++pos)
            if (modN (pos * fifth, d) == pc.value())
                return pos;
        return -1;
    }

    int distance (PitchClass a, PitchClass b) noexcept
    {
        const int pa = positionOf (a);
        const int pb = positionOf (b);
        if (pa < 0 || pb < 0)
            return 0;
        const int count = size (a.divisions());
        const int d = modN (pb - pa, count);
        return d > count / 2 ? d - count : d;
    }
} // namespace fifths

} // namespace mbox::theory
