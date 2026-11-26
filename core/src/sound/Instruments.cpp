#include "mbox/sound/Instruments.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace mbox::sound
{

namespace
{
    constexpr double kTwoPi = 2.0 * std::numbers::pi;
    constexpr float kSilence = 1.0e-4f;

    std::uint32_t nextRandom (std::uint32_t& state) noexcept
    {
        state = state * 1664525u + 1013904223u;
        return state;
    }

    /** Uniform noise in [-1, 1). */
    float noiseSample (std::uint32_t& state) noexcept
    {
        return static_cast<float> (nextRandom (state) >> 8) * (2.0f / 16777216.0f) - 1.0f;
    }

    /** Per-sample gain making a signal fall by 60 dB in t60 seconds. */
    double decayPerSample (double t60, double sampleRate) noexcept
    {
        return std::pow (10.0, -3.0 / (std::max (t60, 1.0e-3) * sampleRate));
    }
} // namespace

//==============================================================================
// PluckedString
//==============================================================================

void PluckedString::Resonator::set (double f, double q, double sr, float g)
{
    // RBJ bandpass (constant 0 dB peak gain), normalised by a0.
    const double w = kTwoPi * f / sr;
    const double alpha = std::sin (w) / (2.0 * q);
    const double a0 = 1.0 + alpha;
    b0 = static_cast<float> (alpha / a0);
    a1 = static_cast<float> (-2.0 * std::cos (w) / a0);
    a2 = static_cast<float> ((1.0 - alpha) / a0);
    x1 = x2 = y1 = y2 = 0.0f;
    gain = g;
}

float PluckedString::Resonator::process (float x) noexcept
{
    const float y = b0 * x - b0 * x2 - a1 * y1 - a2 * y2;
    x2 = x1;
    x1 = x;
    y2 = y1;
    y1 = y;
    return gain * y;
}

PluckedString::PluckedString() : line (static_cast<std::size_t> (kMaxDelay), 0.0f) {}

void PluckedString::setDecay (double t60Seconds) noexcept
{
    // Loss per period wanted at the fundamental, minus what the loop filter already removes there.
    const double perPeriod = std::pow (10.0, -3.0 / (frequency * t60Seconds));
    const double w = kTwoPi * frequency / sampleRate;
    const double d = damping;
    const double filterGain = std::sqrt ((1.0 - d) * (1.0 - d) + d * d + 2.0 * d * (1.0 - d) * std::cos (w));
    loopGain = static_cast<float> (std::min (0.99995, perPeriod / std::max (filterGain, 1.0e-3)));
}

void PluckedString::start (double frequencyHz, double sr, float velocity, std::uint32_t noiseSeed)
{
    sampleRate = sr;
    frequency = std::clamp (frequencyHz, sr / (kMaxDelay - 2), sr / 4.0);
    velocity = std::clamp (velocity, 0.05f, 1.0f);

    // Loop delay = integer line + one-zero filter (d samples) + allpass (fractional part).
    damping = 0.42f - 0.12f * velocity; // harder pluck = brighter string
    period = sampleRate / frequency;
    delayInt = std::clamp (static_cast<int> (std::floor (period - damping - 0.1)), 1, kMaxDelay - 1);
    const double frac = period - damping - delayInt; // in [0.1, 1.1)
    apCoef = static_cast<float> ((1.0 - frac) / (1.0 + frac));
    apIn = apOut = filterPrev = 0.0f;
    dcIn = dcOut = 0.0f;

    // Ring time of the fundamental: long in the bass, short in the treble.
    setDecay (std::clamp (7.0 * std::sqrt (82.4 / frequency), 1.2, 8.0));

    // Excitation: low-passed noise (brighter when louder), pick position comb, no DC.
    std::uint32_t rng = noiseSeed | 1u;
    const float lp = 0.25f + 0.6f * velocity;
    float state = 0.0f;
    for (int i = 0; i < delayInt; ++i)
    {
        state += lp * (noiseSample (rng) - state);
        line[static_cast<std::size_t> (i)] = state;
    }

    const int pick = std::max (1, static_cast<int> (std::lround (0.13 * delayInt)));
    for (int i = delayInt - 1; i >= pick; --i)
        line[static_cast<std::size_t> (i)] -= line[static_cast<std::size_t> (i - pick)];

    float mean = 0.0f, peak = 0.0f;
    for (int i = 0; i < delayInt; ++i)
        mean += line[static_cast<std::size_t> (i)];
    mean /= static_cast<float> (delayInt);
    for (int i = 0; i < delayInt; ++i)
        peak = std::max (peak, std::abs (line[static_cast<std::size_t> (i)] -= mean));
    const float norm = peak > 0.0f ? 1.0f / peak : 0.0f;
    for (int i = 0; i < delayInt; ++i)
        line[static_cast<std::size_t> (i)] *= norm;

    body[0].set (98.0, 6.0, sampleRate, 0.35f);  // air cavity
    body[1].set (205.0, 5.0, sampleRate, 0.25f); // top plate

    level = 0.9f * velocity;
    writePos = 0;
    silentSamples = 0;
    active = true;
}

void PluckedString::release() noexcept
{
    if (! active)
        return;
    damping = 0.6f;
    setDecay (0.15);
}

float PluckedString::next() noexcept
{
    if (! active)
        return 0.0f;

    auto& slot = line[static_cast<std::size_t> (writePos)];
    const float out = slot;

    const float filtered = (1.0f - damping) * out + damping * filterPrev;
    filterPrev = out;
    const float ap = apCoef * filtered + apIn - apCoef * apOut;
    apIn = filtered;
    apOut = ap;
    slot = ap * loopGain;
    if (++writePos >= delayInt)
        writePos = 0;

    float y = out + body[0].process (out) + body[1].process (out);

    // DC blocker (the excitation is zero-mean, the loop filter can still leak a little).
    const float hp = y - dcIn + 0.995f * dcOut;
    dcIn = y;
    dcOut = hp;
    y = hp * level;

    if (std::abs (out) < kSilence)
    {
        if (++silentSamples > 4096)
            active = false;
    }
    else
    {
        silentSamples = 0;
    }
    return y;
}

//==============================================================================
// PianoTone
//==============================================================================

double PianoTone::partialFrequency (double f0, int n, double inharmonicity) noexcept
{
    // Stiff string partials, scaled so the fundamental stays exactly at f0 (exact N-TET tuning).
    const double nn = static_cast<double> (n);
    return nn * f0 * std::sqrt ((1.0 + inharmonicity * nn * nn) / (1.0 + inharmonicity));
}

double PianoTone::inharmonicityFor (double f0) noexcept
{
    return std::clamp (3.0e-4 * std::pow (f0 / 261.63, 1.5), 5.0e-5, 0.02);
}

void PianoTone::start (double frequencyHz, double sr, float velocity, std::uint32_t noiseSeed)
{
    const double f0 = std::clamp (frequencyHz, 20.0, sr * 0.45);
    velocity = std::clamp (velocity, 0.05f, 1.0f);
    noise = noiseSeed | 1u;

    const double B = inharmonicityFor (f0);
    const double maxFreq = std::min (0.45 * sr, 12000.0);
    const double hammerPos = 1.0 / 7.3;                 // striking point along the string
    const double hardness = 1.9 - 0.9 * velocity;       // spectral slope: softer = duller
    const double cutoff = 1200.0 + 4000.0 * velocity;   // felt low-pass
    const double t60Fundamental = std::clamp (14.0 * std::pow (110.0 / f0, 0.7), 1.0, 25.0);

    // Unison detuning (a fraction of a cent) varies a little from note to note.
    const double detuneCents = 0.35 + 0.5 * static_cast<double> (nextRandom (noise) >> 16) / 65536.0;
    const double detune = std::exp2 (detuneCents / 1200.0);

    numModes = 0;
    double total = 0.0;
    std::array<double, kMaxPartials> amps {};
    std::array<double, kMaxPartials> freqs {};
    int numPartials = 0;
    for (int n = 1; n <= kMaxPartials; ++n)
    {
        const double fn = partialFrequency (f0, n, B);
        if (fn * detune >= maxFreq)
            break;
        const double shape = std::abs (std::sin (std::numbers::pi * n * hammerPos)) + 0.05;
        const double a = shape / std::pow (static_cast<double> (n), hardness) / (1.0 + (fn / cutoff) * (fn / cutoff));
        amps[static_cast<std::size_t> (numPartials)] = a;
        freqs[static_cast<std::size_t> (numPartials)] = fn;
        total += a;
        ++numPartials;
    }

    const double scale = total > 0.0 ? 1.0 * std::pow (velocity, 1.4) / total : 0.0;

    auto addMode = [&] (double f, double amplitude, double t60)
    {
        auto& m = modes[static_cast<std::size_t> (numModes++)];
        const double w = kTwoPi * f / sr;
        const double r = decayPerSample (t60, sr);
        m.c1 = static_cast<float> (2.0 * r * std::cos (w));
        m.c2 = static_cast<float> (r * r);
        // y[n] = A r^n sin (n w): seed y[-1] and y[-2] so the first output is 0 (no click).
        m.y1 = static_cast<float> (amplitude * std::sin (-w) / r);
        m.y2 = static_cast<float> (amplitude * std::sin (-2.0 * w) / (r * r));
    };

    for (int i = 0; i < numPartials; ++i)
    {
        const auto k = static_cast<std::size_t> (i);
        const double t60 = t60Fundamental / (1.0 + (freqs[k] - f0) / 900.0);
        const double a = amps[k] * scale;
        addMode (freqs[k], 0.72 * a, t60);                  // prompt sound
        addMode (freqs[k] * detune, 0.28 * a, 2.6 * t60);   // aftersound (second string, slower)
    }

    thumpLevel = 0.12f * velocity * velocity;
    thumpDecay = static_cast<float> (decayPerSample (0.04, sr));
    thumpCoef = static_cast<float> (1.0 - std::exp (-kTwoPi * std::min (900.0, 3.0 * f0 + 200.0) / sr));
    thumpState = 0.0f;

    attack = 0.0f;
    attackStep = static_cast<float> (1.0 / (0.0015 * sr));
    releaseGain = 1.0f;
    released = false;
    releaseFactor = 1.0f;

    // Damper time, set now so release() does not need the sample rate.
    releaseFactor = static_cast<float> (decayPerSample (f0 < 130.0 ? 0.3 : 0.12, sr));

    silentSamples = 0;
    active = numModes > 0;
}

void PianoTone::release() noexcept
{
    released = true;
}

float PianoTone::next() noexcept
{
    if (! active)
        return 0.0f;

    float sum = 0.0f;
    for (int i = 0; i < numModes; ++i)
    {
        auto& m = modes[static_cast<std::size_t> (i)];
        const float y = m.c1 * m.y1 - m.c2 * m.y2;
        m.y2 = m.y1;
        m.y1 = y;
        sum += y;
    }

    thumpState += thumpCoef * (noiseSample (noise) * thumpLevel - thumpState);
    thumpLevel *= thumpDecay;

    if (attack < 1.0f)
        attack = std::min (1.0f, attack + attackStep);
    if (released)
        releaseGain *= releaseFactor;

    const float out = (sum * attack + thumpState) * releaseGain;

    if (std::abs (out) < kSilence)
    {
        if (++silentSamples > 4096 || releaseGain < 1.0e-4f)
            active = false;
    }
    else
    {
        silentSamples = 0;
    }
    return out;
}

//==============================================================================
// VoicePool
//==============================================================================

VoicePool::VoicePool() = default;

void VoicePool::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    numPendingEvents = 0;
    for (auto& v : voices)
        v = {};
}

