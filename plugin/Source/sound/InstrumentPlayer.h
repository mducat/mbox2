#pragma once

#include <mbox/sound/Instruments.h>

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>
#include <span>

namespace mbox::sound
{

/**
    Piano / guitar voices played from the UI (click on a key, a fret, a chord
    of the tonal map, "Play" of the composition). Rendered by the
    SpectrumEngine next to the additive synth, so the spectrogram sees them too.

    UI thread (single producer): play() / playChord() / stopAll().
    Audio thread: prepare() / render().
    Events go through a lock-free FIFO and are scheduled sample-accurately by
    the core VoicePool, so strums and progressions keep their timing.
*/
class InstrumentPlayer
{
public:
    InstrumentPlayer() = default;

    // -- UI thread -------------------------------------------------------------

    /** One note. delay / length in seconds; length <= 0 lets the note ring out. */
    void play (double frequencyHz, Instrument instrument, float velocity = 0.8f, double delaySeconds = 0.0, double lengthSeconds = 0.0);

    /**
        Several notes at once (frequencies low to high). A guitar chord is
        strummed (strumSeconds between strings), a piano chord is struck together.
    */
    void playChord (std::span<const double> frequenciesHz, Instrument instrument, float velocity = 0.75f,
                    double delaySeconds = 0.0, double lengthSeconds = 0.0, double strumSeconds = 0.018);

    /** Cancels scheduled notes and damps everything that sounds. */
    void stopAll();

    /** When false, play() and playChord() do nothing. */
    std::atomic<bool> enabled { true };

    // -- Audio thread ----------------------------------------------------------

    void prepare (double sampleRate);

    /** Adds the voices to `mono` (numSamples samples). */
    void render (float* mono, int numSamples) noexcept;

private:
    struct Command
    {
        NoteEvent event;
        bool stop = false;
    };

    void push (const Command&);

    std::atomic<double> sampleRate { 44100.0 };
    juce::AbstractFifo fifo { 1024 };
    std::array<Command, 1024> commands {};
    VoicePool pool;
};

} // namespace mbox::sound
