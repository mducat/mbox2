#include "InstrumentPlayer.h"

#include <cmath>

namespace mbox::sound
{

void InstrumentPlayer::push (const Command& c)
{
    // Dropped if the FIFO is full (audio not running).
    fifo.write (1).forEach ([&] (int index) { commands[static_cast<std::size_t> (index)] = c; });
}

void InstrumentPlayer::play (double frequencyHz, Instrument instrument, float velocity, double delaySeconds, double lengthSeconds)
{
    if (! enabled.load() || ! (frequencyHz > 0.0))
        return;

    const auto sr = sampleRate.load();
    Command c;
    c.event.frequencyHz = frequencyHz;
    c.event.instrument = instrument;
    c.event.velocity = juce::jlimit (0.0f, 1.0f, velocity);
    c.event.startSample = static_cast<std::int64_t> (std::llround (juce::jmax (0.0, delaySeconds) * sr));
    c.event.lengthSamples = lengthSeconds > 0.0 ? static_cast<std::int64_t> (std::llround (lengthSeconds * sr)) : 0;
    push (c);
}

void InstrumentPlayer::playChord (std::span<const double> frequenciesHz, Instrument instrument, float velocity,
                                  double delaySeconds, double lengthSeconds, double strumSeconds)
{
    const bool strum = instrument == Instrument::guitar;
    for (std::size_t i = 0; i < frequenciesHz.size(); ++i)
    {
        const double offset = strum ? strumSeconds * static_cast<double> (i) : 0.0;
        // Slightly softer upper voices, as when a chord is played by hand.
        const float v = velocity * (1.0f - 0.04f * static_cast<float> (i));
        play (frequenciesHz[i], instrument, v, delaySeconds + offset, lengthSeconds > 0.0 ? juce::jmax (0.05, lengthSeconds - offset) : 0.0);
    }
}

void InstrumentPlayer::stopAll()
{
    Command c;
    c.stop = true;
    push (c);
}

void InstrumentPlayer::prepare (double newSampleRate)
{
    sampleRate.store (newSampleRate);
    pool.prepare (newSampleRate);
}

void InstrumentPlayer::render (float* mono, int numSamples) noexcept
{
    fifo.read (fifo.getNumReady()).forEach ([&] (int index)
    {
        const auto& c = commands[static_cast<std::size_t> (index)];
        if (c.stop)
            pool.stopAll();
        else
            pool.queue (c.event);
    });

    pool.render (mono, numSamples);
}

} // namespace mbox::sound
