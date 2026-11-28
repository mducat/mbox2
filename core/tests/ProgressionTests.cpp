#include <catch2/catch_test_macros.hpp>

#include "mbox/theory/Progressions.h"

using namespace mbox::theory;

namespace
{
    const Key cMajor { PitchClass (0), Mode::Major };
    const Key aMinor { PitchClass (9), Mode::Minor };

    RomanChord chord (const Key& k, const char* numeral)
    {
        auto c = findChord (k, numeral);
        REQUIRE (c.has_value());
        return *c;
    }

    std::optional<MotionStrength> motion (const Key& k, const char* a, const char* b)
    {
        return motionStrength (chord (k, a), chord (k, b));
    }
} // namespace

TEST_CASE ("findChord resolves numerals with inversions")
{
    CHECK (chord (cMajor, "ii6").bass() == PitchClass (5));
    CHECK (chord (cMajor, "V6/5/V").symbol() == "D7/F#");
    CHECK (chord (cMajor, "I6/4").origin == ChordOrigin::Cadential);
    CHECK (chord (cMajor, "N6").special == RomanChord::Special::Neapolitan);
    CHECK (chord (aMinor, "VII").symbol() == "G");
    CHECK_FALSE (findChord (cMajor, "XIV").has_value());
}

TEST_CASE ("ii - V - I: strong functional motions")
{
    CHECK (motion (cMajor, "ii", "V") == MotionStrength::Strong);
    CHECK (motion (cMajor, "V7", "I") == MotionStrength::Strong);
    CHECK (motion (cMajor, "vii\xC2\xB0", "I") == MotionStrength::Strong);
    CHECK (motion (cMajor, "vi", "ii") == MotionStrength::Strong);
    CHECK (motion (cMajor, "IV", "V") == MotionStrength::Strong);
    CHECK (motion (cMajor, "V7/V", "V") == MotionStrength::Strong);
    CHECK (motion (cMajor, "V7/V", "I6/4") == MotionStrength::Strong);
    CHECK (motion (cMajor, "I6/4", "V7") == MotionStrength::Strong);
    CHECK (motion (cMajor, "Ger+6", "V") == MotionStrength::Strong);
    CHECK (motion (aMinor, "ii\xC3\xB8" "7", "V7") == MotionStrength::Strong);
    CHECK (motion (aMinor, "VII", "III") == MotionStrength::Strong);
}

TEST_CASE ("Common, deceptive and unusual motions")
{
    CHECK (motion (cMajor, "IV", "I") == MotionStrength::Common);   // plagal
    CHECK (motion (cMajor, "I", "vi") == MotionStrength::Common);
    CHECK (motion (cMajor, "I", "IV") == MotionStrength::Common);
    CHECK (motion (aMinor, "i", "V7") == MotionStrength::Common);
    CHECK_FALSE (motion (cMajor, "I", "vii\xC2\xB0").has_value());
    CHECK_FALSE (motion (cMajor, "I", "bVI").has_value());
    CHECK_FALSE (motion (aMinor, "i", "v").has_value());
    CHECK_FALSE (motion (cMajor, "ii", "bVII").has_value());
    CHECK_FALSE (motion (cMajor, "ii", "N6").has_value());
    CHECK (motion (aMinor, "iv", "N6") == MotionStrength::Common);
    CHECK (motion (cMajor, "vi", "IV") == MotionStrength::Common);
    CHECK (motion (cMajor, "IV", "ii") == MotionStrength::Common);
    CHECK (motion (cMajor, "IV", "iv") == MotionStrength::Common);
    CHECK (motion (cMajor, "V", "vi") == MotionStrength::Deceptive);
    CHECK (motion (aMinor, "V7", "VI") == MotionStrength::Deceptive);

    CHECK_FALSE (motion (cMajor, "V", "IV").has_value());   // retrogression
    CHECK_FALSE (motion (cMajor, "V7/V", "I").has_value());
    CHECK_FALSE (motion (cMajor, "I", "I6/4").has_value());
    CHECK_FALSE (motion (cMajor, "I", "V7/V").has_value());
    CHECK_FALSE (motion (cMajor, "V", "V").has_value());
}

TEST_CASE ("Every common progression resolves in every key")
{
    for (const auto mode : { Mode::Major, Mode::Minor })
        for (int tonic = 0; tonic < 12; ++tonic)
        {
            const Key key { PitchClass (tonic), mode };
            for (const auto& p : commonProgressions (mode))
            {
                INFO (key.name() << ": " << p.name);
                const auto chords = realise (key, p);
                CHECK (chords.size() == p.numerals.size());
                for (std::size_t i = 0; i < chords.size(); ++i)
                    CHECK (chords[i].numeral() == p.numerals[i]);
            }
        }

    const auto& major = commonProgressions (Mode::Major);
    REQUIRE_FALSE (major.empty());
    const auto iiVI = realise (cMajor, major.front());
    REQUIRE (iiVI.size() == 3);
    CHECK (iiVI[0].symbol() == "Dm");
    CHECK (iiVI[1].symbol() == "G");
    CHECK (iiVI[2].symbol() == "C");
}
