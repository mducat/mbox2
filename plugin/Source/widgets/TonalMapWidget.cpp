#include "TonalMapWidget.h"

namespace mbox
{

TonalMapWidget::TonalMapWidget (AppState& s) : Widget (s), strip (s)
{
    map.onChordChosen = [this] (const theory::RomanChord& rc)
    {
        const auto index = state.appendChord (toProgressionChord (rc));
        state.setSelectedChordIndex (index);
        playNotes (*this, state, state.getProgression()[static_cast<std::size_t> (index)].notes, std::nullopt, 0.75f, 0.0, 2.0);
    };

    map.onPatternAdd = [this] (const std::vector<theory::RomanChord>& chords)
    {
        int last = -1;
        for (const auto& rc : chords)
            last = state.appendChord (toProgressionChord (rc));
        state.setSelectedChordIndex (last);
    };

    map.onPatternPlay = [this] (const std::vector<theory::RomanChord>& chords)
    {
        stopAllNotes (*this);
        const double step = 0.9;
        for (std::size_t i = 0; i < chords.size(); ++i)
            playNotes (*this, state, toProgressionChord (chords[i]).notes, std::nullopt, 0.75f,
                       step * static_cast<double> (i), step * 0.95);
    };

    strip.chipColour = [this] (const ProgressionChord& c)
    {
        const auto rc = findInKey (c);
        return rc ? functionColour (rc->function) : colours::textDim;
    };

    addAndMakeVisible (map);
    addAndMakeVisible (strip);
    addAndMakeVisible (soundBox); // over the right end of the map's toolbar

    syncKey();
}

ProgressionChord TonalMapWidget::toProgressionChord (const theory::RomanChord& rc) const
{
    const int n = state.getDivisions();
    return { theory::Chord (rc.chord.root().convertedTo (n), rc.chord.quality()),
             rc.voicingIn (n, 48),
             rc.numeral(),
             rc.symbol() };
}

theory::Key TonalMapWidget::keyFromScale (const theory::Scale& scale)
{
    const auto steps = scale.type().stepsIn (theory::kDefaultDivisions);
    const bool minorThird = std::find (steps.begin(), steps.end(), 3) != steps.end()
                         && std::find (steps.begin(), steps.end(), 4) == steps.end();
    return { scale.tonic().convertedTo (theory::kDefaultDivisions),
             minorThird ? theory::Mode::Minor : theory::Mode::Major };
}

void TonalMapWidget::appStateChanged (AppState&, int changes)
{
    if (changes & (AppState::keyChanged | AppState::tuningChanged))
        syncKey();
    else if (changes & (AppState::progressionChanged | AppState::selectionChanged))
        syncSelection();
}

void TonalMapWidget::syncKey()
{
    const auto key = keyFromScale (state.getKey());
    map.setKey (key);

    // Chords as generated first (so "i6/4" is the cadential 6/4), then every other inversion.
    allChords = theory::chordsInKey (key);
    const auto generated = allChords;
    for (const auto& rc : generated)
        for (int inv = 0; inv < rc.size(); ++inv)
            if (const auto v = rc.withInversion (inv); v.inversion == inv && inv != rc.inversion)
                allChords.push_back (v);

    syncSelection();
}

std::optional<theory::RomanChord> TonalMapWidget::findInKey (const ProgressionChord& c) const
{
    const auto root12 = c.chord.root().convertedTo (theory::kDefaultDivisions);
    for (const auto& rc : allChords)
        if (rc.chord.root() == root12 && &rc.chord.quality() == &c.chord.quality() && rc.numeral() == c.degree)
            return rc;
    return std::nullopt;
}

void TonalMapWidget::syncSelection()
{
    std::vector<theory::RomanChord> selection;
    for (const auto& c : state.getProgression())
        if (auto rc = findInKey (c))
            selection.push_back (*rc);

    map.setSelection (selection);
    strip.refresh();
}

void TonalMapWidget::resized()
{
    auto area = getLocalBounds();
    strip.setBounds (area.removeFromBottom (64));
    map.setBounds (area);
    soundBox.setBounds (area.reduced (10, 8).removeFromTop (TonalMapComponent::kToolbarRowHeight)
                            .removeFromRight (TonalMapComponent::kToolbarReserve - 10).reduced (0, 3));
}

void TonalMapWidget::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
}

} // namespace mbox
