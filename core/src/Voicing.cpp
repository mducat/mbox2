#include "mbox/theory/Voicing.h"

#include <algorithm>
#include <cmath>
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

// ---------------------------------------------------------------------------

namespace
{
    // Range of the voicings, as fractions of an octave above note 0 (12-TET: MIDI).
    int scaled (int midi, int divisions) { return static_cast<int> (std::lround (midi * divisions / 12.0)); }
}

std::vector<std::vector<int>> keyboardVoicings (const Chord& chord, int bassNote)
{
    const int n = chord.divisions();
    const auto steps = chord.steps();
    if (steps.empty())
        return {};

    const int lowestUpperMin = std::max (bassNote + 1, scaled (55, n)); // G3
    const int lowestUpperMax = scaled (72, n);                          // C5
    const int topMax = scaled (84, n);                                  // C6

    std::vector<int> classes;
    for (const int s : steps)
        classes.push_back (modN (chord.root().value() + s, n));
    const int numTones = static_cast<int> (classes.size());

    std::vector<std::vector<int>> result;
    auto consider = [&] (std::vector<int> upper)
    {
        std::sort (upper.begin(), upper.end());
        if (upper.front() <= bassNote || upper.front() < scaled (48, n) || upper.back() > topMax)
            return;
        std::vector<int> v { bassNote };
        v.insert (v.end(), upper.begin(), upper.end());
        if (std::find (result.begin(), result.end(), v) == result.end())
            result.push_back (std::move (v));
    };

    for (int rotation = 0; rotation < numTones; ++rotation)
    {
        const int firstClass = classes[static_cast<std::size_t> (rotation)];
        for (int start = lowestUpperMin; start <= lowestUpperMax; ++start)
        {
            if (modN (start, n) != firstClass)
                continue;

            // Close position from `start`, stacking the next chord tones upwards.
            std::vector<int> close { start };
            for (int k = 1; k < numTones; ++k)
            {
                const int pc = classes[static_cast<std::size_t> ((rotation + k) % numTones)];
                close.push_back (close.back() + 1 + modN (pc - (close.back() + 1), n));
            }
            consider (close);

            // Drop 2 / drop 3: the 2nd or 3rd voice from the top goes down an octave.
            for (const int drop : { 2, 3 })
            {
                if (numTones < drop + 1)
                    continue;
                auto open = close;
                open[static_cast<std::size_t> (numTones - drop)] -= n;
                consider (open);
            }
        }
    }

    auto mean = [] (const std::vector<int>& v)
    {
        double sum = 0.0;
        for (const int x : v) sum += x;
        return sum / static_cast<double> (v.size());
    };
    std::stable_sort (result.begin(), result.end(), [&] (const auto& a, const auto& b) { return mean (a) < mean (b); });
    return result;
}

int closestVoicingIndex (const std::vector<std::vector<int>>& voicings, const std::vector<int>& notes)
{
    int best = -1;
    int bestCost = std::numeric_limits<int>::max();
    for (std::size_t i = 0; i < voicings.size(); ++i)
    {
        if (voicings[i] == notes)
            return static_cast<int> (i);
        const int cost = movement (voicings[i], notes);
        if (cost < bestCost)
        {
            bestCost = cost;
            best = static_cast<int> (i);
        }
    }
    return best;
}

} // namespace mbox::theory
