#pragma once

#include "AdditiveSynth.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <atomic>
#include <vector>

namespace mbox::spectrum
{

/** What the spectrogram listens to. */
enum class Source : int
{
    internalSynth = 0, // the built-in additive synth playing the incoming MIDI notes
    sidechain     = 1  // audio routed from the DAW into the plugin's input bus
};

/**
    Audio side of the harmonic decomposition widget.

    Audio thread: process() renders the additive synth, picks the analysed
    signal and pushes it into a lock-free FIFO.
    UI thread:    pullFrame() drains the FIFO and computes FFT magnitude frames.
*/
class SpectrumEngine
{
public:
    static constexpr int kFftOrder = 13;
    static constexpr int kFftSize  = 1 << kFftOrder; // 8192 -> ~5.9 Hz bins at 48 kHz
    static constexpr int kHopSize  = kFftSize / 8;
    static constexpr int kNumBins  = kFftSize / 2 + 1;

    SpectrumEngine();

    void prepare (double sampleRate, int maximumBlockSize);

    /**
        Audio thread. The first numInputChannels channels of `buffer` hold the
        sidechain input; on return `buffer` holds the plugin output (silence,
        plus the synth if it is audible).
    */
    void process (juce::AudioBuffer<float>& buffer, int numInputChannels, const juce::MidiBuffer& midi);

    /**
        UI thread. Computes at most one new frame of magnitudes in dBFS (a full
        scale sine reads 0 dB), kNumBins values. Returns false when not enough
        new audio is available yet: call it in a loop until it returns false.
    */
    bool pullFrame (std::vector<float>& magnitudesDb);

    double getSampleRate() const noexcept { return sampleRate.load(); }
    double getBinWidth() const noexcept { return getSampleRate() / kFftSize; }

    /** Frequency of a MIDI note in the current tuning (same table as the synth). */
    double noteFrequency (int midiNote) const noexcept;

    AdditiveSynth& getSynth() noexcept { return synth; }

    std::atomic<Source> source { Source::internalSynth };
    std::atomic<bool> synthAudible { false }; // muted by default: the plugin stays a silent MIDI tool

private:
    AdditiveSynth synth;
    juce::AudioBuffer<float> synthBuffer, analysisBuffer;
    std::atomic<double> sampleRate { 44100.0 };

    // Audio -> UI FIFO
    juce::AbstractFifo fifo { 1 << 15 };
    std::vector<float> fifoData;

    // UI-thread analysis state
    juce::dsp::FFT fft { kFftOrder };
    juce::dsp::WindowingFunction<float> window { static_cast<size_t> (kFftSize), juce::dsp::WindowingFunction<float>::hann, false };
    std::vector<float> history, hopBuffer, fftData;
    int hopFilled = 0;
};

} // namespace mbox::spectrum
