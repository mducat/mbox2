#include "SpectrumEngine.h"

#include <algorithm>
#include <cmath>

namespace mbox::spectrum
{

SpectrumEngine::SpectrumEngine()
    : fifoData (static_cast<std::size_t> (fifo.getTotalSize())),
      history (static_cast<std::size_t> (kFftSize), 0.0f),
      hopBuffer (static_cast<std::size_t> (kHopSize), 0.0f),
      fftData (static_cast<std::size_t> (2 * kFftSize), 0.0f)
{
}

void SpectrumEngine::prepare (double newSampleRate, int maximumBlockSize)
{
    sampleRate.store (newSampleRate);
    synth.prepare (newSampleRate);
    instruments.prepare (newSampleRate);
    synthBuffer.setSize (1, maximumBlockSize, false, true, true);
    instrumentBuffer.setSize (1, maximumBlockSize, false, true, true);
    analysisBuffer.setSize (1, maximumBlockSize, false, true, true);
}

void SpectrumEngine::process (juce::AudioBuffer<float>& buffer, int numInputChannels, const juce::MidiBuffer& midi)
{
    const auto numSamples = buffer.getNumSamples();
    if (synthBuffer.getNumSamples() < numSamples)
    {
        // Host sent a bigger block than announced: not real-time safe, but better than crashing.
        synthBuffer.setSize (1, numSamples, false, true, true);
        instrumentBuffer.setSize (1, numSamples, false, true, true);
        analysisBuffer.setSize (1, numSamples, false, true, true);
    }

    const auto src = source.load (std::memory_order_relaxed);
    numInputChannels = juce::jmin (numInputChannels, buffer.getNumChannels());

    // 1. Grab the sidechain (mono mix) before the output overwrites the shared channels.
    analysisBuffer.clear (0, 0, numSamples);
    if (src == Source::sidechain && numInputChannels > 0)
    {
        const auto g = 1.0f / static_cast<float> (numInputChannels);
        for (int ch = 0; ch < numInputChannels; ++ch)
            analysisBuffer.addFrom (0, 0, buffer, ch, 0, numSamples, g);
    }

    buffer.clear();

    // 2. Internal synth (always runs, so switching source or unmuting is seamless).
    synthBuffer.clear (0, 0, numSamples);
    synth.render (synthBuffer, midi, 0, numSamples);

    // Piano / guitar previews from the widgets: always audible (they are only
    // triggered when the user asked for sound), analysed with the synth.
    instrumentBuffer.clear (0, 0, numSamples);
    instruments.render (instrumentBuffer.getWritePointer (0), numSamples);

    if (src == Source::internalSynth)
    {
        analysisBuffer.copyFrom (0, 0, synthBuffer, 0, 0, numSamples);
        analysisBuffer.addFrom (0, 0, instrumentBuffer, 0, 0, numSamples);
    }

    const bool audible = synthAudible.load (std::memory_order_relaxed);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        if (audible)
            buffer.copyFrom (ch, 0, synthBuffer, 0, 0, numSamples);
        buffer.addFrom (ch, 0, instrumentBuffer, 0, 0, numSamples);
    }

    // 3. Hand the analysed signal to the UI thread (dropped if the FIFO is full, e.g. editor closed).
    const auto scope = fifo.write (numSamples);
    const auto* data = analysisBuffer.getReadPointer (0);
    if (scope.blockSize1 > 0)
        std::copy_n (data, scope.blockSize1, fifoData.begin() + scope.startIndex1);
    if (scope.blockSize2 > 0)
        std::copy_n (data + scope.blockSize1, scope.blockSize2, fifoData.begin() + scope.startIndex2);
}

bool SpectrumEngine::pullFrame (std::vector<float>& magnitudesDb)
{
    // If the UI fell behind (editor just opened, stalled message thread), skip stale audio.
    if (const auto ready = fifo.getNumReady(); ready > kHopSize * 8)
        fifo.read (ready - kHopSize * 2);

    while (hopFilled < kHopSize && fifo.getNumReady() > 0)
    {
        const auto scope = fifo.read (juce::jmin (kHopSize - hopFilled, fifo.getNumReady()));
        auto* dest = hopBuffer.data() + hopFilled;
        std::copy_n (fifoData.begin() + scope.startIndex1, scope.blockSize1, dest);
        std::copy_n (fifoData.begin() + scope.startIndex2, scope.blockSize2, dest + scope.blockSize1);
        hopFilled += scope.blockSize1 + scope.blockSize2;
    }

    if (hopFilled < kHopSize)
        return false;

    hopFilled = 0;
    std::move (history.begin() + kHopSize, history.end(), history.begin());
    std::copy (hopBuffer.begin(), hopBuffer.end(), history.end() - kHopSize);

    std::copy (history.begin(), history.end(), fftData.begin());
    std::fill (fftData.begin() + kFftSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), static_cast<size_t> (kFftSize));
    fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

    // A Hann-windowed sine of amplitude A peaks at A * N / 4.
    constexpr float scale = 4.0f / static_cast<float> (kFftSize);
    magnitudesDb.resize (static_cast<std::size_t> (kNumBins));
    for (std::size_t k = 0; k < magnitudesDb.size(); ++k)
        magnitudesDb[k] = juce::Decibels::gainToDecibels (fftData[k] * scale, -140.0f);

    return true;
}

double SpectrumEngine::noteFrequency (int midiNote) const noexcept
{
    return synth.settings().noteFrequency[static_cast<std::size_t> (juce::jlimit (0, 127, midiNote))].load();
}

} // namespace mbox::spectrum
