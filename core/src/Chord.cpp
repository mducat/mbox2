#include "mbox/theory/Chord.h"

#include <algorithm>
#include <array>
#include <cmath>

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

std::vector<int> ChordQuality::stepsIn (int targetDivisions) const
{
    std::vector<int> out;
    out.reserve (intervals.size());
    for (const int i : intervals)
        out.push_back (edo::convert (i, divisions, targetDivisions));
    return out;
}

PitchClassSet Chord::pitchClasses() const noexcept
{
    auto s = PitchClassSet::withDivisions (divisions());
    for (const int i : chordQuality->intervals)
        s.insert (chordRoot.transposed (edo::convert (i, chordQuality->divisions, divisions())));
    return s;
}

std::vector<int> Chord::voicing (int rootNote) const
{
    auto notes = steps();
    for (auto& n : notes)
        n += rootNote;
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

    const int d = notes.divisions();
    for (const auto* quality : chords::all())
        for (int r = 0; r < d; ++r)
            if (Chord c (PitchClass (r, d), *quality); c.pitchClasses() == notes)
                result.push_back (c);

    if (bass)
        std::stable_partition (result.begin(), result.end(),
                               [b = bass->convertedTo (d)] (const Chord& c) { return c.root() == b; });

    return result;
}


// ---------------------------------------------------------------------------
namespace
{
    constexpr std::array<const char*, 12> kIntervalNames {
        "P1", "m2", "M2", "m3", "M3", "P4", "TT", "P5", "m6", "M6", "m7", "M7"
    };

    /** One reading of a pitch-class set from a given root. */
    struct Reading
    {
        std::string text;
        double cost = 0.0;
    };

