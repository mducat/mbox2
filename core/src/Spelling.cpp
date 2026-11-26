#include "mbox/theory/Spelling.h"

#include <algorithm>
#include <array>
#include <cstdlib>

namespace mbox::theory
{

namespace
{
    // Position of each natural letter on the line of fifths (F C G D A E B = -1..5).
    constexpr std::array kLetterFifths { 0, 2, 4, -1, 1, 3, 5 };

    // Order in which sharps are added to a key signature (F C G D A E B); flats use the reverse.
    constexpr std::array kSharpOrder { Letter::F, Letter::C, Letter::G, Letter::D, Letter::A, Letter::E, Letter::B };

    // Chromatic degree (0..6) of each semitone above the tonic: 1 = b2, 3 = b3, 6 = #4, 8 = b6, 10 = b7.
    constexpr std::array kDegreeOfSemitone { 0, 1, 1, 2, 2, 3, 3, 4, 5, 5, 6, 6 };

    // Default generic size (letters spanned) of each chord interval, by semitones mod 12.
    constexpr std::array kGenericOfSemitone { 0, 1, 1, 2, 2, 3, 4, 4, 4, 5, 6, 6 };

    int floorDiv (int a, int b) noexcept
    {
        const int q = a / b;
        return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
    }
} // namespace

// ---------------------------------------------------------------------------

int SpelledPitchClass::lineOfFifths() const noexcept
{
    return kLetterFifths[static_cast<std::size_t> (letter)] + 7 * alter;
}

std::string SpelledPitchClass::name() const
{
    std::string s (1, "CDEFGAB"[static_cast<int> (letter)]);
    if (alter > 0)
    {
        // Double sharps are written "x" (Fx), as on the staff.
        for (int i = 0; i < alter / 2; ++i) s += 'x';
        if (alter % 2 != 0)                 s += '#';
    }
    for (int i = 0; i < -alter; ++i)
        s += 'b';
    return s;
}

SpelledPitchClass SpelledPitchClass::withLetter (Letter l, PitchClass pc) noexcept
{
    int d = mod12 (pc.value() - naturalPitchClass (l));
    if (d > 5)
        d -= 12;
    return { l, d };
}

SpelledNote SpelledNote::fromMidi (int midiNote, SpelledPitchClass spelling) noexcept
{
    const int c = midiNote - naturalPitchClass (spelling.letter) - spelling.alter;
    return { spelling, floorDiv (c, 12) - 1 };
}

// ---------------------------------------------------------------------------

int KeySignature::alterationOf (Letter l) const noexcept
{
    const auto it = std::find (kSharpOrder.begin(), kSharpOrder.end(), l);
    const int sharpIndex = static_cast<int> (it - kSharpOrder.begin());

    if (fifthsCount > 0 && sharpIndex < fifthsCount)
        return 1;
    if (fifthsCount < 0 && (kNumLetters - 1 - sharpIndex) < -fifthsCount)
        return -1;
    return 0;
}

std::vector<Letter> KeySignature::accidentalLetters() const
{
    std::vector<Letter> result;
    const int n = std::min (std::abs (fifthsCount), kNumLetters);
    for (int i = 0; i < n; ++i)
        result.push_back (fifthsCount > 0 ? kSharpOrder[static_cast<std::size_t> (i)]
                                          : kSharpOrder[static_cast<std::size_t> (kNumLetters - 1 - i)]);
    return result;
}

// ---------------------------------------------------------------------------

Key Key::fromPitchClass (PitchClass tonic, bool minor)
{
    Key best { SpelledPitchClass::withLetter (Letter::C, tonic), minor };
    int bestFifths = 1000;

    for (int i = 0; i < kNumLetters; ++i)
    {
        const Key candidate { SpelledPitchClass::withLetter (letterAt (i), tonic), minor };
        if (std::abs (candidate.tonic.alter) > 1)
            continue;

        const int f = candidate.signature().fifths();
        if (std::abs (f) < std::abs (bestFifths) || (std::abs (f) == std::abs (bestFifths) && f < bestFifths))
        {
            best = candidate;
            bestFifths = f;
        }
    }
    return best;
}

KeySignature Key::signature() const noexcept
{
    // A minor key shares the signature of its relative major, three fifths higher.
    return KeySignature (tonic.lineOfFifths() - (minor ? 3 : 0));
}

SpelledPitchClass Key::spellDegree (PitchClass pc) const noexcept
{
    const int semis = tonic.pitchClass().intervalTo (pc);
    const auto degree = kDegreeOfSemitone[static_cast<std::size_t> (semis)];
    return SpelledPitchClass::withLetter (letterAt (static_cast<int> (tonic.letter) + degree), pc);
}

// ---------------------------------------------------------------------------

int genericInterval (const ChordQuality& quality, int intervalIndex) noexcept
{
    const int semis = quality.intervals[static_cast<std::size_t> (intervalIndex)];
    const int octaves = floorDiv (semis, 12);
    int generic = kGenericOfSemitone[static_cast<std::size_t> (mod12 (semis))];

    // 9 semitones is a sixth (C6, Cm6) except over a diminished fifth, where it is
    // a diminished seventh (Cdim7 = C Eb Gb Bbb).
    if (mod12 (semis) == 9)
    {
        const auto& iv = quality.intervals;
        if (std::find (iv.begin(), iv.end(), 6) != iv.end())
            generic = 6;
    }
    return octaves * kNumLetters + generic;
}

std::vector<SpelledPitchClass> spellChord (const Chord& chord, SpelledPitchClass root)
{
    const auto& quality = chord.quality();
    std::vector<SpelledPitchClass> result;
    result.reserve (quality.intervals.size());

    for (std::size_t i = 0; i < quality.intervals.size(); ++i)
    {
        const auto letter = letterAt (static_cast<int> (root.letter) + genericInterval (quality, static_cast<int> (i)));
        result.push_back (SpelledPitchClass::withLetter (letter, root.pitchClass().transposed (quality.intervals[i])));
    }
    return result;
}

std::vector<SpelledPitchClass> spellChord (const Chord& chord, const Key& key)
{
    return spellChord (chord, key.spellDegree (chord.root()));
}

} // namespace mbox::theory
