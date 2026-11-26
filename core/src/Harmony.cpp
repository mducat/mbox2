#include "mbox/theory/Harmony.h"

#include <algorithm>
#include <array>

namespace mbox::theory
{

namespace chords
{
    namespace
    {
        constexpr std::array kItalian { 0, 4, 10 };
        constexpr std::array kFrench  { 0, 4, 6, 10 };
        constexpr std::array kGerman  { 0, 4, 7, 10 };
    } // namespace

    const ChordQuality italianSixth { "Italian augmented sixth", "7(no5)", kItalian, 12 };
    const ChordQuality frenchSixth  { "French augmented sixth",  "7b5",    kFrench,  12 };
    const ChordQuality germanSixth  { "German augmented sixth",  "7",      kGerman,  12 };
} // namespace chords

namespace
{
    // UTF-8 written as escapes so the source compiles the same with or without /utf-8.
    constexpr const char* kDegreeSign   = "\xC2\xB0"; // °
    constexpr const char* kHalfDimSign  = "\xC3\xB8"; // ø

    constexpr std::array<const char*, 7> kRomanUpper { "I", "II", "III", "IV", "V", "VI", "VII" };
    constexpr std::array<const char*, 7> kRomanLower { "i", "ii", "iii", "iv", "v", "vi", "vii" };

    using F = HarmonicFunction;

    /** Finds the built-in quality whose intervals equal `pcs` read from `root`. */
    const ChordQuality* qualityFor (PitchClass root, PitchClassSet pcs, int size)
    {
        for (const auto* q : chords::all())
            if (static_cast<int> (q->intervals.size()) == size && Chord (root, *q).pitchClasses() == pcs)
                return q;
        return nullptr;
    }

    /** Tertian chord stacked on a scale degree: 3 notes (triad) or 4 (seventh). */
    Chord stackThirds (const Scale& scale, int degree, int size)
    {
        PitchClassSet pcs;
        for (int i = 0; i < size; ++i)
            pcs.insert (scale.degree (degree + 2 * i));

        const auto root = scale.degree (degree);
        const auto* q = qualityFor (root, pcs, size);
        return Chord (root, q != nullptr ? *q : chords::major);
    }

    int signedSemitones (int v) noexcept
    {
        const int m = mod12 (v);
        return m > 6 ? m - 12 : m;
    }

    bool isMajorLike (const ChordQuality& q) noexcept
    {
        return std::find (q.intervals.begin(), q.intervals.end(), 4) != q.intervals.end();
    }

    class Builder
    {
    public:
        Builder (const Key& k, const HarmonyOptions& o) : key (k), opts (o) {}

        std::vector<RomanChord> out;

        RomanChord make (Chord chord, int degree, F function, ChordOrigin origin = ChordOrigin::Diatonic,
                         std::optional<F> alternate = std::nullopt)
        {
            const int alteration = signedSemitones (chord.root().value() - key.degree (degree).value());
            RomanChord rc { key, chord, degree, alteration, 0, function, alternate, origin,
                            std::nullopt, RomanChord::Special::None };
            return rc;
        }

        void add (RomanChord rc) { out.push_back (std::move (rc)); }

        /** Triad and/or seventh on a degree of `scale` (defaults to the key's natural scale). */
        void diatonic (int degree, F function, std::optional<F> alternate = std::nullopt,
                       const Scale* scale = nullptr, ChordOrigin origin = ChordOrigin::Diatonic)
        {
            const Scale s = scale != nullptr ? *scale : key.scale();
            if (opts.triads)
                add (make (stackThirds (s, degree, 3), degree, function, origin, alternate));
            if (opts.sevenths)
                add (make (stackThirds (s, degree, 4), degree, function, origin, alternate));
        }

        void chord (PitchClass root, const ChordQuality& q, int degree, F function, ChordOrigin origin,
                    std::optional<F> alternate = std::nullopt)
        {
            const bool isSeventh = q.intervals.size() >= 4;
            if ((isSeventh && ! opts.sevenths) || (! isSeventh && ! opts.triads))
                return;
            add (make (Chord (root, q), degree, function, origin, alternate));
        }

