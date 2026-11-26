#pragma once

#include "state/AppState.h"
#include "state/ManualNoteOutput.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

/**
    MIDI-oriented processor. For now it passes MIDI through unchanged and keeps
    track of the currently held notes so the editor can display them (and the
    chord they form). Theory logic lives in the JUCE-free mbox::theory library.
    It also owns the AppState shared by the editor's widgets, saved with the project.
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

    /** Shared UI state (tuning, key, progression, active notes). Message thread only. */
    mbox::AppState& getAppState() noexcept { return appState; }

    /** Plays the widgets' manual notes on the MIDI output (can be switched off). Message thread only. */
    mbox::ManualNoteOutput& getManualNoteOutput() noexcept { return manualNoteOutput; }

    /**
        Queues a note-on/off from the UI; it is sent on the MIDI output at the
        start of the next audio block. Message thread only (single producer).
    */
    void queueUiNote (int note, bool on) noexcept;

    /** Applies a state restored off the message thread, if any. Message thread only. */
    void applyPendingState();

    // Editor layout, remembered while the plugin instance lives (and saved).
    int lastTabIndex = 0;
    int lastEditorWidth = 900, lastEditorHeight = 640;

private:
    void setNote (int note, bool on) noexcept;

    std::array<std::atomic<std::uint64_t>, 2> heldNotes {};
    mbox::AppState appState;

    // UI -> audio thread note queue (lock-free, single producer / single consumer).
    juce::AbstractFifo uiNoteFifo { 512 };
    std::array<std::int16_t, 512> uiNotes {}; // note number for note-on, -(note + 1) for note-off
    mbox::ManualNoteOutput manualNoteOutput { appState, [this] (int note, bool on) { queueUiNote (note, on); } };
    juce::SpinLock pendingStateLock;
    juce::ValueTree pendingState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MusicBox2Processor)
};
