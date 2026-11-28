#include "CompositionWidget.h"

namespace mbox
{

namespace
{
    // Chord tone sitting in the bass of `notes` (0 = root), or 0 if the bass is not a chord tone.
    int inversionOf (const theory::Chord& chord, const std::vector<int>& notes, int divisions)
    {
        if (notes.empty())
            return 0;
        const auto bass = theory::PitchClass::fromNote (notes.front(), divisions);
        const auto steps = chord.steps();
        for (std::size_t i = 0; i < steps.size(); ++i)
            if (chord.root().transposed (steps[i]) == bass)
                return static_cast<int> (i);
        return 0;
    }

    int noteTo12 (int note, int divisions)
    {
        if (divisions == theory::kDefaultDivisions)
            return note;
        const int octave = static_cast<int> (std::floor (static_cast<double> (note) / divisions));
        const int step = note - octave * divisions;
        return octave * 12 + theory::edo::convert (step, divisions, theory::kDefaultDivisions);
    }
} // namespace

CompositionWidget::CompositionWidget (AppState& s) : Widget (s)
{
    title.setText ("Composition", juce::dontSendNotification);
    title.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, colours::text);
    addAndMakeVisible (title);

    notice.setFont (juce::FontOptions (12.0f));
    notice.setColour (juce::Label::textColourId, colours::secondary);
    addAndMakeVisible (notice);

    voiceLeadingToggle.setTooltip (juce::String::fromUTF8 ("Réécrit les accords avec une conduite des voix douce au lieu des voicings de la progression"));
    voiceLeadingToggle.setColour (juce::ToggleButton::textColourId, colours::text);
    voiceLeadingToggle.onClick = [this]
    {
        score.setVoiceLeading (voiceLeadingToggle.getToggleState());
        updateVoicingControls();
    };
    addAndMakeVisible (voiceLeadingToggle);

    tabToggle.setToggleState (true, juce::dontSendNotification);
    tabToggle.setColour (juce::ToggleButton::textColourId, colours::text);
    tabToggle.onClick = [this]
    {
        score.setShowTablature (tabToggle.getToggleState());
        tuningBox.setEnabled (tabToggle.getToggleState());
        updateScoreSize();
    };
    addAndMakeVisible (tabToggle);

    const auto presets = instrument::stringTunings::presets();
    for (std::size_t i = 0; i < presets.size(); ++i)
        tuningBox.addItem (juce::String (std::string (presets[i]->name)), static_cast<int> (i) + 1);
    tuningBox.setSelectedItemIndex (0, juce::dontSendNotification);
    tuningBox.onChange = [this]
    {
        const auto all = instrument::stringTunings::presets();
        if (const int i = tuningBox.getSelectedItemIndex(); i >= 0 && i < static_cast<int> (all.size()))
            score.setTuning (*all[static_cast<std::size_t> (i)]);
        updateScoreSize();
    };
    addAndMakeVisible (tuningBox);

    playButton.setTooltip (juce::String::fromUTF8 ("Joue la progression (depuis l'accord sélectionné s'il y en a un)"));
    playButton.setClickingTogglesState (false);
    playButton.onClick = [this] { playing ? stopPlayback() : startPlayback(); };
    addAndMakeVisible (playButton);

    tempoLabel.setText ("Tempo", juce::dontSendNotification);
    tempoLabel.setColour (juce::Label::textColourId, colours::textDim);
    tempoLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (tempoLabel);

    tempoSlider.setRange (40.0, 220.0, 1.0);
    tempoSlider.setValue (90.0, juce::dontSendNotification);
    tempoSlider.setTextValueSuffix (" BPM");
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 70, 22);
    tempoSlider.setTooltip (juce::String::fromUTF8 ("Une mesure de 4 temps par accord"));
    addAndMakeVisible (tempoSlider);

    loopToggle.setColour (juce::ToggleButton::textColourId, colours::text);
    addAndMakeVisible (loopToggle);
    addAndMakeVisible (soundBox);

    voicingLabel.setColour (juce::Label::textColourId, colours::textDim);
    voicingLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (voicingLabel);
    previousVoicing.setTooltip (juce::String::fromUTF8 ("Voicing précédent de l'accord sélectionné (même basse)"));
    nextVoicing.setTooltip (juce::String::fromUTF8 ("Voicing suivant de l'accord sélectionné (même basse)"));
    previousVoicing.onClick = [this] { cycleVoicing (-1); };
    nextVoicing.onClick = [this] { cycleVoicing (1); };
    addAndMakeVisible (previousVoicing);
    addAndMakeVisible (nextVoicing);

    score.onChordClicked = [this] (int index)
    {
        state.setSelectedChordIndex (index);
        // Hear the chord clicked on the staff (not while the progression plays).
        if (! playing && juce::isPositiveAndBelow (index, static_cast<int> (state.getProgression().size())))
            playNotes (*this, state, state.getProgression()[static_cast<std::size_t> (index)].notes, std::nullopt, 0.75f, 0.0, 2.0);
    };
    viewport.setViewedComponent (&score, false);
    viewport.setScrollBarsShown (false, true);
    addAndMakeVisible (viewport);

    refreshScore();
    updateVoicingControls();
}

