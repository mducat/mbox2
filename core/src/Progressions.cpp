#include "mbox/theory/Progressions.h"

#include <algorithm>

namespace mbox::theory
{

namespace
{
    int mod7 (int v) noexcept { return ((v % 7) + 7) % 7; }

    bool isSecondary (const RomanChord& c) noexcept { return c.targetDegree.has_value(); }
    bool isCadential (const RomanChord& c) noexcept { return c.origin == ChordOrigin::Cadential; }
    bool isSpecial (const RomanChord& c) noexcept { return c.special != RomanChord::Special::None; }

    bool hasMajorThird (const RomanChord& c)
    {
        const auto& iv = c.chord.quality().intervals;
        return std::find (iv.begin(), iv.end(), 4) != iv.end();
    }

    /** I / i (or the Picardy I), root position or not, but not the cadential 6/4. */
    bool isTonicChord (const RomanChord& c)
    {
        return c.degree == 0 && c.alteration == 0 && ! isSecondary (c) && ! isCadential (c) && ! isSpecial (c);
    }

    /** V or V7 (major third): the real dominant, not the minor v. */
    bool isMajorDominant (const RomanChord& c)
    {
        return c.degree == 4 && c.alteration == 0 && ! isSecondary (c) && ! isCadential (c) && hasMajorThird (c);
    }

    /** vii° / vii°7 / viiø7 on the leading tone. */
    bool isLeadingToneChord (const RomanChord& c)
    {
        const bool leading = c.key.mode == Mode::Major ? c.alteration == 0 : c.alteration == 1;
        return c.degree == 6 && leading && ! isSecondary (c) && ! hasMajorThird (c);
    }

    bool sameChord (const RomanChord& a, const RomanChord& b)
    {
        return a.chord == b.chord && a.origin == b.origin && a.targetDegree == b.targetDegree && a.special == b.special;
    }

