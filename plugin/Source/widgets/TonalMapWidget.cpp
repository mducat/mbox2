#include "TonalMapWidget.h"

namespace mbox
{

TonalMapWidget::TonalMapWidget (AppState& s) : Widget (s), strip (s)
{
    map.onChordChosen = [this] (const theory::RomanChord& rc)
    {
        const int n = state.getDivisions();
        ProgressionChord pc { theory::Chord (rc.chord.root().convertedTo (n), rc.chord.quality()),
                              rc.voicingIn (n, 48),
                              rc.numeral(),
                              rc.symbol() };
        state.setSelectedChordIndex (state.appendChord (std::move (pc)));
    };

    strip.chipColour = [this] (const ProgressionChord& c)
    {
        const auto rc = findInKey (c);
        return rc ? functionColour (rc->function) : colours::textDim;
    };

    addAndMakeVisible (map);
    addAndMakeVisible (strip);

    syncKey();
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

    allChords.clear();
    for (const auto& rc : theory::chordsInKey (key))
        for (int inv = 0; inv < rc.size(); ++inv)
            if (const auto v = rc.withInversion (inv); v.inversion == inv)
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
}

void TonalMapWidget::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
}

} // namespace mbox
