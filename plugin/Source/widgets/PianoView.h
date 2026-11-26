#pragma once

#include "InstrumentShared.h"

/**
    Keyboard drawing for any equal temperament (layout from mbox::instrument::KeyboardLayout).
    Click a key to toggle it; the parent decides what toggling means.
*/
class PianoView final : public juce::Component
{
public:
    PianoView();

    /** Range in N-TET steps, inclusive. Also resets the layout when the step count changes. */
    void setRange (int stepsPerOctave, int firstPitch, int lastPitch);
    void setSnapshot (const instrument_ui::Snapshot& s);

    std::function<void (int pitch)> onKeyClicked;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> keyBounds (int pitch) const;
    double unitWidth() const;
    double firstX() const;

    mbox::instrument::KeyboardLayout layout = mbox::instrument::KeyboardLayout::forEqualTemperament (12);
    int first = 36;
    int last = 96;
    instrument_ui::Snapshot snapshot;

    static constexpr float upperHeight = 0.62f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoView)
};
