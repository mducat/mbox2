#include "InstrumentWidget.h"

#include "../PluginProcessor.h"

namespace mbox
{

namespace inst = instrument;
using namespace instrument_ui;

InstrumentWidget::InstrumentWidget (AppState& s) : Widget (s)
{
    viewBox.addItem ("Piano + guitare", static_cast<int> (ViewMode::both));
    viewBox.addItem ("Piano", static_cast<int> (ViewMode::piano));
    viewBox.addItem ("Guitare", static_cast<int> (ViewMode::guitar));
    viewBox.setSelectedId (static_cast<int> (ViewMode::both), juce::dontSendNotification);
    viewBox.onChange = [this] { resized(); };
    addAndMakeVisible (viewBox);

    const auto presets = inst::stringTunings::presets();
    for (std::size_t i = 0; i < presets.size(); ++i)
        stringTuningBox.addItem (juce::String::fromUTF8 (std::string (presets[i]->name).c_str()), static_cast<int> (i) + 1);
    stringTuningBox.setSelectedId (1, juce::dontSendNotification);
    stringTuningBox.onChange = [this]
    {
        const auto list = inst::stringTunings::presets();
        const int index = stringTuningBox.getSelectedId() - 1;
        if (index >= 0 && index < static_cast<int> (list.size()))
        {
            stringTuning = list[static_cast<std::size_t> (index)];
            rebuildFretboard();
            refresh();
        }
    };
    addAndMakeVisible (stringTuningBox);

    prevVoicing.setTooltip (juce::String::fromUTF8 ("Doigté précédent de cet accord"));
    nextVoicing.setTooltip (juce::String::fromUTF8 ("Doigté suivant de cet accord"));
    prevVoicing.onClick = [this] { stepVoicing (-1); };
    nextVoicing.onClick = [this] { stepVoicing (+1); };
    addAndMakeVisible (prevVoicing);
    addAndMakeVisible (nextVoicing);

    voicingLabel.setJustificationType (juce::Justification::centred);
    voicingLabel.setColour (juce::Label::textColourId, text);
    addAndMakeVisible (voicingLabel);

    clearButton.onClick = [this]
    {
        std::fill (stringFrets.begin(), stringFrets.end(), -1);
        voicingIndex = -1;
        state.clearManualNotes();
    };
    addAndMakeVisible (clearButton);

    midiOutButton.setButtonText ("Sortie MIDI");
    midiOutButton.setToggleState (true, juce::dontSendNotification);
    midiOutButton.setTooltip (juce::String::fromUTF8 ("Joue les notes activées à la main sur la sortie MIDI du plugin"));
    midiOutButton.setColour (juce::ToggleButton::textColourId, text);
    midiOutButton.onClick = [this]
    {
        if (auto* out = findMidiOutput())
            out->setEnabled (midiOutButton.getToggleState());
    };
    addAndMakeVisible (midiOutButton);

    piano.onKeyClicked = [this] (int note) { togglePitch (note); };
    fretboard.onPositionClicked = [this] (inst::FretPosition pos) { togglePosition (pos); };
    addAndMakeVisible (piano);
    addAndMakeVisible (fretboard);

    rebuildForTuning();
    refresh();
}

ManualNoteOutput* InstrumentWidget::findMidiOutput() const
{
    if (auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>())
        if (auto* processor = dynamic_cast<MusicBox2Processor*> (editor->getAudioProcessor()))
            return &processor->getManualNoteOutput();
    return nullptr;
}

void InstrumentWidget::parentHierarchyChanged()
{
    // The output's flag is shared by every editor instance: show its real state.
    const auto* out = findMidiOutput();
    midiOutButton.setEnabled (out != nullptr);
    if (out != nullptr)
        midiOutButton.setToggleState (out->isEnabled(), juce::dontSendNotification);
}

//==============================================================================
void InstrumentWidget::appStateChanged (AppState&, int changes)
{
    if ((changes & AppState::tuningChanged) != 0)
        rebuildForTuning();

    if ((changes & AppState::activeNotesChanged) != 0)
    {
        // A note switched off elsewhere (another widget, "clear"...) leaves its string.
        const auto manual = manualNotes();
        for (int s = 0; s < board.numStrings(); ++s)
            if (auto& f = stringFrets[static_cast<std::size_t> (s)];
                f >= 0 && ! std::binary_search (manual.begin(), manual.end(), board.pitchAt (s, f)))
                f = -1;
    }

    refresh();
}

void InstrumentWidget::rebuildForTuning()
{
    divisions = state.getDivisions();

    // Five octaves in 12-TET, three in finer tunings so the keys stay clickable.
    const int low  = divisions <= 12 ? 36 : 48;
    const int high = divisions <= 12 ? 96 : 84;
    piano.setRange (divisions, inst::pitchFromTwelveTet (low, divisions), inst::pitchFromTwelveTet (high, divisions));

    rebuildFretboard();
}

void InstrumentWidget::rebuildFretboard()
{
    board = inst::Fretboard::fromPreset (*stringTuning, 22, divisions);
    fretboard.setFretboard (board);
    resized(); // string count may change

    // Notes stay on, but their string positions belong to the old neck.
    stringFrets.assign (static_cast<std::size_t> (board.numStrings()), -1);
    voicings.clear();
    voicingKey = { {}, -1 };
    voicingIndex = -1;
}

std::vector<int> InstrumentWidget::manualNotes() const
{
    return state.getManualNotes(); // sorted, unique
}

//==============================================================================
void InstrumentWidget::togglePitch (int note)
{
    for (int s = 0; s < board.numStrings(); ++s)
        if (auto& f = stringFrets[static_cast<std::size_t> (s)]; f >= 0 && board.pitchAt (s, f) == note)
            f = -1;

    voicingIndex = -1;
    state.toggleManualNote (note);
}

void InstrumentWidget::togglePosition (inst::FretPosition pos)
{
    if (pos.string < 0 || pos.string >= board.numStrings())
        return;

    auto manual = manualNotes();

    auto releaseString = [&] (int string)
    {
        auto& f = stringFrets[static_cast<std::size_t> (string)];
        if (f < 0)
            return;
        const int note = board.pitchAt (string, f);
        f = -1;

        bool stillPlaced = false;
        for (int s = 0; s < board.numStrings(); ++s)
            if (const int other = stringFrets[static_cast<std::size_t> (s)]; other >= 0 && board.pitchAt (s, other) == note)
                stillPlaced = true;
        if (! stillPlaced)
            manual.erase (std::remove (manual.begin(), manual.end(), note), manual.end());
    };

    const bool sameFret = stringFrets[static_cast<std::size_t> (pos.string)] == pos.fret;
    releaseString (pos.string);
    if (! sameFret)
    {
        stringFrets[static_cast<std::size_t> (pos.string)] = pos.fret;
        manual.push_back (board.pitchAt (pos));
    }

    voicingIndex = -1;
    state.setManualNotes (std::move (manual));
    refresh(); // string choice may change without the notes changing
}

void InstrumentWidget::stepVoicing (int delta)
{
    // The chord to finger: what is playing, or else the chord selected in the progression.
    auto notes = state.getActiveNotes();
    if (notes.empty())
        if (const auto* selected = state.getSelectedChord())
            notes = selected->notes;
    if (notes.empty())
        return;

    std::vector<int> classes;
    for (const int n : notes)
        classes.push_back (inst::wrap (n, divisions));
    std::sort (classes.begin(), classes.end());
    classes.erase (std::unique (classes.begin(), classes.end()), classes.end());
    const int bassClass = inst::wrap (*std::min_element (notes.begin(), notes.end()), divisions);

    if (voicingKey.first != classes || voicingKey.second != bassClass)
    {
        voicings = inst::findVoicings (board, classes, bassClass);
        voicingKey = { classes, bassClass };
        voicingIndex = -1;
    }

    if (voicings.empty())
    {
        refresh();
        return;
    }

    const int count = static_cast<int> (voicings.size());
    const int index = voicingIndex < 0 ? (delta > 0 ? 0 : count - 1) : inst::wrap (voicingIndex + delta, count);

    const auto& v = voicings[static_cast<std::size_t> (index)];
    stringFrets = v.frets;
    voicingIndex = index;
    state.setManualNotes (v.pitches (board));
    refresh();
}

//==============================================================================
juce::String InstrumentWidget::nameOf (int note) const
{
    return juce::String::fromUTF8 (state.getTuning().pitchClassOf (note).name().c_str());
}

void InstrumentWidget::refresh()
{
    const auto active = state.getActiveNotes();
    const auto& held = state.getHeldNotes();
    const auto manual = manualNotes();

    Snapshot snap;
    snap.stepsPerOctave = divisions;
    snap.manual.insert (manual.begin(), manual.end());
    snap.held.insert (held.begin(), held.end());
    snap.stringFrets = stringFrets;
    snap.nameOf = [this] (int n) { return nameOf (n); };

    chordName = "-";
    chordDetails = {};

    if (! active.empty())
    {
        for (const int n : active)
            snap.classes.insert (inst::wrap (n, divisions));

        juce::StringArray noteNames;
        for (const int pc : snap.classes)
            noteNames.add (nameOf (pc));
        chordDetails = noteNames.joinIntoString (" ");

        const auto bass = state.getTuning().pitchClassOf (active.front());
        const auto readings = state.getRecognisedChords();
        auto symbolOf = [&] (const theory::Chord& c)
        {
            juce::String sym = juce::String::fromUTF8 (c.symbol().c_str());
            if (c.root() != bass)
                sym << "/" << juce::String::fromUTF8 (bass.name().c_str());
            return sym;
        };

        if (readings.empty())
        {
            chordName = "?";
        }
        else
        {
            snap.rootClass = readings.front().root().value();
            chordName = symbolOf (readings.front());

            juce::StringArray others;
            for (std::size_t i = 1; i < readings.size(); ++i)
                others.add (symbolOf (readings[i]));
            if (! others.isEmpty())
                chordDetails << "   (ou " << others.joinIntoString (", ") << ")";
        }
    }
    else if (const auto* selected = state.getSelectedChord())
    {
        // Nothing playing: show where the selected progression chord lies.
        for (const int n : selected->notes)
            snap.classes.insert (inst::wrap (n, divisions));
        snap.rootClass = selected->chord.root().value();
        chordName = juce::String::fromUTF8 (selected->symbol.c_str());
        chordDetails = juce::String::fromUTF8 ("accord sélectionné")
                     + (selected->degree.empty() ? juce::String() : juce::String (" (") + juce::String::fromUTF8 (selected->degree.c_str()) + ")")
                     + juce::String::fromUTF8 (" - « > » pour un doigté");
    }

    if (voicingIndex >= 0 && ! voicings.empty())
        voicingLabel.setText (juce::String::fromUTF8 ("Doigté ") + juce::String (voicingIndex + 1) + "/" + juce::String (static_cast<int> (voicings.size())),
                              juce::dontSendNotification);
    else if (voicingKey.second >= 0 && voicings.empty())
        voicingLabel.setText (juce::String::fromUTF8 ("Aucun doigté"), juce::dontSendNotification);
    else
        voicingLabel.setText (juce::String::fromUTF8 ("Doigté"), juce::dontSendNotification);

    piano.setSnapshot (snap);
    fretboard.setSnapshot (snap);
    repaint (headerArea);
}

//==============================================================================
void InstrumentWidget::paint (juce::Graphics& g)
{
    g.fillAll (background);

    auto area = headerArea;

    auto legend = area.removeFromRight (270).withSizeKeepingCentre (270, 20);
    auto drawLegend = [&] (juce::Colour c, const juce::String& label, int width)
    {
        auto row = legend.removeFromLeft (width);
        g.setColour (c);
        g.fillEllipse (row.removeFromLeft (14).withSizeKeepingCentre (10, 10).toFloat());
        g.setColour (juce::Colours::grey);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (label, row.withTrimmedLeft (3), juce::Justification::centredLeft);
    };
    drawLegend (accent, "clic", 60);
    drawLegend (instrument_ui::held, "MIDI in", 76);
    drawLegend (root, "fondamentale", 120);

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    const int nameWidth = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), chordName) + 20;
    g.drawText (chordName, area.removeFromLeft (juce::jmax (90, nameWidth)), juce::Justification::centredLeft);

    g.setColour (juce::Colours::lightgrey);
    g.setFont (juce::FontOptions (14.0f));
    g.drawText (chordDetails, area, juce::Justification::centredLeft);
}