    void appendList (std::string& s, const std::vector<std::string>& items)
    {
        if (items.empty())
            return;
        s += '(';
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            if (i > 0)
                s += ',';
            s += items[i];
        }
        s += ')';
    }

    Reading readFrom (PitchClass root, const std::vector<PitchClass>& notes, bool preferFlats)
    {
        const int d = root.divisions();

        // Split the notes into 12-TET roles and genuinely microtonal steps.
        std::array<bool, 12> has {};
        std::vector<std::pair<int, std::string>> micro; // (nearest semitone, label)
        for (const auto pc : notes)
        {
            const int k = root.intervalTo (pc);
            if (k == 0)
                continue;

            int role = -1;
            for (int s = 1; s < 12 && role < 0; ++s)
                if (edo::fromSemitones (s, d) == k)
                    role = s;

            if (role >= 0)
            {
                has[static_cast<std::size_t> (role)] = true;
                continue;
            }

            const double cents = 1200.0 * k / d;
            const int nearest = static_cast<int> (std::ceil (cents / 100.0 - 0.5)); // ties go down
            const int offset = static_cast<int> (std::lround (cents - nearest * 100.0));
            std::string label = nearest >= 12 ? std::string ("P8") : std::string (kIntervalNames[static_cast<std::size_t> (nearest)]);
            label += (offset > 0 ? "+" : "") + std::to_string (offset);
            micro.emplace_back (nearest, label);
        }

        Reading r;
        r.text = root.name (preferFlats);

        // Two notes: just name the interval.
        if (notes.size() == 2)
        {
            if (! micro.empty())
                r.text += "(" + micro.front().second + ")";
            else
                for (int s = 1; s < 12; ++s)
                    if (has[static_cast<std::size_t> (s)])
                        r.text += std::string ("(") + kIntervalNames[static_cast<std::size_t> (s)] + ")";
            return r;
        }

        auto take = [&has] (int s)
        {
            const bool h = has[static_cast<std::size_t> (s)];
            has[static_cast<std::size_t> (s)] = false;
            return h;
        };

        bool microThird = false;
        for (const auto& m : micro)
            microThird = microThird || m.first == 3 || m.first == 4;

        // Third
        enum class Third { none, minor, major, sus2, sus4 } third = Third::none;
        if (take (4))       third = Third::major;
        else if (take (3))  third = Third::minor;
        else if (! microThird && take (5)) third = Third::sus4;
        else if (! microThird && take (2)) third = Third::sus2;

        // Fifth
        enum class Fifth { none, perfect, flat, sharp } fifth = Fifth::none;
        if (take (7))                                  fifth = Fifth::perfect;
        else if (third != Third::major && take (6))    fifth = Fifth::flat;
        else if (third == Third::major && take (8))    fifth = Fifth::sharp;
        else if (third == Third::major && take (6))    fifth = Fifth::flat;

        // Seventh
        enum class Seventh { none, dominant, major, diminished } seventh = Seventh::none;
        if (take (10))      seventh = Seventh::dominant;
        else if (take (11)) seventh = Seventh::major;
        else if (third == Third::minor && fifth == Fifth::flat && take (9)) seventh = Seventh::diminished;
        const bool hasSeventh = seventh != Seventh::none;

        const bool sixth = ! hasSeventh && take (9);

        // Remaining notes become tensions / adds / alterations.
        std::vector<std::string> extras;
        double cost = 0.0;
        auto extra = [&] (int s, const char* withSeventh, const char* without, double c)
        {
            if (take (s))
            {
                extras.emplace_back (hasSeventh || sixth ? withSeventh : without);
                cost += c;
            }
        };
        extra (1, "b9", "b9", 1.5);
        extra (2, "9", "add9", 1.0);
        if (third == Third::major) extra (3, "#9", "#9", 1.25);
        extra (5, "11", "add11", 1.0);
        extra (6, "#11", "#11", 1.25);
        extra (8, "b13", "b6", 1.25);
        extra (9, "13", "6", 1.0);
        extra (3, "m3", "m3", 2.0);   // leftovers that fit nowhere else
        extra (10, "m7", "m7", 2.0);
        extra (11, "M7", "M7", 2.0);
        extra (5, "P4", "P4", 2.0);
        extra (2, "M2", "M2", 2.0);
        for (const auto& m : micro)
        {
            extras.push_back (m.second);
            cost += 1.5;
        }

        // Stack natural extensions into the chord number (C7 + 9 -> C9, + 11 -> C11...).
        int number = seventh == Seventh::diminished ? 7 : (hasSeventh ? 7 : 0);
        if (hasSeventh && seventh != Seventh::diminished)
        {
            auto pop = [&extras] (const char* t)
            {
                const auto it = std::find (extras.begin(), extras.end(), t);
                if (it == extras.end())
                    return false;
                extras.erase (it);
                return true;
            };
            if (pop ("9"))
            {
                number = 9;
                cost -= 0.25; // stacked natural extensions are cheaper than loose tensions
                if (pop ("11"))
                {
                    number = 11;
                    cost -= 0.25;
                }
                if (pop ("13"))
                {
                    number = 13;
                    cost -= 0.25;
                }
            }
        }

        // Assemble: prefix + number + fifth alteration + sus + (extras)
        std::string core;
        std::string alt;
        switch (fifth)
        {
            case Fifth::flat:
                if (third == Third::minor && seventh == Seventh::diminished)
                {
                    core = "dim7";
                    cost += 1.0; // symmetric: prefer a dominant 7(b9) reading when one exists
                }
                else if (third == Third::minor && ! hasSeventh && ! sixth)
                    core = "dim";
                else
                {
                    alt = "b5";
                    if (! (third == Third::minor && seventh == Seventh::dominant)) // m7b5 is idiomatic
                        cost += 0.75;
                }
                break;
            case Fifth::sharp:
                if (third == Third::major && (seventh == Seventh::none || seventh == Seventh::dominant) && ! sixth)
                    core = "aug";
                else
                {
                    alt = "#5";
                    cost += 0.75;
                }
                break;
            case Fifth::none:
                cost += 0.25;
                break;
            case Fifth::perfect:
                break;
        }

        if (core != "dim7")
        {
            std::string head = (third == Third::minor && core.empty()) ? "m" : "";
            if (core == "dim" || core == "aug")
                head = core;
            if (seventh == Seventh::major)
            {
                head += third == Third::minor ? "Maj" : "maj";
                if (third == Third::minor)
                    cost += 0.5; // minor-major is rarer than the other readings
            }
            if (number > 0)
                head += std::to_string (number);
            else if (sixth)
                head += "6";
            core = head;
        }
        if (number > 0 || sixth)
            core += alt;
        else if (! alt.empty())
            extras.insert (extras.begin(), alt); // "C(b5)", never "Cb5"

        if (third == Third::sus4) { core += "sus4"; cost += 0.5; }
        if (third == Third::sus2) { core += "sus2"; cost += 0.5; }
        if (third == Third::none && ! microThird)
        {
            extras.emplace_back ("no3");
            cost += 2.0;
        }
        if (fifth == Fifth::none && ! hasSeventh && ! sixth && extras.empty() && third != Third::none)
            extras.emplace_back ("no5");

        r.text += core;
        appendList (r.text, extras);
        r.cost = cost;
        return r;
    }
} // namespace

std::string describeChord (PitchClassSet notes, std::optional<PitchClass> bass, bool preferFlats)
{
    const auto pcs = notes.toVector();
    if (pcs.empty())
        return {};

    const int d = notes.divisions();
    const std::optional<PitchClass> b = bass ? std::optional<PitchClass> (bass->convertedTo (d)) : std::nullopt;

    if (pcs.size() == 1)
        return pcs.front().name (preferFlats);

    if (const auto found = identifyChords (notes, b); ! found.empty())
    {
        std::string s = found.front().symbol (preferFlats);
        if (b && found.front().root() != *b && notes.contains (*b))
            s += "/" + b->name (preferFlats);
        return s;
    }

    const bool bassIn = b && notes.contains (*b);

    // Dyads are read from the bass (or the lowest step).
    if (pcs.size() == 2)
        return readFrom (bassIn ? *b : pcs.front(), pcs, preferFlats).text;

    std::optional<Reading> best;
    PitchClass bestRoot;
    for (const auto root : pcs)
    {
        auto r = readFrom (root, pcs, preferFlats);
        if (bassIn && root != *b)
            r.cost += 0.75;
        if (! best || r.cost < best->cost)
        {
            best = r;
            bestRoot = root;
        }
    }

    std::string s = best->text;
    if (bassIn && bestRoot != *b)
        s += "/" + b->name (preferFlats);
    return s;
}

} // namespace mbox::theory
