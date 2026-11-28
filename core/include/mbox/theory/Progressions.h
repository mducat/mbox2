#pragma once

#include "mbox/theory/Harmony.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mbox::theory
{

/**
    Chord-to-chord motion in functional harmony: which chords usually follow a
    given chord, and a library of common progressions (ii-V-I, cadences...).
    Used by the tonal map to draw arrows between chords.
*/

/** How idiomatic a motion between two chords is. */
enum class MotionStrength
{
    Strong,    // the expected resolution: V -> I, ii -> V, V/V -> V, root down a fifth
    Common,    // frequent and smooth: I -> IV, vi -> IV, plagal IV -> I...
    Deceptive  // the "surprise" resolution of a dominant: V -> vi (cadence rompue)
};

/**
    Strength of the motion from -> to in their (shared) key, nullopt when it is
    not a usual functional motion (e.g. V -> IV, a retrogression).
    Inversions and triad / seventh do not matter, only degree, function and origin.
*/
std::optional<MotionStrength> motionStrength (const RomanChord& from, const RomanChord& to);

/**
    Finds a chord of the key by its Roman numeral, inversion included:
    "ii6", "V6/5/V", "i6/4" (cadential), "N6", "bVI". The numeral must be
    spelled as RomanChord::numeral() writes it (° = U+00B0, ø = U+00F8).
*/
std::optional<RomanChord> findChord (const Key& key, std::string_view numeral);

/** A named progression, written as Roman numerals. */
struct ProgressionPattern
{
    std::string name;                  // "ii - V - I"
    std::string description;           // short explanation (French, for the UI)
    std::vector<std::string> numerals; // { "ii7", "V7", "IM7" }
};

/** Common progressions for a mode (every numeral resolves with findChord in any key of that mode). */
const std::vector<ProgressionPattern>& commonProgressions (Mode mode);

/** The chords of a pattern in a key (numerals that do not resolve are skipped). */
std::vector<RomanChord> realise (const Key& key, const ProgressionPattern& pattern);

} // namespace mbox::theory
