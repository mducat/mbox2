#include "mbox/theory/Voicing.h"

#include <algorithm>
#include <cstdlib>
#include <limits>

namespace mbox::theory
{

namespace
{
    // Total movement between two sets of voices. Same size: voice by voice;
    // otherwise each note is matched with the nearest note of the other chord.
    int movement (const std::vector<int>& a, const std::vector<int>& b)
    {
        if (a.size() == b.size())
        {
            int sum = 0;
            for (std::size_t i = 0; i < a.size(); ++i)
                sum += std::abs (a[i] - b[i]);
            return sum;
        }

        auto nearestSum = [] (const std::vector<int>& from, const std::vector<int>& to)
        {
            int sum = 0;
            for (const int x : from)
            {
                int best = std::numeric_limits<int>::max();
                for (const int y : to)
                    best = std::min (best, std::abs (x - y));
                sum += best;
            }
            return sum;
        };
        return nearestSum (a, b) + nearestSum (b, a);
    }
} // namespace

std::vector<int> GrandStaffVoicing::midiNotes() const
{
    std::vector<int> notes { bass.midi() };
    for (const auto& n : upper)
        notes.push_back (n.midi());
    return notes;
}

GrandStaffVoicing KeyboardVoicer::next (const std::vector<SpelledPitchClass>& tones, int inversion)
{
    GrandStaffVoicing result;
    if (tones.empty())
        return result;

    const int n = static_cast<int> (tones.size());

    // --- Bass ---------------------------------------------------------------
    const auto& bassTone = tones[static_cast<std::size_t> (std::clamp (inversion, 0, n - 1))];
    const int bassTarget = hasPrevious ? previous.bass.midi() : (ranges.bassLow + ranges.bassHigh) / 2;

    int bestBass = -1;
    for (int m = ranges.bassLow; m <= ranges.bassHigh; ++m)
        if (PitchClass::fromMidi (m) == bassTone.pitchClass()
            && (bestBass < 0 || std::abs (m - bassTarget) < std::abs (bestBass - bassTarget)))
            bestBass = m;

    result.bass = SpelledNote::fromMidi (bestBass, bassTone);

    // --- Upper voices: close position, best rotation ------------------------
    std::vector<int> previousUpper;
    for (const auto& note : previous.upper)
        previousUpper.push_back (note.midi());

    int bestCost = std::numeric_limits<int>::max();

    for (int rotation = 0; rotation < n; ++rotation)
    {
        for (int start = ranges.upperLow; start <= ranges.upperHigh; ++start)
        {
            if (PitchClass::fromMidi (start) != tones[static_cast<std::size_t> (rotation)].pitchClass())
                continue;

            std::vector<int> midi { start };
            std::vector<SpelledNote> notes { SpelledNote::fromMidi (start, tones[static_cast<std::size_t> (rotation)]) };

            for (int k = 1; k < n; ++k)
            {
                const auto& tone = tones[static_cast<std::size_t> ((rotation + k) % n)];
                const int m = midi.back() + 1 + PitchClass::fromMidi (midi.back() + 1).intervalTo (tone.pitchClass());
                midi.push_back (m);
                notes.push_back (SpelledNote::fromMidi (m, tone));
            }

            int cost = 0;
            if (hasPrevious)
            {
                cost = movement (midi, previousUpper);
            }
            else
            {
                int sum = 0;
                for (const int m : midi)
                    sum += m;
                cost = std::abs (sum - ranges.upperCentre * static_cast<int> (midi.size()));
            }

            if (cost < bestCost)
            {
                bestCost = cost;
                result.upper = std::move (notes);
            }
        }
    }

    previous = result;
    hasPrevious = true;
    return result;
}

} // namespace mbox::theory
