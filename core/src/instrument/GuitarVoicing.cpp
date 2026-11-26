#include "mbox/instrument/GuitarVoicing.h"

#include <algorithm>
#include <climits>

namespace mbox::instrument
{

std::vector<int> GuitarVoicing::pitches (const Fretboard& board) const
{
    std::vector<int> result;
    for (std::size_t s = 0; s < frets.size(); ++s)
        if (frets[s] >= 0)
            result.push_back (board.pitchAt (static_cast<int> (s), frets[s]));
    return result;
}

int GuitarVoicing::lowestFret() const noexcept
{
    int lowest = INT_MAX;
    for (const int f : frets)
        if (f > 0)
            lowest = std::min (lowest, f);
    return lowest == INT_MAX ? 0 : lowest;
}

int GuitarVoicing::highestFret() const noexcept
{
    int highest = 0;
    for (const int f : frets)
        highest = std::max (highest, f);
    return highest;
}

int GuitarVoicing::numSounding() const noexcept
{
    return static_cast<int> (std::count_if (frets.begin(), frets.end(), [] (int f) { return f >= 0; }));
}

namespace
{
    int fingersNeeded (const std::vector<int>& frets)
    {
        int lowest = INT_MAX, fretted = 0, onLowest = 0;
        for (const int f : frets)
            if (f > 0)
            {
                ++fretted;
                lowest = std::min (lowest, f);
            }
        for (const int f : frets)
            if (f > 0 && f == lowest)
                ++onLowest;

        // Several notes on the lowest fret are played with one barre finger.
        return onLowest > 1 ? fretted - onLowest + 1 : fretted;
    }

    struct Search
    {
        const Fretboard& board;
        const std::vector<int>& classes;
        std::optional<int> bass;
        const VoicingOptions& options;
        int n;
        int windowStart = 1;
        int windowEnd = 1;
        std::vector<int> current;
        std::vector<std::vector<int>> found;

        bool isChordTone (int pitch) const
        {
            return std::find (classes.begin(), classes.end(), wrap (pitch, n)) != classes.end();
        }

        void accept()
        {
            const int numStrings = board.numStrings();
            int sounding = 0, first = -1, last = -1;
            int lowestPitch = INT_MAX;
            unsigned covered = 0;
            std::vector<bool> coveredClass (classes.size(), false);

            for (int s = 0; s < numStrings; ++s)
            {
                const int f = current[static_cast<std::size_t> (s)];
                if (f < 0)
                    continue;
                ++sounding;
                if (first < 0) first = s;
                last = s;
                const int p = board.pitchAt (s, f);
                lowestPitch = std::min (lowestPitch, p);
                for (std::size_t c = 0; c < classes.size(); ++c)
                    if (! coveredClass[c] && wrap (classes[c], n) == wrap (p, n))
                    {
                        coveredClass[c] = true;
                        ++covered;
                    }
            }

            if (covered != classes.size())
                return;
            if (sounding < std::min (options.minSounding, numStrings))
                return;
            if (options.contiguous && last - first + 1 != sounding)
                return;
            if (bass && wrap (lowestPitch, n) != wrap (*bass, n))
                return;
            if (fingersNeeded (current) > options.maxFingers)
                return;

            found.push_back (current);
        }

        void recurse (int string)
        {
            if (string == board.numStrings())
            {
                accept();
                return;
            }

            auto& slot = current[static_cast<std::size_t> (string)];

            slot = -1;
            recurse (string + 1);

            if (options.allowOpenStrings && isChordTone (board.pitchAt (string, 0)))
            {
                slot = 0;
                recurse (string + 1);
            }

            for (int f = std::max (1, windowStart); f <= std::min (windowEnd, board.numFrets()); ++f)
                if (isChordTone (board.pitchAt (string, f)))
                {
                    slot = f;
                    recurse (string + 1);
                }

            slot = -1;
        }
    };

    int score (const GuitarVoicing& v, int numStrings)
    {
        return v.highestFret() * 3 + (numStrings - v.numSounding()) * 2 + fingersNeeded (v.frets);
    }
} // namespace

std::vector<GuitarVoicing> findVoicings (const Fretboard& board,
                                         const std::vector<int>& pitchClasses,
                                         std::optional<int> bassClass,
                                         const VoicingOptions& options)
{
    if (pitchClasses.empty() || board.numStrings() == 0)
        return {};

    const int n = board.stepsPerOctave();
    const int span = std::max (1, pitchFromTwelveTet (options.maxSpan, n));
    const int maxStart = std::max (1, pitchFromTwelveTet (options.maxFret, n));

    // Deduplicated pitch classes; a chord with more classes than strings cannot be played.
    std::vector<int> classes;
    for (const int pc : pitchClasses)
        if (std::find (classes.begin(), classes.end(), wrap (pc, n)) == classes.end())
            classes.push_back (wrap (pc, n));
    if (static_cast<int> (classes.size()) > board.numStrings())
        return {};

    Search search { board, classes, bassClass, options, n, 1, 1, {}, {} };
    search.current.assign (static_cast<std::size_t> (board.numStrings()), -1);

    for (int start = 1; start <= maxStart; ++start)
    {
        search.windowStart = start;
        search.windowEnd = start + span - 1;
        search.recurse (0);
    }

    std::sort (search.found.begin(), search.found.end());
    search.found.erase (std::unique (search.found.begin(), search.found.end()), search.found.end());

    std::vector<GuitarVoicing> result;
    result.reserve (search.found.size());
    for (auto& frets : search.found)
        result.push_back ({ std::move (frets) });

    const int numStrings = board.numStrings();
    std::stable_sort (result.begin(), result.end(), [numStrings] (const GuitarVoicing& a, const GuitarVoicing& b) {
        const int sa = score (a, numStrings), sb = score (b, numStrings);
        if (sa != sb)
            return sa < sb;
        return a.lowestFret() < b.lowestFret();
    });

    if (result.size() > options.maxResults)
        result.resize (options.maxResults);
    return result;
}

} // namespace mbox::instrument
