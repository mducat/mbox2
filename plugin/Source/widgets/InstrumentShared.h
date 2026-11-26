#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <mbox/instrument/Keyboard.h>

#include <algorithm>
#include <functional>
#include <set>
#include <vector>

/** Palette, note naming and the display snapshot shared by the piano and fretboard views. */
namespace instrument_ui
{
    inline const juce::Colour background { 0xff15171c };
    inline const juce::Colour panel      { 0xff1e2129 };
    inline const juce::Colour dim        { 0xff3a3f4b };
    inline const juce::Colour text       { 0xffd8dce4 };
    inline const juce::Colour accent     { 0xff4fc3f7 }; // notes switched on by hand
    inline const juce::Colour held       { 0xffffb74d }; // notes coming in through MIDI
    inline const juce::Colour root       { 0xffef5350 }; // chord root

    /**
        Everything a view draws. Pitches are N-TET steps (MIDI notes in 12-TET),
        so the views never assume 12 notes per octave.
    */
    struct Snapshot
    {
        int stepsPerOctave = 12;
        std::set<int> manual;          // switched on by clicking
        std::set<int> held;            // coming in through MIDI
        std::set<int> classes;         // pitch classes of everything that sounds
        int rootClass = -1;            // root of the recognised chord, -1 if none
        std::vector<int> stringFrets;  // fretboard: fret chosen on each string, -1 = none
        std::function<juce::String (int pitch)> nameOf;

        int classOf (int pitch) const noexcept { return mbox::instrument::wrap (pitch, stepsPerOctave); }
        bool isManual (int pitch) const { return manual.count (pitch) > 0; }
        bool isHeld (int pitch) const { return held.count (pitch) > 0; }
        bool isChordClass (int pitch) const { return classes.count (classOf (pitch)) > 0; }
        bool isRoot (int pitch) const { return rootClass >= 0 && classOf (pitch) == rootClass; }
    };

    /** Fill colour of an active note (manual and/or held). */
    inline juce::Colour activeColour (const Snapshot& s, int pitch)
    {
        if (s.isManual (pitch) && s.isHeld (pitch))
            return accent.interpolatedWith (held, 0.5f);
        return s.isHeld (pitch) ? held : accent;
    }
}
