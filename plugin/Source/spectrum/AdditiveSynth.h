#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <span>

namespace mbox::spectrum
{

/**
    Settings shared between the UI thread (writer) and the audio thread (reader).
    The tuning is a plain table MIDI note -> Hz, so any tuning system (12-TET,
    N-TET, just intonation...) plugs in by filling the table.
*/
struct AdditiveSynthSettings
{
    AdditiveSynthSettings();

    static constexpr int kMaxSustained = 16;

    std::array<std::atomic<float>, 128> noteFrequency;
    std::atomic<int> numPartials { 12 };
    std::atomic<float> rolloff { 1.0f }; // partial n has amplitude 1 / n^rolloff

    // Notes held from the UI (toggled by hand in a widget), as frequencies; 0 = unused slot.
    std::array<std::atomic<float>, kMaxSustained> sustainedFrequency {};
    std::atomic<std::uint32_t> sustainedGeneration { 0 };

    // Audio thread only: frequency of each sustained slot currently playing.
    std::array<float, kMaxSustained> playingSustained {};
};

/**
    Minimal polyphonic additive synth: each note is a sum of ideal harmonic
    partials. It exists to give the spectrogram something to analyse when the
    plugin is used on its own (the plugin is otherwise silent).
*/
class AdditiveSynth
{
public:
    static constexpr int kMaxPartials = 32;
    static constexpr int kNumVoices   = 16;

    AdditiveSynth();

    void prepare (double sampleRate);

    /** Renders into `buffer` (all channels get the same signal), adding to its content. */
    void render (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, int startSample, int numSamples);

    /** UI thread: replaces the tuning table (frequencies of MIDI notes 0..127). */
    void setNoteFrequencies (std::span<const double> hz) noexcept;

    /**
        UI thread: notes sustained independently of the MIDI input (e.g. keys
        toggled by hand in a widget), as frequencies. At most kMaxSustained.
    */
    void setSustainedFrequencies (std::span<const double> hz) noexcept;

    AdditiveSynthSettings& settings() noexcept { return sharedSettings; }
    const AdditiveSynthSettings& settings() const noexcept { return sharedSettings; }

private:
    /** MIDI channel used internally for the sustained notes (note number = slot). */
    static constexpr int kSustainChannel = 16;

    void addSustainedNoteEvents (juce::MidiBuffer& events, int samplePosition);

    AdditiveSynthSettings sharedSettings;
    juce::Synthesiser synth;
    juce::MidiBuffer mergedMidi;
    std::uint32_t lastSustainedGeneration = 0;
};

} // namespace mbox::spectrum