void CompositionWidget::appStateChanged (AppState&, int changes)
{
    if ((changes & AppState::notationChanged) != 0 && (changes & ~AppState::notationChanged) == 0)
        refreshScore(); // labels only: keep playing
    if ((changes & (AppState::progressionChanged | AppState::keyChanged | AppState::tuningChanged)) != 0)
    {
        stopPlayback(); // the scheduled notes no longer match the score
        refreshScore();
    }
    if ((changes & AppState::selectionChanged) != 0)
        score.setSelectedIndex (state.getSelectedChordIndex());
    if ((changes & (AppState::progressionChanged | AppState::selectionChanged | AppState::tuningChanged)) != 0)
        updateVoicingControls();
}

void CompositionWidget::updateVoicingControls()
{
    const auto& progression = state.getProgression();
    const int index = state.getSelectedChordIndex();
    const bool hasChord = juce::isPositiveAndBelow (index, static_cast<int> (progression.size()))
                          && ! progression[static_cast<std::size_t> (index)].notes.empty();

    juce::String text ("Voicing");
    bool enabled = false;
    if (hasChord && ! voiceLeadingToggle.getToggleState())
    {
        const auto& chord = progression[static_cast<std::size_t> (index)];
        const auto voicings = theory::keyboardVoicings (chord.chord, chord.notes.front());
        if (! voicings.empty())
        {
            const int current = theory::closestVoicingIndex (voicings, chord.notes);
            text << " " << (voicings[static_cast<std::size_t> (current)] == chord.notes ? juce::String (current + 1) : juce::String ("-"))
                 << "/" << static_cast<int> (voicings.size());
            enabled = voicings.size() > 1 || voicings.front() != chord.notes;
        }
    }

    voicingLabel.setText (text, juce::dontSendNotification);
    voicingLabel.setTooltip (voiceLeadingToggle.getToggleState()
                                 ? juce::String::fromUTF8 ("Désactive « Conduite des voix » pour choisir le voicing de chaque accord")
                                 : (hasChord ? juce::String() : juce::String::fromUTF8 ("Sélectionne un accord sur la portée")));
    previousVoicing.setEnabled (enabled);
    nextVoicing.setEnabled (enabled);
}

void CompositionWidget::cycleVoicing (int delta)
{
    const auto& progression = state.getProgression();
    const int index = state.getSelectedChordIndex();
    if (! juce::isPositiveAndBelow (index, static_cast<int> (progression.size())))
        return;

    auto chord = progression[static_cast<std::size_t> (index)];
    if (chord.notes.empty())
        return;

    const auto voicings = theory::keyboardVoicings (chord.chord, chord.notes.front());
    if (voicings.empty())
        return;

    const int count = static_cast<int> (voicings.size());
    const int current = theory::closestVoicingIndex (voicings, chord.notes);

    // From a voicing that is not in the list (set elsewhere), the first step lands on the closest one.
    const int next = voicings[static_cast<std::size_t> (current)] == chord.notes ? (current + delta + count) % count : current;

    chord.notes = voicings[static_cast<std::size_t> (next)];
    state.replaceChord (index, chord);
    playNotes (*this, state, chord.notes, std::nullopt, 0.75f, 0.0, 2.0);
}

double CompositionWidget::secondsPerChord() const
{
    return 4.0 * 60.0 / juce::jmax (1.0, tempoSlider.getValue());
}

void CompositionWidget::startPlayback()
{
    const auto& progression = state.getProgression();
    if (progression.empty() || ! SoundSelector::chosenInstrument (*this))
        return;

    stopAllNotes (*this);

    const int selected = state.getSelectedChordIndex();
    playStartIndex = juce::isPositiveAndBelow (selected, static_cast<int> (progression.size())) && selected < static_cast<int> (progression.size()) - 1 ? selected : 0;
    playCount = static_cast<int> (progression.size()) - playStartIndex;
    playChordSeconds = secondsPerChord();

    // Everything is scheduled now, sample-accurately, on the audio thread.
    for (int i = 0; i < playCount; ++i)
        playNotes (*this, state, progression[static_cast<std::size_t> (playStartIndex + i)].notes, std::nullopt,
                   0.75f, i * playChordSeconds, playChordSeconds * 0.97);

    playing = true;
    playingIndex = -1;
    playStartMs = juce::Time::getMillisecondCounterHiRes();
    playButton.setButtonText ("Stop");
    tempoSlider.setEnabled (false);
    startTimerHz (30);
    timerCallback();
}

