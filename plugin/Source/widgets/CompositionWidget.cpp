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
    voiceLeadingToggle.onClick = [this] { score.setVoiceLeading (voiceLeadingToggle.getToggleState()); };
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

    score.onChordClicked = [this] (int index) { state.setSelectedChordIndex (index); };
    viewport.setViewedComponent (&score, false);
    viewport.setScrollBarsShown (false, true);
    addAndMakeVisible (viewport);

    refreshScore();
}

void CompositionWidget::appStateChanged (AppState&, int changes)
{
    if ((changes & (AppState::progressionChanged | AppState::keyChanged | AppState::tuningChanged)) != 0)
        refreshScore();
    if ((changes & AppState::selectionChanged) != 0)
        score.setSelectedIndex (state.getSelectedChordIndex());
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
