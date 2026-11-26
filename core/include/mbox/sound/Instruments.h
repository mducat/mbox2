#pragma once

#include <array>
#include <cstdint>
#include <vector>

/**
    Sample-free instrument models (no JUCE dependency, real-time safe once
    constructed: nothing allocates after construction / prepare()).

    Every note is given as a frequency in Hz, never as a note number, so the
    models play any tuning exactly (12-TET, N-TET, just intonation...).

      - PluckedString: extended Karplus-Strong string (guitar). Fractional
        delay tuning, pick position, frequency dependent damping and a light
        body resonance.
      - PianoTone: modal piano string. Inharmonic (stiff string) partials,
        hammer position / hardness spectrum, two slightly detuned unison
        strings per partial (beating + two-stage decay), hammer thump.
      - VoicePool: polyphonic player with a time-stamped event list, so a
        strum or a whole chord progression can be scheduled sample-accurately.
*/
namespace mbox::sound
{

enum class Instrument : std::uint8_t
{
    piano  = 0,
    guitar = 1
};

/** Extended Karplus-Strong plucked string. */
class PluckedString
{
public:
    /** Longest period handled: 2^13 samples (~ 23 Hz at 192 kHz, ~ 6 Hz at 48 kHz). */
    static constexpr int kMaxDelay = 1 << 13;

    PluckedString();

    /**
        Starts a note. velocity in 0..1 (louder and brighter), noiseSeed makes
        two plucks of the same note slightly different.
    */
    void start (double frequencyHz, double sampleRate, float velocity, std::uint32_t noiseSeed);

    /** Damps the string (fingers released): it dies out in ~ 0.15 s. */
    void release() noexcept;

    float next() noexcept;
    bool isActive() const noexcept { return active; }

    /** Actual loop period in samples, for tests (the tuning is exact to < 1 cent). */
    double periodSamples() const noexcept { return period; }

private:
    void setDecay (double t60Seconds) noexcept;

    std::vector<float> line;
    int writePos = 0, delayInt = 2;
    double period = 2.0, frequency = 110.0, sampleRate = 48000.0;
    float loopGain = 0.99f, damping = 0.5f; // one-zero loop filter: (1 - d) x[n] + d x[n-1]
    float apCoef = 0.0f, apIn = 0.0f, apOut = 0.0f, filterPrev = 0.0f;
    float dcIn = 0.0f, dcOut = 0.0f;

    // Two resonators for the guitar body (biquad bandpass, direct form I state).
    struct Resonator
    {
        float b0 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0, gain = 0;
        void set (double f, double q, double sr, float g);
        float process (float x) noexcept;
    };
    std::array<Resonator, 2> body;

    float level = 1.0f;
    int silentSamples = 0;
    bool active = false;
};

/** Modal (additive, inharmonic) piano tone. */
class PianoTone
{
public:
    static constexpr int kMaxPartials = 28;

    void start (double frequencyHz, double sampleRate, float velocity, std::uint32_t noiseSeed);

    /** Damper falls on the string: it dies out in ~ 0.12 s (bass notes ring a little longer). */
    void release() noexcept;

    float next() noexcept;
    bool isActive() const noexcept { return active; }

    /** Frequency of partial n (1-based) for a fundamental f0 and inharmonicity B (partial 1 = f0). */
    static double partialFrequency (double f0, int n, double inharmonicity) noexcept;

    /** Inharmonicity coefficient used for a fundamental (larger in the treble). */
    static double inharmonicityFor (double f0) noexcept;

private:
    // Damped resonator y[n] = c1 y[n-1] - c2 y[n-2], two per partial (unison strings).
    struct Mode
    {
        float c1 = 0, c2 = 0, y1 = 0, y2 = 0;
    };

    std::array<Mode, 2 * kMaxPartials> modes {};
    int numModes = 0;

    float releaseGain = 1.0f, releaseFactor = 1.0f;
    bool released = false;

    // Hammer thump: decaying low-passed noise.
    std::uint32_t noise = 1;
    float thumpLevel = 0.0f, thumpDecay = 0.0f, thumpState = 0.0f, thumpCoef = 0.0f;

    float attack = 0.0f, attackStep = 1.0f;
    int silentSamples = 0;
    bool active = false;
};

/** One note to play. */
struct NoteEvent
{
    double frequencyHz = 440.0;
    float velocity = 0.8f;
    Instrument instrument = Instrument::piano;
    std::int64_t startSample = 0;  // delay from "now" (when queued) in samples
    std::int64_t lengthSamples = 0; // time until release; <= 0 = let it ring out
};

/**
    Polyphonic player. queue() adds events (they start after their delay),
    render() mixes the sounding voices into a mono buffer. Single thread: the
    owner hands events over from the UI thread (e.g. through a FIFO).
*/
class VoicePool
{
public:
    static constexpr int kNumVoices   = 32;
    static constexpr int kMaxPending  = 512;

    VoicePool();

    void prepare (double sampleRate);

    /** Schedules a note; returns false (and drops it) when the pending list is full. */
    bool queue (const NoteEvent& event) noexcept;

    /** Cancels every pending event and quickly damps every sounding voice. */
    void stopAll() noexcept;

    /** Adds numSamples of output to `out` (mono). */
    void render (float* out, int numSamples) noexcept;

    int numActiveVoices() const noexcept;
    int numPending() const noexcept { return numPendingEvents; }

    float gain = 0.35f; // master level of the mix

private:
    struct Voice
    {
        Instrument instrument = Instrument::piano;
        PluckedString string;
        PianoTone piano;
        std::int64_t samplesToRelease = -1; // -1 = no scheduled release
        std::uint64_t startedAt = 0;
        bool isActive() const noexcept { return instrument == Instrument::guitar ? string.isActive() : piano.isActive(); }
        void release() noexcept { instrument == Instrument::guitar ? string.release() : piano.release(); }
        float next() noexcept { return instrument == Instrument::guitar ? string.next() : piano.next(); }
    };

    struct Pending
    {
        NoteEvent event;
        std::uint64_t due = 0; // absolute sample time
    };

    void startVoice (const NoteEvent& e) noexcept;

    std::array<Voice, kNumVoices> voices;
    std::array<Pending, kMaxPending> pending {};
    int numPendingEvents = 0;
    double sampleRate = 48000.0;
    std::uint64_t now = 0;
    std::uint32_t seed = 0x9e3779b9u;
};

} // namespace mbox::sound