    bool isPlainChord (const RomanChord& c) { return ! isSecondary (c) && ! isCadential (c) && ! isSpecial (c); }
} // namespace

std::optional<MotionStrength> motionStrength (const RomanChord& from, const RomanChord& to)
{
    using S = MotionStrength;

    if (from.key != to.key || sameChord (from, to))
        return std::nullopt;

    // Applied chords lead to their target (and V/V also to the cadential 6/4).
    if (isSecondary (from))
    {
        if (isPlainChord (to) && to.degree == *from.targetDegree && to.alteration == 0)
            return S::Strong;
        if (*from.targetDegree == 4 && isCadential (to))
            return S::Strong;
        return std::nullopt;
    }

    // Arrows into applied chords would point everywhere: patterns show them instead.
    if (isSecondary (to))
        return std::nullopt;

    if (isCadential (from))
        return isMajorDominant (to) ? std::optional (S::Strong) : std::nullopt;

    // Neapolitan and augmented sixths prepare the dominant.
    if (isSpecial (from))
        return (isMajorDominant (to) || isCadential (to)) ? std::optional (S::Strong) : std::nullopt;

    const int step = mod7 (to.degree - from.degree); // 3 = root down a fifth, 5 = down a third, 1 = up a step

    switch (from.function)
    {
        case HarmonicFunction::Dominant:
        {
            // Natural-minor subtonic VII: dominant of III, weak dominant of i.
            if (from.key.mode == Mode::Minor && from.degree == 6 && from.alteration == 0)
            {
                if (to.degree == 2 && isPlainChord (to)) return S::Strong;
                if (isTonicChord (to))                    return S::Common;
                return std::nullopt;
            }

            if (isTonicChord (to))
                return (isMajorDominant (from) || isLeadingToneChord (from)) ? S::Strong : S::Common;

            if (to.degree == 5 && to.function == HarmonicFunction::Tonic && isPlainChord (to)
                && (from.degree == 4 || from.degree == 6))
                return S::Deceptive;

            return std::nullopt;
        }

        case HarmonicFunction::Subdominant:
        {
            if (to.function == HarmonicFunction::Dominant)
            {
                if (isMajorDominant (to) || isCadential (to) || isLeadingToneChord (to))
                    return S::Strong;
                return S::Common; // v, VII in minor
            }

            if (isTonicChord (to))
                return S::Common; // plagal

            if (to.function == HarmonicFunction::Subdominant && isPlainChord (to))
            {
                if (step == 5 && to.origin == ChordOrigin::Diatonic) return S::Common; // IV -> ii
                if (from.degree == 3 && to.degree == 3 && to.origin == ChordOrigin::Borrowed
                    && from.origin == ChordOrigin::Diatonic)         return S::Common; // IV -> iv
                if (step == 1 && from.origin == ChordOrigin::Borrowed
                    && to.origin == ChordOrigin::Borrowed)           return S::Common; // bVI -> bVII
            }

            if (isSpecial (to) && from.degree == 3 && from.alteration == 0)
                return S::Common; // iv -> N6, IV -> Ger+6

            return std::nullopt;
        }

        case HarmonicFunction::Tonic:
        {
            if (isCadential (to) || isSpecial (to))
                return std::nullopt;

            if (isTonicChord (from))
            {
                // The tonic opens towards the key's main chords: IV, ii, vi, iii, V.
                if (to.origin != ChordOrigin::Diatonic || to.degree == 0 || to.degree == 6)
                    return std::nullopt;
                if (to.degree == 4 && ! isMajorDominant (to))
                    return std::nullopt; // minor v: rather reached from iv
                return S::Common;
            }

            if (step == 3)                                       return S::Strong; // vi -> ii, iii -> vi
            if (step == 5)                                       return S::Common; // vi -> IV, iii -> I
            if (to.function == HarmonicFunction::Subdominant)    return S::Common;
            return std::nullopt;
        }
    }

    return std::nullopt;
}

std::optional<RomanChord> findChord (const Key& key, std::string_view numeral)
{
    const auto all = chordsInKey (key);

    // Chords as generated first (so "i6/4" is the cadential 6/4, "N6" the Neapolitan)...
    for (const auto& c : all)
        if (c.numeral() == numeral)
            return c;

    // ...then every inversion.
    for (const auto& c : all)
        for (int inv = 0; inv < c.size(); ++inv)
            if (const auto v = c.withInversion (inv); v.inversion == inv && v.numeral() == numeral)
                return v;

    return std::nullopt;
}

const std::vector<ProgressionPattern>& commonProgressions (Mode mode)
{
    // "\xC2\xB0" = °, "\xC3\xB8" = ø (see RomanChord::numeral).
    static const std::vector<ProgressionPattern> major {
        { "ii - V - I",          "La cadence parfaite la plus courante",           { "ii", "V", "I" } },
        { "ii7 - V7 - IM7",      "Le ii-V-I du jazz, avec septièmes",              { "ii7", "V7", "IM7" } },
        { "IV - V - I",          "Cadence parfaite classique",                      { "IV", "V", "I" } },
        { "ii6 - I6/4 - V7 - I", "Cadence avec quarte et sixte de cadence",         { "ii6", "I6/4", "V7", "I" } },
        { "vi - ii - V - I",     "Descente par quintes vers la tonique",            { "vi", "ii", "V", "I" } },
        { "iii - vi - ii - V - I", "Le cycle des quintes complet",                  { "iii", "vi", "ii", "V", "I" } },
        { "I - vi - IV - V",     "Progression des années 50",                       { "I", "vi", "IV", "V" } },
        { "I - V - vi - IV",     "Progression pop",                                 { "I", "V", "vi", "IV" } },
        { "IV - I",              "Cadence plagale",                                 { "IV", "I" } },
        { "V7 - vi",             "Cadence rompue : la dominante évite la tonique",  { "V7", "vi" } },
        { "I - V7/V - V - I",    "Dominante secondaire : on tonicise V",            { "I", "V7/V", "V", "I" } },
        { "I - V7/vi - vi",      "On tonicise le relatif mineur",                   { "I", "V7/vi", "vi" } },
        { "IV - iv - I",         "Sous-dominante mineure empruntée",                { "IV", "iv", "I" } },
        { "bVI - bVII - I",      "Emprunts au mineur, cadence « héroïque »",        { "bVI", "bVII", "I" } },
    };

    static const std::vector<ProgressionPattern> minor {
        { "ii\xC3\xB8" "7 - V7 - i",       "Le ii-V-i mineur",                           { "ii\xC3\xB8" "7", "V7", "i" } },
        { "iv - V - i",                    "Cadence parfaite en mineur",                 { "iv", "V", "i" } },
        { "ii\xC2\xB0" "6 - i6/4 - V7 - i", "Cadence avec quarte et sixte de cadence",   { "ii\xC2\xB0" "6", "i6/4", "V7", "i" } },
        { "i - iv - V7 - i",               "Le tour complet des trois fonctions",        { "i", "iv", "V7", "i" } },
        { "i - VI - III - VII",            "Progression « épique »",                     { "i", "VI", "III", "VII" } },
        { "i - VII - VI - V",              "Cadence andalouse",                          { "i", "VII", "VI", "V" } },
        { "VI - VII - i",                  "Montée vers la tonique",                     { "VI", "VII", "i" } },
        { "V7 - VI",                       "Cadence rompue",                             { "V7", "VI" } },
        { "N6 - V7 - i",                   "Sixte napolitaine",                          { "N6", "V7", "i" } },
        { "Ger+6 - i6/4 - V7 - i",         "Sixte augmentée allemande",                  { "Ger+6", "i6/4", "V7", "i" } },
        { "i - V7/iv - iv - V7 - i",       "On tonicise la sous-dominante",              { "i", "V7/iv", "iv", "V7", "i" } },
        { "iv - i",                        "Cadence plagale mineure",                    { "iv", "i" } },
    };

    return mode == Mode::Major ? major : minor;
}

std::vector<RomanChord> realise (const Key& key, const ProgressionPattern& pattern)
{
    std::vector<RomanChord> out;
    for (const auto& n : pattern.numerals)
        if (auto c = findChord (key, n))
            out.push_back (*c);
    return out;
}

} // namespace mbox::theory