        void secondaries (int target, F function)
        {
            const auto targetRoot = key.degree (target);
            if (key.mode == Mode::Minor && target == 6)
                return; // VII's dominant is IV of III: rarely used, keeps the map readable

            const auto dominantRoot = targetRoot.transposed (7);
            const auto leadingTone  = targetRoot.transposed (11);
            const int dominantDegree = (target + 4) % 7;
            const int leadingDegree  = (target + 6) % 7;

            auto applied = [&] (PitchClass root, const ChordQuality& q, int degree)
            {
                const bool isSeventh = q.intervals.size() >= 4;
                if ((isSeventh && ! opts.sevenths) || (! isSeventh && ! opts.triads))
                    return;
                // V/IV in major is just I: keep only the applied chords that add something.
                const Chord c (root, q);
                if (std::any_of (out.begin(), out.end(), [&] (const RomanChord& e) { return e.chord == c; }))
                    return;
                auto rc = make (c, degree, function, ChordOrigin::Secondary);
                rc.targetDegree = target;
                add (rc);
            };

            applied (dominantRoot, chords::major, dominantDegree);
            applied (dominantRoot, chords::dominant7, dominantDegree);
            applied (leadingTone, chords::diminished7, leadingDegree);
        }

        void neapolitan()
        {
            if (! opts.triads)
                return;
            auto rc = make (Chord (key.tonic.transposed (1), chords::major), 1, F::Subdominant, ChordOrigin::Chromatic);
            rc.inversion = 1;
            rc.special = RomanChord::Special::Neapolitan;
            add (rc);
        }

        void augmentedSixth (const ChordQuality& q, RomanChord::Special special)
        {
            const bool fourNotes = q.intervals.size() >= 4;
            if ((fourNotes && ! opts.sevenths) || (! fourNotes && ! opts.triads))
                return;
            auto rc = make (Chord (key.tonic.transposed (8), q), 5, F::Subdominant, ChordOrigin::Chromatic);
            rc.special = special;
            add (rc);
        }

        void cadentialSixFour()
        {
            const auto& q = key.mode == Mode::Major ? chords::major : chords::minor;
            auto rc = make (Chord (key.tonic, q), 0, F::Dominant, ChordOrigin::Cadential);
            rc.inversion = 2;
            add (rc);
        }

    private:
        const Key& key;
        const HarmonyOptions& opts;
    };

