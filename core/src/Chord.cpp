#include "mbox/theory/Chord.h"

#include <algorithm>
#include <array>

namespace mbox::theory
{

namespace chords
{
    namespace
    {
        constexpr std::array kMajor        { 0, 4, 7 };
        constexpr std::array kMinor        { 0, 3, 7 };
        constexpr std::array kDim          { 0, 3, 6 };
        constexpr std::array kAug          { 0, 4, 8 };
        constexpr std::array kSus2         { 0, 2, 7 };
        constexpr std::array kSus4         { 0, 5, 7 };
        constexpr std::array kDom7         { 0, 4, 7, 10 };
        constexpr std::array kMaj7         { 0, 4, 7, 11 };
        constexpr std::array kMin7         { 0, 3, 7, 10 };
        constexpr std::array kMinMaj7      { 0, 3, 7, 11 };
        constexpr std::array kHalfDim7     { 0, 3, 6, 10 };
        constexpr std::array kDim7         { 0, 3, 6, 9 };
        constexpr std::array kAug7         { 0, 4, 8, 10 };
        constexpr std::array kMaj6         { 0, 4, 7, 9 };
        constexpr std::array kMin6         { 0, 3, 7, 9 };
        constexpr std::array kPower5       { 0, 7 };
    } // namespace

    const ChordQuality major           { "Major",              "",      kMajor };
    const ChordQuality minor           { "Minor",              "m",     kMinor };
    const ChordQuality diminished      { "Diminished",         "dim",   kDim };
    const ChordQuality augmented       { "Augmented",          "aug",   kAug };
    const ChordQuality sus2            { "Suspended second",   "sus2",  kSus2 };
    const ChordQuality sus4            { "Suspended fourth",   "sus4",  kSus4 };
    const ChordQuality dominant7       { "Dominant seventh",   "7",     kDom7 };
    const ChordQuality major7          { "Major seventh",      "maj7",  kMaj7 };
    const ChordQuality minor7          { "Minor seventh",      "m7",    kMin7 };
    const ChordQuality minorMajor7     { "Minor-major seventh","mMaj7", kMinMaj7 };
    const ChordQuality halfDiminished7 { "Half-diminished",    "m7b5",  kHalfDim7 };
    const ChordQuality diminished7     { "Diminished seventh", "dim7",  kDim7 };
    const ChordQuality augmented7      { "Augmented seventh",  "aug7",  kAug7 };
    const ChordQuality major6          { "Major sixth",        "6",     kMaj6 };
    const ChordQuality minor6          { "Minor sixth",        "m6",    kMin6 };
    const ChordQuality power5          { "Power chord",        "5",     kPower5 };

    std::span<const ChordQuality* const> all()
    {
        // Recognition order: when several readings match (e.g. C6 = Am7),
        // identifyChords() lists them in this order, then the bass picks the root.
        static const std::array<const ChordQuality*, 16> list {
            &major, &minor, &diminished, &augmented, &sus4, &sus2,
            &dominant7, &major7, &minor7, &halfDiminished7, &diminished7,
            &minorMajor7, &augmented7, &major6, &minor6, &power5
        };
        return list;
    }
} // namespace chords

PitchClassSet Chord::pitchClasses() const noexcept
{
    PitchClassSet s;
    for (const int i : chordQuality->intervals)
        s.insert (chordRoot.transposed (i));
    return s;
}

std::vector<int> Chord::voicing (int rootMidiNote) const
{
    std::vector<int> notes;
    notes.reserve (chordQuality->intervals.size());
    for (const int i : chordQuality->intervals)
        notes.push_back (rootMidiNote + i);
    return notes;
}

std::string Chord::symbol (bool preferFlats) const
{
    return chordRoot.name (preferFlats) + std::string (chordQuality->symbol);
}

std::vector<Chord> identifyChords (PitchClassSet notes, std::optional<PitchClass> bass)
{
    std::vector<Chord> result;
    if (notes.empty())
        return result;

    for (const auto* quality : chords::all())
        for (int r = 0; r < kNumPitchClasses; ++r)
            if (Chord c (PitchClass (r), *quality); c.pitchClasses() == notes)
                result.push_back (c);

    if (bass)
        std::stable_partition (result.begin(), result.end(),
                               [b = *bass] (const Chord& c) { return c.root() == b; });

    return result;
}

} // namespace mbox::theory
