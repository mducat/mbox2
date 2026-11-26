#pragma once

#include "mbox/theory/PitchClass.h"

#include <string>
#include <vector>

namespace mbox::theory
{

/**
    A tuning: how the N steps of the pitch space sound.

    Notes are absolute integers ("note numbers"): note = octave * N + step,
    laid out like MIDI so that in 12-TET the note number *is* the MIDI note
    (60 = middle C, 69 = A4). In any N, note 5 * N is middle C (C4) and step 0
    is always C.

    Two kinds of tuning:
      - equal (N-TET / N-EDO): step k is k * period / N cents.  Tuning::equal (19)
      - table: explicit cents for each step of the period (just intonation,
        meantone, Scala files later...).                          Tuning::fromCents (...)

    The period is normally the octave (1200 cents); non-octave tunings
    (Bohlen-Pierce...) only need another periodCents.
*/
class Tuning
{
public:
    /** 12-TET, A4 = 440 Hz. */
    Tuning();

    /** Equal division of the period in `divisions` steps (1..kMaxDivisions). */
    static Tuning equal (int divisions, double periodCents = 1200.0);

    /**
        Arbitrary tuning from the cents of each step of one period. stepCents[0]
        must be 0 and the list ascending; its size is the number of divisions.
    */
    static Tuning fromCents (std::string name, std::vector<double> stepCents, double periodCents = 1200.0);

    // -- Description ---------------------------------------------------------

    const std::string& name() const noexcept { return tuningName; }
    int divisions() const noexcept { return static_cast<int> (steps.size()); }
    double periodCents() const noexcept { return period; }
    bool isEqual() const noexcept { return equalSteps; }
    bool isTwelveTet() const noexcept;

    /** Concert pitch: frequency of A4 (default 440 Hz). C4 is placed 900 cents below. */
    double concertA() const noexcept { return aHz; }
    Tuning withConcertA (double hz) const;

    // -- Notes ---------------------------------------------------------------

    /** Note number of middle C (C4): 5 * divisions (60 in 12-TET). */
    int middleC() const noexcept { return 5 * divisions(); }

    int octaveOf (int note) const noexcept;               // 4 for middle C (MIDI convention)
    PitchClass pitchClassOf (int note) const noexcept;
    int noteOf (PitchClass pc, int octave) const noexcept; // inverse of the two above

    /** Cents of a note above middle C (negative below). */
    double centsFromMiddleC (int note) const noexcept;

    /** Cents of one step of the period, 0 <= result < periodCents. */
    double stepCents (int step) const noexcept;

    double frequencyOf (int note) const noexcept;

    /** Closest note to a frequency, with the remaining offset in cents if wanted. */
    int nearestNote (double hz, double* centsOffset = nullptr) const noexcept;

    /** Name of a note: pitch class name + octave ("C4", "F#3", "D-37 4" style in N-TET). */
    std::string noteName (int note, bool preferFlats = false) const;

    // -- Intervals -----------------------------------------------------------

    /** Size of an interval of `steps` steps starting from step 0, in cents. */
    double intervalCents (int steps) const noexcept;

    /** Number of steps closest to an interval in cents (e.g. 386.3 for a just major third). */
    int stepsFromCents (double cents) const noexcept;

    /** Number of steps closest to a frequency ratio (e.g. 3.0 / 2.0). */
    int stepsFromRatio (double ratio) const noexcept;

    /** Number of steps realising a 12-TET interval (identity in 12-TET). */
    int stepsFromSemitones (int semitones) const noexcept { return edo::fromSemitones (semitones, divisions()); }

    /**
        Converts a note of another tuning to the closest note of this one, by
        pitch. Used to map MIDI keyboards (12-TET) onto N-TET.
    */
    int convertNote (int note, const Tuning& from) const noexcept;

    /** Same steps, period and concert pitch (the name is ignored). */
    friend bool operator== (const Tuning& a, const Tuning& b) noexcept;

private:
    Tuning (std::string name, std::vector<double> stepCents, double periodCents, bool isEqualTuning);

    std::string tuningName;
    std::vector<double> steps; // cents of each step within one period, steps[0] == 0
    double period = 1200.0;
    double aHz = 440.0;
    bool equalSteps = true;
};

/** Common presets for UI lists: 12, 17, 19, 22, 24, 31, 41, 53-TET. */
namespace tunings
{
    std::vector<Tuning> presets();
} // namespace tunings

} // namespace mbox::theory
