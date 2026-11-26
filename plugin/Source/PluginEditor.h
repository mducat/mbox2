#pragma once

#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <mbox/theory/Theory.h>

/**
    Placeholder editor: draws the circle of fifths, highlights the held pitch
    classes, joins them into a polygon and shows the recognised chord.
*/
class MusicBox2Editor final : public juce::AudioProcessorEditor,
                              private juce::Timer
{
public:
    explicit MusicBox2Editor (MusicBox2Processor&);
    ~MusicBox2Editor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override {}

private:
    void timerCallback() override;

    MusicBox2Processor& processor;
    std::array<std::uint64_t, 2> lastHeld {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MusicBox2Editor)
};
