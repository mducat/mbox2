#include "PluginEditor.h"

namespace theory = mbox::theory;

namespace
{
    constexpr int kHeaderHeight = 56;

    std::vector<int> decode (const std::array<std::uint64_t, 2>& mask)
    {
        std::vector<int> notes;
        for (int n = 0; n < 128; ++n)
            if ((mask[static_cast<std::size_t> (n / 64)] >> (n % 64)) & 1u)
                notes.push_back (n);
        return notes;
    }
} // namespace

MusicBox2Editor::MusicBox2Editor (MusicBox2Processor& p)
    : AudioProcessorEditor (&p), processor (p), state (p.getAppState()),
      tuningPresets (theory::tunings::presets())
{
    // Tuning
    for (int i = 0; i < static_cast<int> (tuningPresets.size()); ++i)
        tuningBox.addItem (juce::String (tuningPresets[static_cast<std::size_t> (i)].name()), i + 1);
    tuningBox.setTooltip ("Tempérament");
    tuningBox.onChange = [this]
    {
        const int i = tuningBox.getSelectedItemIndex();
        if (juce::isPositiveAndBelow (i, static_cast<int> (tuningPresets.size())))
            state.setTuning (tuningPresets[static_cast<std::size_t> (i)].withConcertA (state.getTuning().concertA()));
    };

    // Key
    const auto scaleTypes = theory::scales::all();
    for (int i = 0; i < static_cast<int> (scaleTypes.size()); ++i)
        scaleBox.addItem (juce::String (std::string (scaleTypes[static_cast<std::size_t> (i)]->name)), i + 1);

    auto applyKey = [this, scaleTypes]
    {
        const int t = tonicBox.getSelectedItemIndex();
        const int s = scaleBox.getSelectedItemIndex();
        if (t >= 0 && juce::isPositiveAndBelow (s, static_cast<int> (scaleTypes.size())))
            state.setKey (theory::PitchClass (t, state.getDivisions()), *scaleTypes[static_cast<std::size_t> (s)]);
    };
    tonicBox.onChange = applyKey;
    scaleBox.onChange = applyKey;

    chordLabel.setJustificationType (juce::Justification::centredRight);
    chordLabel.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    chordLabel.setColour (juce::Label::textColourId, mbox::colours::text);

    for (auto* c : { &tuningBox, &tonicBox, &scaleBox })
        addAndMakeVisible (c);
    addAndMakeVisible (chordLabel);

    // Widgets
    tabs.setTabBarDepth (30);
    tabs.setOutline (0);
    for (const auto& info : mbox::allWidgets())
        tabs.addTab (info.title, mbox::colours::background, info.create (state).release(), true);
    tabs.setCurrentTabIndex (juce::jlimit (0, juce::jmax (0, tabs.getNumTabs() - 1), processor.lastTabIndex));
    addAndMakeVisible (tabs);

    state.addListener (this);
    refreshHeader();

    setResizable (true, true);
    setResizeLimits (640, 480, 2400, 1600);
    setSize (processor.lastEditorWidth, processor.lastEditorHeight);
    startTimerHz (30);
}

MusicBox2Editor::~MusicBox2Editor()
{
    processor.lastTabIndex = tabs.getCurrentTabIndex();
    processor.lastEditorWidth = getWidth();
    processor.lastEditorHeight = getHeight();
    state.removeListener (this);
}

void MusicBox2Editor::timerCallback()
{
    processor.applyPendingState();

    const auto held = processor.getHeldNotes();
    if (held != lastHeld)
    {
        lastHeld = held;
        state.setHeldMidiNotes (decode (held));
    }
}

void MusicBox2Editor::appStateChanged (mbox::AppState&, int)
{
    refreshHeader();
}

void MusicBox2Editor::populateTonics()
{
    tonicBox.clear (juce::dontSendNotification);
    const int n = state.getDivisions();
    for (int i = 0; i < n; ++i)
        tonicBox.addItem (juce::String (theory::PitchClass (i, n).name()), i + 1);
}

void MusicBox2Editor::refreshHeader()
{
    const auto& tuning = state.getTuning();

    if (tonicBox.getNumItems() != tuning.divisions())
        populateTonics();

    int tuningIndex = -1;
    for (int i = 0; i < static_cast<int> (tuningPresets.size()); ++i)
        if (tuningPresets[static_cast<std::size_t> (i)].divisions() == tuning.divisions()
            && tuningPresets[static_cast<std::size_t> (i)].isEqual() == tuning.isEqual())
            tuningIndex = i;
    if (tuningIndex >= 0)
        tuningBox.setSelectedItemIndex (tuningIndex, juce::dontSendNotification);
    else
        tuningBox.setText (juce::String (tuning.name()), juce::dontSendNotification);

    const auto& key = state.getKey();
    tonicBox.setSelectedItemIndex (key.tonic().value(), juce::dontSendNotification);
    const auto scaleTypes = theory::scales::all();
    for (int i = 0; i < static_cast<int> (scaleTypes.size()); ++i)
        if (scaleTypes[static_cast<std::size_t> (i)] == &key.type())
            scaleBox.setSelectedItemIndex (i, juce::dontSendNotification);

    const auto chords = state.getRecognisedChords();
    juce::String text;
    if (! state.getActiveNotes().empty())
        text = chords.empty() ? juce::String::fromUTF8 (state.describeActiveChord().c_str()) : juce::String (chords.front().symbol());
    chordLabel.setText (text, juce::dontSendNotification);
}

void MusicBox2Editor::paint (juce::Graphics& g)
{
    g.fillAll (mbox::colours::background);

    auto header = getLocalBounds().removeFromTop (kHeaderHeight).reduced (16, 0);
    g.setColour (mbox::colours::text);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("Music Box 2", header, juce::Justification::centredLeft);
}

void MusicBox2Editor::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (kHeaderHeight).reduced (16, 12);

    header.removeFromLeft (160); // title
    tuningBox.setBounds (header.removeFromLeft (100));
    header.removeFromLeft (12);
    tonicBox.setBounds (header.removeFromLeft (90));
    header.removeFromLeft (6);
    scaleBox.setBounds (header.removeFromLeft (170));
    chordLabel.setBounds (header);

    tabs.setBounds (area);
}
