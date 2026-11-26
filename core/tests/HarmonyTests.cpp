#include <catch2/catch_test_macros.hpp>

#include "mbox/theory/Harmony.h"

#include <algorithm>
#include <string>

using namespace mbox::theory;

namespace
{
    const std::string deg = "\xC2\xB0";  // °
    const std::string hdim = "\xC3\xB8"; // ø

    const Key cMajor { PitchClass (0), Mode::Major };
    const Key aMinor { PitchClass (9), Mode::Minor };

    const RomanChord* find (const std::vector<RomanChord>& list, const std::string& numeral)
    {
        const auto it = std::find_if (list.begin(), list.end(),
                                      [&] (const RomanChord& rc) { return rc.numeral() == numeral; });
        return it == list.end() ? nullptr : &*it;
    }
} // namespace

TEST_CASE ("Key names and signatures")
{
    CHECK (cMajor.name() == "C major");
    CHECK (aMinor.name() == "A minor");
    CHECK (Key { PitchClass (10), Mode::Major }.name() == "Bb major");
    CHECK (Key { PitchClass (2), Mode::Minor }.prefersFlats());     // D minor: 1 flat
    CHECK_FALSE (Key { PitchClass (6), Mode::Minor }.prefersFlats()); // F# minor: 3 sharps
    CHECK (aMinor.parallel() == Key { PitchClass (9), Mode::Major });
    CHECK (aMinor.degree (2) == PitchClass (0));
}

TEST_CASE ("Diatonic chords of C major")
{
    HarmonyOptions o;
    o.secondaryDominants = o.borrowedChords = o.chromaticChords = o.cadentialSixFour = false;
    const auto list = chordsInKey (cMajor, o);
    REQUIRE (list.size() == 14);

    struct Expect { const char* numeral; const char* symbol; HarmonicFunction f; };
    const Expect expected[] = {
        { "I",  "C",  HarmonicFunction::Tonic },        { "IM7", "Cmaj7", HarmonicFunction::Tonic },
        { "vi", "Am", HarmonicFunction::Tonic },        { "vi7", "Am7",   HarmonicFunction::Tonic },
        { "iii", "Em", HarmonicFunction::Tonic },       { "iii7", "Em7",  HarmonicFunction::Tonic },
        { "IV", "F",  HarmonicFunction::Subdominant },  { "IVM7", "Fmaj7", HarmonicFunction::Subdominant },
        { "ii", "Dm", HarmonicFunction::Subdominant },  { "ii7", "Dm7",   HarmonicFunction::Subdominant },
        { "V",  "G",  HarmonicFunction::Dominant },     { "V7", "G7",     HarmonicFunction::Dominant },
    };

    for (std::size_t i = 0; i < std::size (expected); ++i)
    {
        INFO (expected[i].numeral);
        CHECK (list[i].numeral() == expected[i].numeral);
        CHECK (list[i].symbol() == expected[i].symbol);
        CHECK (list[i].function == expected[i].f);
        CHECK (list[i].origin == ChordOrigin::Diatonic);
    }

    CHECK (list[12].numeral() == "vii" + deg);
    CHECK (list[12].symbol() == "Bdim");
    CHECK (list[13].numeral() == "vii" + hdim + "7");
    CHECK (list[13].symbol() == "Bm7b5");

    CHECK (list[4].alternateFunction == HarmonicFunction::Dominant); // iii
}

TEST_CASE ("Minor keys mix natural and harmonic minor")
{
    const auto list = chordsInKey (aMinor);

    const auto* V7 = find (list, "V7");
    REQUIRE (V7 != nullptr);
    CHECK (V7->symbol() == "E7");
    CHECK (V7->function == HarmonicFunction::Dominant);

    const auto* vii7 = find (list, "vii" + deg + "7");
    REQUIRE (vii7 != nullptr);
    CHECK (vii7->symbol() == "G#dim7");

    REQUIRE (find (list, "v") != nullptr);       // natural minor dominant
    REQUIRE (find (list, "VII") != nullptr);     // subtonic G
    CHECK (find (list, "VII")->symbol() == "G");
    CHECK (find (list, "III")->symbol() == "C");
    CHECK (find (list, "ii" + deg)->function == HarmonicFunction::Subdominant);
    CHECK (find (list, "iv7")->symbol() == "Dm7");
    CHECK (find (list, "I")->origin == ChordOrigin::Borrowed); // Picardy third
}

TEST_CASE ("Inversions: figures, bass and voicing")
{
    const auto list = chordsInKey (cMajor);
    const auto V7 = *find (list, "V7");

    CHECK (V7.withInversion (1).numeral() == "V6/5");
    CHECK (V7.withInversion (2).numeral() == "V4/3");
    CHECK (V7.withInversion (3).numeral() == "V4/2");
    CHECK (V7.withInversion (3).symbol() == "G7/F");
    CHECK (V7.withInversion (9).inversion == 3); // clamped

    const auto I = *find (list, "I");
    CHECK (I.withInversion (1).numeral() == "I6");
    CHECK (I.withInversion (1).bass() == PitchClass (4));
    CHECK (I.withInversion (2).numeral() == "I6/4");
    CHECK (I.withInversion (2).symbol() == "C/G");

    CHECK (I.voicing (48) == std::vector<int> { 48, 52, 55 });
    CHECK (I.withInversion (1).voicing (48) == std::vector<int> { 52, 55, 60 });
    CHECK (V7.withInversion (3).voicing (48) == std::vector<int> { 53, 55, 59, 62 });

    const auto ii7 = *find (list, "ii" + hdim + "7"); // borrowed
    CHECK (ii7.withInversion (1).numeral() == "ii" + hdim + "6/5");
}