    int functionRank (F f) noexcept
    {
        switch (f)
        {
            case F::Tonic:       return 0;
            case F::Subdominant: return 1;
            case F::Dominant:    return 2;
        }
        return 0;
    }
} // namespace

// ---------------------------------------------------------------------------
// Key
// ---------------------------------------------------------------------------

Scale Key::scale() const noexcept
{
    return Scale (tonic, mode == Mode::Major ? scales::major : scales::naturalMinor);
}

bool Key::prefersFlats() const noexcept
{
    const auto relativeMajor = mode == Mode::Major ? tonic : tonic.transposed (3);
    const int pos = fifths::positionOf (relativeMajor);
    return pos >= 6; // Gb, Db, Ab, Eb, Bb, F
}

std::string Key::name() const
{
    return tonic.name (prefersFlats()) + (mode == Mode::Major ? " major" : " minor");
}

const char* functionName (HarmonicFunction f, bool shortName) noexcept
{
    switch (f)
    {
        case F::Tonic:       return shortName ? "T"  : "Tonic";
        case F::Subdominant: return shortName ? "SD" : "Subdominant";
        case F::Dominant:    return shortName ? "D"  : "Dominant";
    }
    return "";
}

// ---------------------------------------------------------------------------
// RomanChord
// ---------------------------------------------------------------------------

PitchClass RomanChord::bass() const noexcept
{
    const auto& iv = chord.quality().intervals;
    const auto idx = static_cast<std::size_t> (std::clamp (inversion, 0, size() - 1));
    return chord.root().transposed (iv[idx]);
}

RomanChord RomanChord::withInversion (int newInversion) const noexcept
{
    RomanChord rc = *this;
    const bool fixedPosition = special == Special::ItalianSixth || special == Special::FrenchSixth
                            || special == Special::GermanSixth;
    rc.inversion = fixedPosition ? 0 : std::clamp (newInversion, 0, size() - 1);
    return rc;
}

std::string RomanChord::figure() const
{
    if (size() >= 4)
    {
        static constexpr std::array<const char*, 4> sevenths { "7", "6/5", "4/3", "4/2" };
        return sevenths[static_cast<std::size_t> (std::clamp (inversion, 0, 3))];
    }
    static constexpr std::array<const char*, 3> triads { "", "6", "6/4" };
    return triads[static_cast<std::size_t> (std::clamp (inversion, 0, 2))];
}

std::string RomanChord::numeral() const
{
    switch (special)
    {
        case Special::ItalianSixth: return "It+6";
        case Special::FrenchSixth:  return "Fr+6";
        case Special::GermanSixth:  return "Ger+6";
        case Special::Neapolitan:   return "N" + figure();
        case Special::None:         break;
    }

    const auto& q = chord.quality();
    // Secondary chords are numbered from their target (V/V, not II/V).
    const int shown = targetDegree ? degree - *targetDegree : degree;
    const auto d  = static_cast<std::size_t> (((shown % 7) + 7) % 7);

    std::string text;

    if (! targetDegree)
    {
        // The raised leading tone of minor is part of the key: vii°, not #vii°.
        const bool minorLeadingTone = key.mode == Mode::Minor && d == 6 && alteration == 1;
        if (! minorLeadingTone)
            for (int i = 0; i < std::abs (alteration); ++i)
                text += alteration < 0 ? "b" : "#";
    }

    text += isMajorLike (q) ? kRomanUpper[d] : kRomanLower[d];

    if (&q == &chords::diminished || &q == &chords::diminished7) text += kDegreeSign;
    else if (&q == &chords::halfDiminished7)                     text += kHalfDimSign;
    else if (&q == &chords::augmented || &q == &chords::augmented7) text += "+";
    else if (&q == &chords::major7)                              text += "M";

    text += figure();

    if (targetDegree)
    {
        const auto t = static_cast<std::size_t> (((*targetDegree % 7) + 7) % 7);
        // Target spelled as its primary triad in the key (minor: harmonic V).
        const bool minorDominant = key.mode == Mode::Minor && t == 4;
        const bool upper = minorDominant || isMajorLike (stackThirds (key.scale(), static_cast<int> (t), 3).quality());
        text += "/";
        text += upper ? kRomanUpper[t] : kRomanLower[t];
    }

    return text;
}

std::string RomanChord::symbol() const
{
    // Altered roots follow their accidental (bVI -> Ab, vii°7/V -> F#), the rest the key signature.
    const bool flats = alteration < 0 || (alteration == 0 && key.prefersFlats());
    auto text = chord.symbol (flats);
    if (inversion > 0)
        text += "/" + bass().name (flats);
    return text;
}

std::vector<int> RomanChord::voicing (int bassOctaveStart) const
{
    const auto& iv = chord.quality().intervals;
    const int n = size();
    const int inv = std::clamp (inversion, 0, n - 1);

    std::vector<int> notes;
    notes.reserve (static_cast<std::size_t> (n));

    int previous = bassOctaveStart + mod12 (bass().value() - bassOctaveStart);
    notes.push_back (previous);

    for (int i = 1; i < n; ++i)
    {
        const auto pc = chord.root().transposed (iv[static_cast<std::size_t> ((inv + i) % n)]);
        int note = previous + mod12 (pc.value() - previous);
        if (note == previous)
            note += 12;
        notes.push_back (note);
        previous = note;
    }
    return notes;
}

// ---------------------------------------------------------------------------
// Generation
// ---------------------------------------------------------------------------

std::vector<RomanChord> chordsInKey (const Key& key, const HarmonyOptions& options)
{
    Builder b (key, options);
    const auto deg = [&] (int d) { return key.degree (d); };

    if (key.mode == Mode::Major)
    {
        // Tonic
        b.diatonic (0, F::Tonic);
        b.diatonic (5, F::Tonic, F::Subdominant);
        b.diatonic (2, F::Tonic, F::Dominant);
        if (options.borrowedChords)
        {
            b.chord (deg (2).transposed (-1), chords::major,  2, F::Tonic, ChordOrigin::Borrowed);      // bIII
            b.chord (deg (2).transposed (-1), chords::major7, 2, F::Tonic, ChordOrigin::Borrowed);      // bIIIM7
        }

        // Subdominant
        b.diatonic (3, F::Subdominant);
        b.diatonic (1, F::Subdominant);
        if (options.borrowedChords)
        {
            b.chord (deg (3), chords::minor,           3, F::Subdominant, ChordOrigin::Borrowed);       // iv
            b.chord (deg (3), chords::minor7,          3, F::Subdominant, ChordOrigin::Borrowed);       // iv7
            b.chord (deg (1), chords::diminished,      1, F::Subdominant, ChordOrigin::Borrowed);       // ii°
            b.chord (deg (1), chords::halfDiminished7, 1, F::Subdominant, ChordOrigin::Borrowed);       // iiø7
            b.chord (deg (5).transposed (-1), chords::major,  5, F::Subdominant, ChordOrigin::Borrowed); // bVI
            b.chord (deg (5).transposed (-1), chords::major7, 5, F::Subdominant, ChordOrigin::Borrowed); // bVIM7
            b.chord (deg (6).transposed (-1), chords::major,     6, F::Subdominant, ChordOrigin::Borrowed, F::Dominant); // bVII
            b.chord (deg (6).transposed (-1), chords::dominant7, 6, F::Subdominant, ChordOrigin::Borrowed, F::Dominant); // bVII7
        }

        // Dominant
        b.diatonic (4, F::Dominant);
        b.diatonic (6, F::Dominant);
        if (options.borrowedChords)
            b.chord (deg (6), chords::diminished7, 6, F::Dominant, ChordOrigin::Borrowed);              // vii°7
    }
    else
    {
        const Scale harmonic (key.tonic, scales::harmonicMinor);

        // Tonic
        b.diatonic (0, F::Tonic);
        b.diatonic (2, F::Tonic, F::Dominant);
        b.diatonic (5, F::Tonic, F::Subdominant);
        if (options.borrowedChords)
            b.chord (deg (0), chords::major, 0, F::Tonic, ChordOrigin::Borrowed);                       // Picardy I

        // Subdominant
        b.diatonic (3, F::Subdominant);
        b.diatonic (1, F::Subdominant);
        if (options.borrowedChords)
        {
            b.chord (deg (3), chords::major,     3, F::Subdominant, ChordOrigin::Borrowed);             // IV (melodic)
            b.chord (deg (3), chords::dominant7, 3, F::Subdominant, ChordOrigin::Borrowed);             // IV7 (dorian)
        }

        // Dominant (harmonic minor first, then the natural-minor v and VII)
        b.diatonic (4, F::Dominant, std::nullopt, &harmonic);
        b.diatonic (6, F::Dominant, std::nullopt, &harmonic);
        b.diatonic (4, F::Dominant);
        b.diatonic (6, F::Dominant, F::Subdominant);
    }

    if (options.cadentialSixFour && options.triads)
        b.cadentialSixFour();

    if (options.chromaticChords)
    {
        b.neapolitan();
        b.augmentedSixth (chords::italianSixth, RomanChord::Special::ItalianSixth);
        b.augmentedSixth (chords::frenchSixth,  RomanChord::Special::FrenchSixth);
        b.augmentedSixth (chords::germanSixth,  RomanChord::Special::GermanSixth);
    }

    if (options.secondaryDominants)
    {
        // Placed in the column of the chord they resolve to.
        if (key.mode == Mode::Major)
        {
            b.secondaries (4, F::Dominant);    // V/V
            b.secondaries (1, F::Subdominant); // V/ii
            b.secondaries (3, F::Subdominant); // V/IV
            b.secondaries (5, F::Tonic);       // V/vi
            b.secondaries (2, F::Tonic);       // V/iii
        }
        else
        {
            b.secondaries (4, F::Dominant);    // V/V
            b.secondaries (3, F::Subdominant); // V/iv
            b.secondaries (5, F::Tonic);       // V/VI
            b.secondaries (2, F::Tonic);       // V/III
        }
    }

    // Stable grouping by function; inside a function the insertion order above
    // is the order of importance (primary chords, substitutes, colour chords).
    std::stable_sort (b.out.begin(), b.out.end(), [] (const RomanChord& a, const RomanChord& c)
    {
        return functionRank (a.function) < functionRank (c.function);
    });

    return std::move (b.out);
}

std::vector<RomanChord> chordsWithFunction (const Key& key, HarmonicFunction function, const HarmonyOptions& options)
{
    auto all = chordsInKey (key, options);
    std::erase_if (all, [function] (const RomanChord& rc) { return rc.function != function; });
    return all;
}

} // namespace mbox::theory