int InstrumentWidget::maxFretboardHeight() const
{
    return board.numStrings() * 40 + 30;
}

void InstrumentWidget::resized()
{
    auto area = getLocalBounds().reduced (8);

    headerArea = area.removeFromTop (44);
    area.removeFromTop (4);

    auto toolbar = area.removeFromTop (28);
    viewBox.setBounds (toolbar.removeFromLeft (140));
    toolbar.removeFromLeft (6);
    stringTuningBox.setBounds (toolbar.removeFromLeft (230));
    toolbar.removeFromLeft (10);
    prevVoicing.setBounds (toolbar.removeFromLeft (28));
    voicingLabel.setBounds (toolbar.removeFromLeft (110));
    nextVoicing.setBounds (toolbar.removeFromLeft (28));
    midiOutButton.setBounds (toolbar.removeFromRight (110));
    toolbar.removeFromRight (6);
    clearButton.setBounds (toolbar.removeFromRight (70));
    area.removeFromTop (8);

    const auto mode = static_cast<ViewMode> (viewBox.getSelectedId());
    const bool showPiano = mode != ViewMode::guitar;
    const bool showGuitar = mode != ViewMode::piano;
    piano.setVisible (showPiano);
    fretboard.setVisible (showGuitar);
    stringTuningBox.setEnabled (showGuitar);

    if (showPiano && showGuitar)
    {
        piano.setBounds (area.removeFromTop (juce::jmin (150, area.getHeight() * 2 / 5)));
        area.removeFromTop (10);
        fretboard.setBounds (area.withHeight (juce::jmin (area.getHeight(), maxFretboardHeight())));
    }
    else if (showPiano)
    {
        piano.setBounds (area.withSizeKeepingCentre (area.getWidth(), juce::jmin (area.getHeight(), 220)));
    }
    else
    {
        fretboard.setBounds (area.withSizeKeepingCentre (area.getWidth(), juce::jmin (area.getHeight(), maxFretboardHeight())));
    }
}

} // namespace mbox
