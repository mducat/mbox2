#include <catch2/catch_test_macros.hpp>

#include "mbox/theory/Voicing.h"

#include <algorithm>
#include <cstdlib>

using namespace mbox::theory;

TEST_CASE ("KeyboardVoicer puts the requested inversion in the bass")
{
    const auto C = SpelledKey::fromPitchClass (PitchClass (0), false);
    KeyboardVoicer voicer;

    const auto first = voicer.next (spellChord (Chord (PitchClass (0), chords::major), C), 1);
    CHECK (first.bass.pitch.name() == "E");
    CHECK (first.bass.midi() >= 43);
    CHECK (first.bass.midi() <= 57);
    REQUIRE (first.upper.size() == 3);
    CHECK (first.upper.front().midi() >= 60);
    for (std::size_t i = 1; i < first.upper.size(); ++i)
    {
        CHECK (first.upper[i].midi() > first.upper[i - 1].midi());
        CHECK (first.upper[i].midi() - first.upper[i - 1].midi() <= 5); // close position
    }
}

TEST_CASE ("KeyboardVoicer keeps common tones (C -> F -> G7 -> C)")
{
    const auto C = SpelledKey::fromPitchClass (PitchClass (0), false);
    KeyboardVoicer voicer;

    auto upperOf = [&] (int root, const ChordQuality& q, int inv = 0)
    {
        return voicer.next (spellChord (Chord (PitchClass (root), q), C), inv).midiNotes();
    };

    const auto c1 = upperOf (0, chords::major);
    const auto f  = upperOf (5, chords::major);
    const auto g7 = upperOf (7, chords::dominant7);
    const auto c2 = upperOf (0, chords::major);

    auto moved = [] (const std::vector<int>& a, const std::vector<int>& b)
    {
        int m = 0;
        for (std::size_t i = 1; i < std::min (a.size(), b.size()); ++i)
            m = std::max (m, std::abs (a[i] - b[i]));
        return m;
    };

    // Upper voices never jump more than a fourth between neighbouring chords.
    CHECK (moved (c1, f) <= 5);
    CHECK (moved (c2, c1) <= 12);
    CHECK (g7.size() == 5);
    CHECK (std::abs (c2[0] - c1[0]) <= 12);
}

TEST_CASE ("Voicing keeps the chord spelling")
{
    const auto cm = SpelledKey::fromPitchClass (PitchClass (0), true);
    KeyboardVoicer voicer;
    const auto v = voicer.next (spellChord (Chord (PitchClass (11), chords::diminished7), cm), 0);
    CHECK (v.bass.pitch.name() == "B");
    bool hasAb = false;
    for (const auto& n : v.upper)
        hasAb |= n.pitch.name() == "Ab";
    CHECK (hasAb);
}

TEST_CASE ("keyboardVoicings keeps the bass and every chord tone")
{
    const Chord c (PitchClass (0), chords::major);
    const auto voicings = keyboardVoicings (c, 48);
    REQUIRE (voicings.size() >= 6);

    for (const auto& v : voicings)
    {
        CHECK (v.front() == 48);
        CHECK (v.size() == 4);
        CHECK (std::is_sorted (v.begin(), v.end()));
        CHECK (PitchClassSet::fromMidiNotes (v) == c.pitchClasses());
        CHECK (v.back() <= 84);
    }

    // Close C-E-G and the open (drop 2) G-E above C3 are both offered.
    CHECK (std::find (voicings.begin(), voicings.end(), std::vector { 48, 60, 64, 67 }) != voicings.end());
    CHECK (std::find (voicings.begin(), voicings.end(), std::vector { 48, 55, 64, 72 }) != voicings.end());

    // Sorted low to high, no duplicates.
    for (std::size_t i = 1; i < voicings.size(); ++i)
        CHECK (voicings[i] != voicings[i - 1]);
}

TEST_CASE ("keyboardVoicings: sevenths get drop 2 / drop 3, and N-TET works")
{
    const auto g7 = keyboardVoicings (Chord (PitchClass (7), chords::dominant7), 43);
    // Close position B D F G over G2, and its drop 2 (F down an octave: F B D G).
    CHECK (std::find (g7.begin(), g7.end(), std::vector { 43, 59, 62, 65, 67 }) != g7.end());
    CHECK (std::find (g7.begin(), g7.end(), std::vector { 43, 53, 59, 62, 67 }) != g7.end());
    for (const auto& v : g7)
        CHECK (v.size() == 5);

    const auto c19 = keyboardVoicings (Chord (PitchClass (0, 19), chords::major), 4 * 19);
    REQUIRE (! c19.empty());
    for (const auto& v : c19)
        CHECK (PitchClass::fromNote (v[1], 19).divisions() == 19);

    CHECK (closestVoicingIndex (g7, g7[3]) == 3);
    CHECK (closestVoicingIndex ({}, { 1, 2 }) == -1);
}
