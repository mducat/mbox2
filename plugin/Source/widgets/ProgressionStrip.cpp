#include "ProgressionStrip.h"
#include "Widget.h"

namespace mbox
{

namespace
{
    constexpr int kChipWidth = 70;
    constexpr int kChipGap   = 6;
} // namespace

ProgressionStrip::ProgressionStrip (AppState& s) : state (s)
{
    clearButton.setButtonText ("Effacer");
    clearButton.onClick = [this] { state.clearProgression(); };
    addAndMakeVisible (clearButton);
    setTooltip (juce::String::fromUTF8 ("Clic : sélectionner un accord  -  Clic droit : le retirer"));
}

void ProgressionStrip::refresh()
{
    hovered = -1;
    repaint();
}

void ProgressionStrip::resized()
{
    auto area = getLocalBounds().reduced (10, 6);
    clearButton.setBounds (area.removeFromRight (72).withSizeKeepingCentre (72, 26));
    area.removeFromRight (kChipGap);
    chipsArea = area;
}

juce::Rectangle<int> ProgressionStrip::chipBounds (int index) const
{
    return { chipsArea.getX() + index * (kChipWidth + kChipGap), chipsArea.getY(), kChipWidth, chipsArea.getHeight() };
}

int ProgressionStrip::chipAt (juce::Point<int> p) const
{
    const int count = static_cast<int> (state.getProgression().size());
    for (int i = 0; i < count; ++i)
        if (chipBounds (i).contains (p))
            return i;
    return -1;
}

void ProgressionStrip::mouseUp (const juce::MouseEvent& e)
{
    const int i = chipAt (e.getPosition());
    if (i < 0)
        return;

    if (e.mods.isPopupMenu())
        state.removeChord (i);
    else
        state.setSelectedChordIndex (i == state.getSelectedChordIndex() ? -1 : i);
}

void ProgressionStrip::mouseMove (const juce::MouseEvent& e)
{
    if (const int i = chipAt (e.getPosition()); i != hovered)
    {
        hovered = i;
        repaint();
    }
}

void ProgressionStrip::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

void ProgressionStrip::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);

    const auto& chords = state.getProgression();
    if (chords.empty())
    {
        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (juce::String::fromUTF8 ("Cliquez sur des accords de la carte pour construire une progression"),
                    chipsArea, juce::Justification::centredLeft);
        return;
    }

    for (int i = 0; i < static_cast<int> (chords.size()); ++i)
    {
        const auto r = chipBounds (i);
        if (r.getRight() > chipsArea.getRight())
        {
            g.setColour (colours::textDim);
            g.drawText ("...", r.withWidth (20), juce::Justification::centred);
            break;
        }

        const auto& c = chords[static_cast<std::size_t> (i)];
        const auto colour = chipColour ? chipColour (c) : colours::accent;
        const bool selected = i == state.getSelectedChordIndex();

        g.setColour (selected ? colour : (i == hovered ? colour.withAlpha (0.25f) : colours::background));
        g.fillRoundedRectangle (r.toFloat().reduced (0.5f), 6.0f);
        g.setColour (colour);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 6.0f, 1.2f);

        auto text = r.reduced (3, 2);
        g.setColour (selected ? colours::background : colours::text);
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        const auto label = c.degree.empty() ? c.symbol : c.degree;
        g.drawFittedText (juce::String (label), text.removeFromTop (text.getHeight() * 3 / 5),
                          juce::Justification::centred, 1, 0.6f);
        g.setColour (selected ? colours::background.withAlpha (0.8f) : colours::textDim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawFittedText (juce::String (c.symbol), text, juce::Justification::centred, 1, 0.6f);
    }
}

} // namespace mbox
