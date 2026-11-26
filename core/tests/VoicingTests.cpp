#include <catch2/catch_test_macros.hpp>

#include "mbox/theory/Voicing.h"

#include <cstdlib>

using namespace mbox::theory;

TEST_CASE ("KeyboardVoicer puts the requested inversion in the bass")
{
    const auto C = Key::fromPitchClass (PitchClass (0), false);
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
    const auto C = Key::fromPitchClass (PitchClass (0), false);
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
    const auto cm = Key::fromPitchClass (PitchClass (0), true);
    KeyboardVoicer voicer;
    const auto v = voicer.next (spellChord (Chord (PitchClass (11), chords::diminished7), cm), 0);
    CHECK (v.bass.pitch.name() == "B");
    bool hasAb = false;
    for (const auto& n : v.upper)
        hasAb |= n.pitch.name() == "Ab";
    CHECK (hasAb);
}