void CompositionWidget::stopPlayback()
{
    if (! playing)
        return;
    playing = false;
    stopTimer();
    stopAllNotes (*this);
    playButton.setButtonText ("Play");
    tempoSlider.setEnabled (true);
}

void CompositionWidget::timerCallback()
{
    const double elapsed = (juce::Time::getMillisecondCounterHiRes() - playStartMs) / 1000.0;
    const int step = static_cast<int> (elapsed / playChordSeconds);

    if (step >= playCount)
    {
        const bool again = loopToggle.getToggleState();
        playing = false; // let the last chord ring
        stopTimer();
        playButton.setButtonText ("Play");
        tempoSlider.setEnabled (true);
        if (again)
        {
            state.setSelectedChordIndex (-1); // loop from the start
            startPlayback();
        }
        return;
    }

    if (step != playingIndex)
    {
        playingIndex = step;
        state.setSelectedChordIndex (playStartIndex + step); // the other widgets follow along
    }
}

void CompositionWidget::refreshScore()
{
    const int n = state.getDivisions();

    composition::ScoreInput in;
    in.divisions = n;
    in.approximated = n != theory::kDefaultDivisions;
    in.key = theory::SpelledKey::fromScale (state.getKey());

    for (const auto& pc : state.getProgression())
    {
        composition::ScoreChord c { theory::Chord (pc.chord.root().convertedTo (theory::kDefaultDivisions), pc.chord.quality()), {}, 0, {}, {}, {}, std::nullopt };

        for (const int note : pc.notes)
            c.notes.push_back (noteTo12 (note, n));
        c.inversion = inversionOf (pc.chord, pc.notes, n);
        c.roman = pc.degree;
        // The chord symbol is recomputed from the spelling so it matches the staff (Db, not C#).

        // Tablature in the real tuning: N-TET frets.
        for (const auto p : pc.chord.pitchClasses().toVector())
            c.tabClasses.push_back (p.value());
        if (! pc.notes.empty())
            c.tabBass = theory::PitchClass::fromNote (pc.notes.front(), n).value();

        in.chords.push_back (std::move (c));
    }

    notice.setText (in.approximated ? juce::String::fromUTF8 ("Portée approchée en 12-TET · tablature en ") + juce::String (n) + "-TET"
                                    : juce::String(),
                    juce::dontSendNotification);

    score.setInput (in);
    score.setSelectedIndex (state.getSelectedChordIndex());
    updateScoreSize();
    repaint();
}

void CompositionWidget::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    if (state.getProgression().empty())
    {
        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText (juce::String::fromUTF8 ("Ajoute des accords depuis la carte tonale pour les voir sur la portée."),
                    viewport.getBounds().removeFromTop (40), juce::Justification::centred);
    }
}

void CompositionWidget::resized()
{
    auto area = getLocalBounds().reduced (8);
    auto header = area.removeFromTop (26);
    title.setBounds (header.removeFromLeft (110));
    tuningBox.setBounds (header.removeFromRight (170).reduced (0, 2));
    header.removeFromRight (8);
    tabToggle.setBounds (header.removeFromRight (100));
    voiceLeadingToggle.setBounds (header.removeFromRight (160));
    notice.setBounds (header);

    area.removeFromTop (4);
    auto transport = area.removeFromTop (26);
    playButton.setBounds (transport.removeFromLeft (70).reduced (0, 1));
    transport.removeFromLeft (8);
    tempoLabel.setBounds (transport.removeFromLeft (50));
    tempoSlider.setBounds (transport.removeFromLeft (150));
    transport.removeFromLeft (8);
    loopToggle.setBounds (transport.removeFromLeft (80));
    soundBox.setBounds (transport.removeFromLeft (140).reduced (0, 2));

    nextVoicing.setBounds (transport.removeFromRight (28).reduced (0, 1));
    voicingLabel.setBounds (transport.removeFromRight (100));
    previousVoicing.setBounds (transport.removeFromRight (28).reduced (0, 1));

    area.removeFromTop (4);
    viewport.setBounds (area);
    updateScoreSize();
}

void CompositionWidget::updateScoreSize()
{
    viewport.setVisible (! state.getProgression().empty());

    // Fit the system to the available height (minus the horizontal scroll bar).
    const float available = static_cast<float> (viewport.getHeight() - viewport.getScrollBarThickness());
    score.setStaffSpace (9.0f);
    const float neededAt9 = static_cast<float> (score.preferredHeight());
    if (available > 0.0f && neededAt9 > 0.0f)
        score.setStaffSpace (9.0f * available / neededAt9);
}

} // namespace mbox