TEST_CASE ("Cadential six-four is a dominant-function tonic chord")
{
    const auto minorList = chordsInKey (aMinor);
    const auto* cad = find (minorList, "i6/4");
    REQUIRE (cad != nullptr);
    CHECK (cad->origin == ChordOrigin::Cadential);
    CHECK (cad->function == HarmonicFunction::Dominant);
    CHECK (cad->bass() == PitchClass (4));
}

TEST_CASE ("Secondary dominants")
{
    const auto list = chordsInKey (cMajor);

    const auto* VofV = find (list, "V7/V");
    REQUIRE (VofV != nullptr);
    CHECK (VofV->symbol() == "D7");
    CHECK (VofV->origin == ChordOrigin::Secondary);
    CHECK (VofV->targetDegree == 4);
    CHECK (VofV->function == HarmonicFunction::Dominant);

    CHECK (find (list, "V/vi")->symbol() == "E");
    CHECK (find (list, "V7/IV")->symbol() == "C7");
    CHECK (find (list, "vii" + deg + "7/ii")->symbol() == "C#dim7");
    CHECK (find (list, "vii" + deg + "7/V")->symbol() == "F#dim7");
    CHECK (find (list, "V7/V")->withInversion (1).numeral() == "V6/5/V");

    const auto minorList = chordsInKey (aMinor);
    CHECK (find (minorList, "V7/iv")->symbol() == "A7");
    CHECK (find (minorList, "V7/V")->symbol() == "B7");
    // Applied chords identical to a diatonic one are not repeated (V/III = VII, V/IV = I).
    CHECK (find (minorList, "V/III") == nullptr);
    CHECK (find (minorList, "V7/III") == nullptr);
    CHECK (find (list, "V/IV") == nullptr);
    CHECK (find (minorList, "vii" + deg + "7/III")->symbol() == "Bdim7");
}

TEST_CASE ("Borrowed chords in major")
{
    const auto list = chordsInKey (cMajor);
    for (const auto* numeral : { "iv", "bVI", "bVII", "bIII" })
    {
        INFO (numeral);
        const auto* rc = find (list, numeral);
        REQUIRE (rc != nullptr);
        CHECK (rc->origin == ChordOrigin::Borrowed);
    }
    CHECK (find (list, "bVI")->symbol() == "Ab");
    CHECK (find (list, "bVII")->symbol() == "Bb");
    CHECK (find (list, "Ger+6")->symbol() == "Ab7");

    // Raised notes are spelled with sharps even in flat keys.
    const auto dMinor = chordsInKey (Key { PitchClass (2), Mode::Minor });
    CHECK (find (dMinor, "vii" + deg + "7")->symbol() == "C#dim7");
    CHECK (find (list, "bVI")->function == HarmonicFunction::Subdominant);
    CHECK (find (list, "vii" + deg + "7")->symbol() == "Bdim7");
}

TEST_CASE ("Neapolitan and augmented sixths")
{
    const auto list = chordsInKey (cMajor);

    const auto* n6 = find (list, "N6");
    REQUIRE (n6 != nullptr);
    CHECK (n6->symbol() == "Db/F");
    CHECK (n6->bass() == PitchClass (5));
    CHECK (n6->function == HarmonicFunction::Subdominant);

    const auto* ger = find (list, "Ger+6");
    REQUIRE (ger != nullptr);
    CHECK (ger->chord.pitchClasses() == PitchClassSet ({ 8, 0, 3, 6 }));
    CHECK (ger->withInversion (2).inversion == 0); // fixed position

    CHECK (find (list, "It+6")->chord.pitchClasses() == PitchClassSet ({ 8, 0, 6 }));
    CHECK (find (list, "Fr+6")->chord.pitchClasses() == PitchClassSet ({ 8, 0, 2, 6 }));

    // Flat keys spell the Neapolitan with flats.
    const auto fMinor = chordsInKey (Key { PitchClass (5), Mode::Minor });
    CHECK (find (fMinor, "N6")->symbol() == "Gb/Bb");
}

TEST_CASE ("Chords are grouped by function and options filter families")
{
    const auto list = chordsInKey (cMajor);
    REQUIRE_FALSE (list.empty());
    CHECK (std::is_sorted (list.begin(), list.end(), [] (const RomanChord& a, const RomanChord& b)
    {
        return static_cast<int> (a.function) < static_cast<int> (b.function);
    }));

    HarmonyOptions triadsOnly;
    triadsOnly.sevenths = false;
    for (const auto& rc : chordsInKey (aMinor, triadsOnly))
        CHECK (rc.size() == 3);

    for (const auto& rc : chordsWithFunction (cMajor, HarmonicFunction::Dominant))
        CHECK (rc.function == HarmonicFunction::Dominant);
}

TEST_CASE ("Harmony in other tunings")
{
    // A 19-TET tonic is brought back to 12-TET for the analysis...
    const auto list = chordsInKey (Key { PitchClass (11, 19), Mode::Major }); // G in 19-TET
    REQUIRE_FALSE (list.empty());
    CHECK (list.front().symbol() == "G");

    // ...and chords can be realised in N-TET: C major triad in 19-TET = steps 0, 6, 11.
    const auto I = *find (chordsInKey (cMajor), "I");
    CHECK (I.voicingIn (19, 60) == std::vector<int> { 95, 101, 106 });
    CHECK (I.voicingIn (12, 60) == I.voicing (60));
}
