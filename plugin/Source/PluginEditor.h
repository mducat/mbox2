#pragma once

#include "PluginProcessor.h"
#include "widgets/WidgetRegistry.h"

#include <juce_gui_basics/juce_gui_basics.h>

/**
    Main window: a header with the shared settings (tuning, key) and the
    recognised chord, then one tab per widget of mbox::allWidgets().
*/
class MusicBox2Editor final : public juce::AudioProcessorEditor,
                              private mbox::AppState::Listener,
                              private juce::Timer
{
public:
    explicit MusicBox2Editor (MusicBox2Processor&);
    ~MusicBox2Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void appStateChanged (mbox::AppState&, int changes) override;

    void refreshHeader();
    void populateTonics();

    MusicBox2Processor& processor;
    mbox::AppState& state;
    std::array<std::uint64_t, 2> lastHeld {};

    juce::ComboBox tuningBox, tonicBox, scaleBox, namingBox;
    juce::Label chordLabel;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    std::vector<mbox::theory::Tuning> tuningPresets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MusicBox2Editor)
};
