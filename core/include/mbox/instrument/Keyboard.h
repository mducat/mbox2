#pragma once

#include <vector>

namespace mbox::instrument
{

/**
    Pitches in this module are plain integers counted in steps of an equal
    temperament with `stepsPerOctave` steps (N-TET). Pitch 0 is the C of MIDI
    octave -1, so in 12-TET a pitch is exactly a MIDI note number, and in any
    N-TET the C's line up: pitch 5*N is middle C.
*/

/** Euclidean modulo (0..n-1, also for negative input). */
constexpr int wrap (int value, int n) noexcept
{
    const int r = value % n;
    return r < 0 ? r + n : r;
}

/** Converts a 12-TET pitch (MIDI note) to the nearest step of an N-TET. */
int pitchFromTwelveTet (int midiNote, int stepsPerOctave) noexcept;

/** Where a key sits on a keyboard drawing. Unit: the width of one lower ("white") key. */
struct KeyGeometry
{
    int step = 0;        // 0 .. stepsPerOctave-1
    bool upper = false;  // true for the raised ("black") row
    double x = 0.0;      // left edge
    double width = 1.0;
};

/**
    Keyboard layout for any equal temperament.

    The lower row holds the steps closest to the 7 diatonic notes (C D E F G A B),
    the steps in between go to the raised row, spread over the gap between two
    lower keys. In 12-TET this gives the usual piano; in 24-TET there are three
    raised keys between C and D, one between E and F, and so on.
*/
class KeyboardLayout
{
public:
    static KeyboardLayout forEqualTemperament (int stepsPerOctave);

    int stepsPerOctave() const noexcept { return steps; }
    int lowerKeysPerOctave() const noexcept { return lowerPerOctave; }

    /** Geometry of a step inside one octave (x in 0 .. lowerKeysPerOctave). */
    const KeyGeometry& key (int step) const noexcept { return keys[static_cast<std::size_t> (wrap (step, steps))]; }

    /** Geometry of an absolute pitch; x is measured from pitch 0. */
    KeyGeometry keyForPitch (int pitch) const noexcept;

    /** Left edge (in lower-key units) of the lower key at or just above `pitch`. */
    double xOfPitch (int pitch) const noexcept { return keyForPitch (pitch).x; }

    /** Pitch under a point of the drawing. y is 0 at the top, 1 at the bottom; the raised row covers y < upperHeight. */
    int pitchAt (double x, double y, double upperHeight, int firstPitch, int lastPitch) const noexcept;

private:
    int steps = 12;
    int lowerPerOctave = 7;
    std::vector<KeyGeometry> keys;
};

} // namespace mbox::instrument