bool VoicePool::queue (const NoteEvent& event) noexcept
{
    if (numPendingEvents >= kMaxPending || ! (event.frequencyHz > 0.0))
        return false;
    pending[static_cast<std::size_t> (numPendingEvents++)] = { event, now + static_cast<std::uint64_t> (std::max<std::int64_t> (0, event.startSample)) };
    return true;
}

void VoicePool::stopAll() noexcept
{
    numPendingEvents = 0;
    for (auto& v : voices)
        if (v.isActive())
            v.release();
}

int VoicePool::numActiveVoices() const noexcept
{
    return static_cast<int> (std::count_if (voices.begin(), voices.end(), [] (const Voice& v) { return v.isActive(); }));
}

void VoicePool::startVoice (const NoteEvent& e) noexcept
{
    Voice* chosen = nullptr;
    for (auto& v : voices)
        if (! v.isActive())
        {
            chosen = &v;
            break;
        }

    if (chosen == nullptr) // steal the oldest note
        chosen = &*std::min_element (voices.begin(), voices.end(),
                                     [] (const Voice& a, const Voice& b) { return a.startedAt < b.startedAt; });

    chosen->instrument = e.instrument;
    if (e.instrument == Instrument::guitar)
        chosen->string.start (e.frequencyHz, sampleRate, e.velocity, nextRandom (seed));
    else
        chosen->piano.start (e.frequencyHz, sampleRate, e.velocity, nextRandom (seed));
    chosen->samplesToRelease = e.lengthSamples > 0 ? e.lengthSamples : -1;
    chosen->startedAt = now;
}

void VoicePool::render (float* out, int numSamples) noexcept
{
    int pos = 0;
    while (pos < numSamples)
    {
        // Start every event due now, then render up to the next due event (or the block end).
        std::uint64_t nextDue = now + static_cast<std::uint64_t> (numSamples - pos);
        for (int i = 0; i < numPendingEvents;)
        {
            auto& p = pending[static_cast<std::size_t> (i)];
            if (p.due <= now)
            {
                startVoice (p.event);
                p = pending[static_cast<std::size_t> (--numPendingEvents)];
                continue;
            }
            nextDue = std::min (nextDue, p.due);
            ++i;
        }

        const int count = static_cast<int> (nextDue - now);
        for (auto& v : voices)
        {
            if (! v.isActive())
                continue;
            for (int j = 0; j < count; ++j)
            {
                if (v.samplesToRelease > 0 && --v.samplesToRelease == 0)
                    v.release();
                out[pos + j] += gain * v.next();
            }
        }

        pos += count;
        now += static_cast<std::uint64_t> (count);
    }
}

} // namespace mbox::sound
