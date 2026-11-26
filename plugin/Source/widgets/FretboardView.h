#pragma once

#include "InstrumentShared.h"

#include <mbox/instrument/Fretboard.h>

/**
    Horizontal neck (nut on the left, lowest string at the bottom) for any
    tuning and any equal temperament: fret spacing follows 1 - 2^(-fret/N).

    Markers:
      - big filled dot:    a fret chosen on that string (by click or by a voicing)
      - small filled dot:  an exact pitch that is on (from the keyboard or MIDI) but not placed on a string
      - faint dot:         another place where a chord tone can be played
*/
class FretboardView final : public juce::Component
{
public:
    FretboardView();

    void setFretboard (const mbox::instrument::Fretboard& board);
    void setSnapshot (const instrument_ui::Snapshot& s);

    std::function<void (mbox::instrument::FretPosition)> onPositionClicked;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> neckArea() const;
    float fretX (int fret) const;          // x of the fret wire
    float noteX (int fret) const;          // x where a note on that fret is drawn
    float stringY (int string) const;

    mbox::instrument::Fretboard board = mbox::instrument::Fretboard::fromPreset (mbox::instrument::stringTunings::guitarStandard);
    instrument_ui::Snapshot snapshot;

    static constexpr float openColumn = 34.0f;
    static constexpr float labelColumn = 40.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FretboardView)
};
