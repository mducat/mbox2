#include "AdditiveSynth.h"

#include <mbox/analysis/Harmonics.h>

#include <cmath>

namespace mbox::spectrum
{

namespace
{
    struct AnySound final : juce::SynthesiserSound
    {
        bool appliesToNote (int) override { return true; }
        bool appliesToChannel (int) override { return true; }
    };

    class AdditiveVoice final : public juce::SynthesiserVoice
    {
    public:
        AdditiveVoice (const AdditiveSynthSettings& s, int sustainMidiChannel) : settings (s), sustainChannel (sustainMidiChannel)
        {
            envelope.setParameters ({ 0.01f, 0.1f, 0.8f, 0.25f });
        }

        using SynthesiserVoice::renderNextBlock;

        bool canPlaySound (juce::SynthesiserSound*) override { return true; }

        void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
        {
            const auto sr = getSampleRate();
            const auto f0 = isPlayingChannel (sustainChannel)
                ? static_cast<double> (settings.playingSustained[static_cast<std::size_t> (juce::jlimit (0, AdditiveSynthSettings::kMaxSustained - 1, midiNote))])
                : static_cast<double> (settings.noteFrequency[static_cast<std::size_t> (juce::jlimit (0, 127, midiNote))].load());
            const auto wanted   = juce::jlimit (1, AdditiveSynth::kMaxPartials, settings.numPartials.load());
            const auto rolloff  = static_cast<double> (settings.rolloff.load());

            numPartials = 0;
            double norm = 0.0;
            for (int h = 1; h <= wanted && f0 * h < 0.45 * sr; ++h)
            {
                auto& p   = partials[static_cast<std::size_t> (numPartials++)];
                const auto w = juce::MathConstants<double>::twoPi * f0 * h / sr;
                p.cosStep = std::cos (w);
                p.sinStep = std::sin (w);
                p.re = 1.0;
                p.im = 0.0;
                p.amplitude = mbox::analysis::harmonicAmplitude (h, rolloff);
                norm += p.amplitude;
            }

            gain = norm > 0.0 ? 0.25 * velocity / norm : 0.0;
            envelope.setSampleRate (sr);
            envelope.noteOn();
        }

        void stopNote (float, bool allowTailOff) override
        {
            if (allowTailOff)
            {
                envelope.noteOff();
            }
            else
            {
                envelope.reset();
                clearCurrentNote();
            }
        }

        void pitchWheelMoved (int) override {}
        void controllerMoved (int, int) override {}

        void renderNextBlock (juce::AudioBuffer<float>& out, int start, int num) override
        {
            if (! isVoiceActive())
                return;

            for (int i = 0; i < num; ++i)
            {
                double sample = 0.0;
                for (int k = 0; k < numPartials; ++k)
                {
                    auto& p = partials[static_cast<std::size_t> (k)];
                    sample += p.amplitude * p.im;
                    const auto re = p.re * p.cosStep - p.im * p.sinStep;
                    p.im          = p.re * p.sinStep + p.im * p.cosStep;
                    p.re          = re;
                }

                const auto value = static_cast<float> (sample * gain) * envelope.getNextSample();
                for (int ch = 0; ch < out.getNumChannels(); ++ch)
                    out.addSample (ch, start + i, value);

                if (! envelope.isActive())
                {
                    clearCurrentNote();
                    break;
                }
            }

            // Keep the rotating phasors on the unit circle (rounding drift).
            for (int k = 0; k < numPartials; ++k)
            {
                auto& p        = partials[static_cast<std::size_t> (k)];
                const auto mag = std::sqrt (p.re * p.re + p.im * p.im);
                if (mag > 0.0)
                {
                    p.re /= mag;
                    p.im /= mag;
                }
            }
        }

    private:
        struct Phasor
        {
            double re = 1.0, im = 0.0, cosStep = 1.0, sinStep = 0.0, amplitude = 0.0;
        };

        const AdditiveSynthSettings& settings;
        const int sustainChannel;
        std::array<Phasor, AdditiveSynth::kMaxPartials> partials {};
        int numPartials = 0;
        double gain = 0.0;
        juce::ADSR envelope;
    };
} // namespace

AdditiveSynthSettings::AdditiveSynthSettings()
{
    for (int n = 0; n < 128; ++n)
        noteFrequency[static_cast<std::size_t> (n)].store (static_cast<float> (mbox::analysis::midiToFrequency (n)));
}

AdditiveSynth::AdditiveSynth()
{
    for (int i = 0; i < kNumVoices; ++i)
        synth.addVoice (new AdditiveVoice (sharedSettings, kSustainChannel));
    synth.addSound (new AnySound());
}

void AdditiveSynth::prepare (double sampleRate)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    synth.allNotesOff (0, false);
    sharedSettings.playingSustained.fill (0.0f);
    lastSustainedGeneration = sharedSettings.sustainedGeneration.load() - 1; // re-trigger sustained notes
    mergedMidi.ensureSize (2048);
}

void AdditiveSynth::addSustainedNoteEvents (juce::MidiBuffer& events, int samplePosition)
{
    const auto generation = sharedSettings.sustainedGeneration.load (std::memory_order_acquire);
    if (generation == lastSustainedGeneration)
        return;
    lastSustainedGeneration = generation;

    for (int slot = 0; slot < AdditiveSynthSettings::kMaxSustained; ++slot)
    {
        const auto i      = static_cast<std::size_t> (slot);
        const auto wanted = sharedSettings.sustainedFrequency[i].load (std::memory_order_relaxed);
        auto& playing     = sharedSettings.playingSustained[i];

        if (juce::exactlyEqual (wanted, playing))
            continue;

        if (playing > 0.0f)
            events.addEvent (juce::MidiMessage::noteOff (kSustainChannel, slot), samplePosition);

        playing = wanted; // read by the voice in startNote()
        if (wanted > 0.0f)
            events.addEvent (juce::MidiMessage::noteOn (kSustainChannel, slot, 0.8f), samplePosition);
    }
}

void AdditiveSynth::render (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, int startSample, int numSamples)
{
    // Sustained notes are merged at the start of the block. Incoming MIDI on
    // channel 16 is dropped: that channel is reserved for them.
    mergedMidi.clear();
    addSustainedNoteEvents (mergedMidi, startSample);
    for (const auto metadata : midi)
        if (metadata.getMessage().getChannel() != kSustainChannel)
            mergedMidi.addEvent (metadata.getMessage(), metadata.samplePosition);

    synth.renderNextBlock (buffer, mergedMidi, startSample, numSamples);
}

void AdditiveSynth::setSustainedFrequencies (std::span<const double> hz) noexcept
{
    for (std::size_t i = 0; i < sharedSettings.sustainedFrequency.size(); ++i)
        sharedSettings.sustainedFrequency[i].store (i < hz.size() ? static_cast<float> (hz[i]) : 0.0f, std::memory_order_relaxed);
    sharedSettings.sustainedGeneration.fetch_add (1, std::memory_order_release);
}

void AdditiveSynth::setNoteFrequencies (std::span<const double> hz) noexcept
{
    const auto n = juce::jmin (hz.size(), sharedSettings.noteFrequency.size());
    for (std::size_t i = 0; i < n; ++i)
        sharedSettings.noteFrequency[i].store (static_cast<float> (hz[i]));
}

} // namespace mbox::spectrum
