#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

/**
    MIDI-oriented processor. For now it passes MIDI through unchanged and keeps
    track of the currently held notes so the editor can display them (and the
    chord they form). Theory logic lives in the JUCE-free mbox::theory library.
*/
class MusicBox2Processor final : public juce::AudioProcessor
{
public:
    MusicBox2Processor();
    ~MusicBox2Processor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    /** Held MIDI notes as a 128-bit mask (two 64-bit words), readable from the UI thread. */
    std::array<std::uint64_t, 2> getHeldNotes() const noexcept
    {
        return { heldNotes[0].load (std::memory_order_relaxed), heldNotes[1].load (std::memory_order_relaxed) };
    }

private:
    void setNote (int note, bool on) noexcept;

    std::array<std::atomic<std::uint64_t>, 2> heldNotes {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MusicBox2Processor)
};
