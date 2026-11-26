#include "SoundSupport.h"

#include "../PluginProcessor.h"

#include <algorithm>

namespace mbox
{

MusicBox2Processor* findProcessor (const juce::Component& c)
{
    if (auto* editor = c.findParentComponentOfClass<juce::AudioProcessorEditor>())
        return dynamic_cast<MusicBox2Processor*> (editor->getAudioProcessor());
    return nullptr;
}

SoundSelector::SoundSelector()
{
    addItem ("Son : piano", soundPiano);
    addItem ("Son : guitare", soundGuitar);
    addItem ("Son : muet", soundMuted);
    setSelectedId (soundPiano, juce::dontSendNotification);
    setTooltip (juce::String::fromUTF8 ("Son joué au clic (accords, Play). Partagé par tous les onglets."));
}

void SoundSelector::parentHierarchyChanged()
{
    ComboBox::parentHierarchyChanged();
    if (bound)
        return;
    if (auto* processor = findProcessor (*this))
    {
        getSelectedIdAsValue().referTo (processor->getSoundChoice());
        bound = true;
    }
}

std::optional<sound::Instrument> SoundSelector::chosenInstrument (const juce::Component& anyWidget)
{
    if (auto* processor = findProcessor (anyWidget))
    {
        switch (static_cast<int> (processor->getSoundChoice().getValue()))
        {
            case soundGuitar: return sound::Instrument::guitar;
            case soundMuted:  return std::nullopt;
            default:          return sound::Instrument::piano;
        }
    }
    return std::nullopt;
}

void playNotes (const juce::Component& from, const AppState& state, std::vector<int> notes,
                std::optional<sound::Instrument> instrument, float velocity, double delaySeconds, double lengthSeconds)
{
    auto* processor = findProcessor (from);
    if (processor == nullptr || notes.empty())
        return;

    // Muted silences everything, including the viewer's forced piano / guitar.
    const auto chosen = SoundSelector::chosenInstrument (from);
    if (! chosen)
        return;
    const auto inst = instrument.value_or (*chosen);

    std::sort (notes.begin(), notes.end());
    std::vector<double> hz;
    for (const int n : notes)
        hz.push_back (state.getTuning().frequencyOf (n));

    processor->getSpectrumEngine().getInstruments().playChord (hz, inst, velocity, delaySeconds, lengthSeconds);
}

void stopAllNotes (const juce::Component& from)
{
    if (auto* processor = findProcessor (from))
        processor->getSpectrumEngine().getInstruments().stopAll();
}

} // namespace mbox
