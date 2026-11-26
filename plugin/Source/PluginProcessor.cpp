#include "PluginProcessor.h"
#include "PluginEditor.h"

MusicBox2Processor::MusicBox2Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void MusicBox2Processor::prepareToPlay (double, int)
{
    heldNotes[0].store (0);
    heldNotes[1].store (0);
}

bool MusicBox2Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void MusicBox2Processor::setNote (int note, bool on) noexcept
{
    if (note < 0 || note > 127)
        return;

    auto& word = heldNotes[static_cast<std::size_t> (note / 64)];
    const auto bit = std::uint64_t { 1 } << (note % 64);

    if (on)
        word.fetch_or (bit, std::memory_order_relaxed);
    else
        word.fetch_and (~bit, std::memory_order_relaxed);
}

void MusicBox2Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // No audio: this plugin only works on MIDI.
    buffer.clear();

    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();

        if (msg.isNoteOn())
            setNote (msg.getNoteNumber(), true);
        else if (msg.isNoteOff())
            setNote (msg.getNoteNumber(), false);
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            heldNotes[0].store (0, std::memory_order_relaxed);
            heldNotes[1].store (0, std::memory_order_relaxed);
        }
    }

    // MIDI is passed through unchanged for now (it stays in `midi`).
}

juce::AudioProcessorEditor* MusicBox2Processor::createEditor()
{
    return new MusicBox2Editor (*this);
}

void MusicBox2Processor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("MusicBox2State");
    state.setProperty ("version", JucePlugin_VersionString, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void MusicBox2Processor::setStateInformation (const void* data, int sizeInBytes)
{
    // Nothing to restore yet; parameters will be added with the first features.
    juce::ignoreUnused (getXmlFromBinary (data, sizeInBytes));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MusicBox2Processor();
}
