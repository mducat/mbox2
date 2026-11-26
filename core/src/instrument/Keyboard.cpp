#include "mbox/instrument/Keyboard.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace mbox::instrument
{

int pitchFromTwelveTet (int midiNote, int stepsPerOctave) noexcept
{
    return static_cast<int> (std::lround (static_cast<double> (midiNote) * stepsPerOctave / 12.0));
}

KeyboardLayout KeyboardLayout::forEqualTemperament (int stepsPerOctave)
{
    KeyboardLayout layout;
    layout.steps = std::max (1, stepsPerOctave);
    const int n = layout.steps;

    // Lower row: the steps nearest to the diatonic notes.
    std::vector<int> lower;
    for (const int semitone : std::array { 0, 2, 4, 5, 7, 9, 11 })
    {
        const int step = wrap (pitchFromTwelveTet (semitone, n), n);
        if (std::find (lower.begin(), lower.end(), step) == lower.end())
            lower.push_back (step);
    }
    std::sort (lower.begin(), lower.end());
    layout.lowerPerOctave = static_cast<int> (lower.size());

    layout.keys.resize (static_cast<std::size_t> (n));
    for (int s = 0; s < n; ++s)
        layout.keys[static_cast<std::size_t> (s)].step = s;

    for (std::size_t i = 0; i < lower.size(); ++i)
    {
        auto& k = layout.keys[static_cast<std::size_t> (lower[i])];
        k.upper = false;
        k.x = static_cast<double> (i);
        k.width = 1.0;

        // Raised keys between this lower key and the next one (wrapping to the next octave).
        const int from = lower[i] + 1;
        const int to   = (i + 1 < lower.size()) ? lower[i + 1] : n + lower[0];
        const int count = to - from;
        if (count <= 0)
            continue;

        const double total = std::min (0.95, 0.6 + 0.15 * (count - 1));
        const double each  = total / count;
        const double left  = static_cast<double> (i + 1) - total / 2.0;
        for (int j = 0; j < count; ++j)
        {
            auto& up = layout.keys[static_cast<std::size_t> (wrap (from + j, n))];
            up.upper = true;
            up.width = each;
            up.x = left + each * j;
        }
    }

    return layout;
}

KeyGeometry KeyboardLayout::keyForPitch (int pitch) const noexcept
{
    const int octave = (pitch - wrap (pitch, steps)) / steps;
    KeyGeometry k = key (pitch);
    k.x += static_cast<double> (octave * lowerPerOctave);
    return k;
}

int KeyboardLayout::pitchAt (double x, double y, double upperHeight, int firstPitch, int lastPitch) const noexcept
{
    if (y < upperHeight)
        for (int p = firstPitch; p <= lastPitch; ++p)
            if (const auto k = keyForPitch (p); k.upper && x >= k.x && x < k.x + k.width)
                return p;

    for (int p = firstPitch; p <= lastPitch; ++p)
        if (const auto k = keyForPitch (p); ! k.upper && x >= k.x && x < k.x + k.width)
            return p;

    return -1;
}

} // namespace mbox::instrument
