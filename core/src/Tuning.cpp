#include "mbox/theory/Tuning.h"

#include <algorithm>
#include <cmath>

namespace mbox::theory
{

namespace
{
    // In every tuning, A4 sits 900 cents above middle C (standard pitch convention).
    constexpr double kMiddleCToA4Cents = 900.0;

    int floorDiv (int a, int b) noexcept
    {
        return a / b - ((a % b != 0) && ((a < 0) != (b < 0)) ? 1 : 0);
    }
} // namespace

Tuning::Tuning() : Tuning (equal (kDefaultDivisions)) {}

Tuning::Tuning (std::string name, std::vector<double> stepCents, double periodCents, bool isEqualTuning)
    : tuningName (std::move (name)),
      steps (std::move (stepCents)),
      period (periodCents > 0.0 ? periodCents : 1200.0),
      equalSteps (isEqualTuning)
{
    std::sort (steps.begin(), steps.end());
    if (steps.empty() || steps.front() != 0.0)
        steps.insert (steps.begin(), 0.0);
    if (static_cast<int> (steps.size()) > kMaxDivisions)
        steps.resize (static_cast<std::size_t> (kMaxDivisions));
}

Tuning Tuning::equal (int divisions, double periodCents)
{
    divisions = clampDivisions (divisions);
    std::vector<double> cents (static_cast<std::size_t> (divisions));
    for (int i = 0; i < divisions; ++i)
        cents[static_cast<std::size_t> (i)] = periodCents * i / divisions;

    return Tuning (std::to_string (divisions) + "-TET", std::move (cents), periodCents, true);
}

Tuning Tuning::fromCents (std::string name, std::vector<double> stepCents, double periodCents)
{
    return Tuning (std::move (name), std::move (stepCents), periodCents, false);
}

bool operator== (const Tuning& a, const Tuning& b) noexcept
{
    return a.steps == b.steps && a.period == b.period && a.aHz == b.aHz;
}

bool Tuning::isTwelveTet() const noexcept
{
    return equalSteps && divisions() == kDefaultDivisions && period == 1200.0;
}

Tuning Tuning::withConcertA (double hz) const
{
    auto t = *this;
    t.aHz = hz > 0.0 ? hz : 440.0;
    return t;
}

int Tuning::octaveOf (int note) const noexcept
{
    return floorDiv (note, divisions()) - 1;
}

PitchClass Tuning::pitchClassOf (int note) const noexcept
{
    return PitchClass::fromNote (note, divisions());
}

int Tuning::noteOf (PitchClass pc, int octave) const noexcept
{
    return (octave + 1) * divisions() + pc.convertedTo (divisions()).value();
}

double Tuning::stepCents (int step) const noexcept
{
    return steps[static_cast<std::size_t> (modN (step, divisions()))];
}

double Tuning::centsFromMiddleC (int note) const noexcept
{
    const int rel = note - middleC();
    return floorDiv (rel, divisions()) * period + stepCents (rel);
}

double Tuning::frequencyOf (int note) const noexcept
{
    return aHz * std::pow (2.0, (centsFromMiddleC (note) - kMiddleCToA4Cents) / 1200.0);
}

int Tuning::nearestNote (double hz, double* centsOffset) const noexcept
{
    if (hz <= 0.0)
        return middleC();

    const double cents = 1200.0 * std::log2 (hz / aHz) + kMiddleCToA4Cents; // from middle C
    const int periods = static_cast<int> (std::floor (cents / period));
    const double within = cents - periods * period;

    int bestStep = 0;
    double bestDiff = within; // distance to step 0 of this period
    for (int i = 1; i <= divisions(); ++i)
    {
        const double c = (i == divisions()) ? period : steps[static_cast<std::size_t> (i)];
        if (std::abs (within - c) < std::abs (bestDiff))
        {
            bestDiff = within - c;
            bestStep = i;
        }
    }

    if (centsOffset != nullptr)
        *centsOffset = bestDiff;
    return middleC() + periods * divisions() + bestStep;
}

std::string Tuning::noteName (int note, bool preferFlats) const
{
    std::string pcName;
    if (isEqual() && period == 1200.0)
        pcName = pitchClassOf (note).name (preferFlats);
    else
        pcName = "#" + std::to_string (modN (note, divisions())); // no meaningful letter names: step index

    // "C4", "F#3", but "D-37 4" so the octave is not read as part of the offset.
    const bool needsSpace = pcName.find_first_of ("+-") != std::string::npos || pcName.front() == '#';
    return pcName + (needsSpace ? " " : "") + std::to_string (octaveOf (note));
}

double Tuning::intervalCents (int stepCount) const noexcept
{
    return floorDiv (stepCount, divisions()) * period + stepCents (stepCount);
}

int Tuning::stepsFromCents (double cents) const noexcept
{
    if (equalSteps)
        return static_cast<int> (std::ceil (cents * divisions() / period - 0.5));

    const double hzRatio = std::pow (2.0, cents / 1200.0);
    return nearestNote (aHz * std::pow (2.0, -kMiddleCToA4Cents / 1200.0) * hzRatio) - middleC();
}

int Tuning::stepsFromRatio (double ratio) const noexcept
{
    return ratio > 0.0 ? stepsFromCents (1200.0 * std::log2 (ratio)) : 0;
}

int Tuning::convertNote (int note, const Tuning& from) const noexcept
{
    if (from == *this)
        return note;
    return nearestNote (from.frequencyOf (note));
}

namespace tunings
{
    std::vector<Tuning> presets()
    {
        std::vector<Tuning> list;
        for (const int n : { 12, 17, 19, 22, 24, 31, 41, 53 })
            list.push_back (Tuning::equal (n));
        return list;
    }
} // namespace tunings

} // namespace mbox::theory
