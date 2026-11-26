#include "mbox/geometry/TrichordSpace.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace mbox::geometry
{

namespace
{
    constexpr double kSqrt2 = 1.4142135623730951;
    constexpr double kSqrt3 = 1.7320508075688772;
    constexpr double kSqrt6 = 2.4494897427831781;

    double wrap (double v, double period) noexcept
    {
        double r = std::fmod (v, period);
        if (r < 0.0)
            r += period;
        // fmod can return `period` itself after the correction because of rounding
        return r >= period ? 0.0 : r;
    }

    /** Shortest signed path from a to b on a circle of length `period`, in [-period/2, period/2). */
    double shortestStep (double a, double b, double period) noexcept
    {
        double d = wrap (b - a, period);
        if (d >= period * 0.5)
            d -= period;
        return d;
    }

    /** Projects an octave-normalised ordered lift onto the orbifold axes. */
    Vec3 project (const std::array<double, 3>& y) noexcept
    {
        const double mean = (y[0] + y[1] + y[2]) / 3.0;
        // Deviations from the mean, recentred so the augmented triad
        // (deviation (-1/3, 0, 1/3)) lands on the central axis.
        const double q0 = y[0] - mean + 1.0 / 3.0;
        const double q1 = y[1] - mean;
        const double q2 = y[2] - mean - 1.0 / 3.0;

        // Orthonormal basis of the plane orthogonal to (1,1,1).
        return { (q0 - q1) / kSqrt2,
                 (q0 + q1 - 2.0 * q2) / kSqrt6,
                 (y[0] + y[1] + y[2]) / kSqrt3 };
    }

    std::uint64_t latticeKey (const Trichord& c) noexcept
    {
        // Fixed-point key, precise enough for lattices up to thousands of divisions.
        auto q = [] (double v) { return static_cast<std::uint64_t> (std::llround (v * 64.0)) & 0x1FFFFFull; };
        const auto& p = c.pitches();
        return (q (p[0]) << 42) | (q (p[1]) << 21) | q (p[2]);
    }
} // namespace

double Vec3::length() const noexcept
{
    return std::sqrt (x * x + y * y + z * z);
}

// ---------------------------------------------------------------------------

Trichord::Trichord (double a, double b, double c, int divisions) noexcept
    : edo (std::max (1, divisions))
{
    const auto n = static_cast<double> (edo);
    pcs = { wrap (a, n), wrap (b, n), wrap (c, n) };
    std::sort (pcs.begin(), pcs.end());
}

double Trichord::sumClass() const noexcept
{
    return wrap (pcs[0] + pcs[1] + pcs[2], static_cast<double> (edo));
}

std::array<double, 3> Trichord::intervals() const noexcept
{
    return { pcs[1] - pcs[0], pcs[2] - pcs[1], pcs[0] + static_cast<double> (edo) - pcs[2] };
}

Trichord Trichord::transposed (double steps) const noexcept
{
    return { pcs[0] + steps, pcs[1] + steps, pcs[2] + steps, edo };
}

int Trichord::numDistinct (double tolerance) const noexcept
{
    const auto iv = intervals();
    int distinct = 0;
    for (const double i : iv)
        if (i > tolerance)
            ++distinct;
    return std::max (1, distinct);
}

bool Trichord::sameAs (const Trichord& other, double tolerance) const noexcept
{
    if (other.edo != edo)
        return false;
    for (std::size_t i = 0; i < 3; ++i)
        if (std::abs (pcs[i] - other.pcs[i]) > tolerance)
            return false;
    return true;
}

// ---------------------------------------------------------------------------

VoiceLeading minimalVoiceLeading (const Trichord& from, const Trichord& to, VoiceLeadingNorm norm) noexcept
{
    static constexpr std::array<std::array<int, 3>, 6> perms { {
        { 0, 1, 2 }, { 1, 2, 0 }, { 2, 0, 1 }, { 0, 2, 1 }, { 2, 1, 0 }, { 1, 0, 2 }
    } };

    const auto period = static_cast<double> (from.divisions());
    const auto& a = from.pitches();
    const auto& b = to.pitches();

    VoiceLeading best;
    best.size = std::numeric_limits<double>::infinity();

    for (const auto& p : perms)
    {
        VoiceLeading vl;
        vl.targetIndex = p;
        double l1 = 0.0, l2 = 0.0, linf = 0.0;
        for (std::size_t i = 0; i < 3; ++i)
        {
            const double d = shortestStep (a[i], b[static_cast<std::size_t> (p[i])], period);
            vl.displacement[i] = d;
            l1 += std::abs (d);
            l2 += d * d;
            linf = std::max (linf, std::abs (d));
        }

        switch (norm)
        {
            case VoiceLeadingNorm::taxicab:   vl.size = l1; break;
            case VoiceLeadingNorm::euclidean: vl.size = std::sqrt (l2); break;
            case VoiceLeadingNorm::maximum:   vl.size = linf; break;
        }

        if (vl.size < best.size - 1e-12)
            best = vl;
    }
    return best;
}

// ---------------------------------------------------------------------------

double OrbifoldEmbedding::height() noexcept
{
    return 1.0 / kSqrt3;
}

double OrbifoldEmbedding::circumradius() noexcept
{
    return kSqrt2 / 3.0;
}

std::array<Vec3, 3> OrbifoldEmbedding::crossSection (double z) noexcept
{
    // The three triple-unison lifts that bound the domain y1 <= y2 <= y3 <= y1 + 1.
    const std::array<std::array<double, 3>, 3> lifts { {
        { 0.0, 0.0, 0.0 }, { 0.0, 0.0, 1.0 }, { 0.0, 1.0, 1.0 }
    } };

    std::array<Vec3, 3> v;
    for (std::size_t i = 0; i < 3; ++i)
    {
        v[i] = project (lifts[i]);
        v[i].z = z;
    }
    return v;
}

std::array<double, 3> OrbifoldEmbedding::representative (const Trichord& chord) noexcept
{
    const auto n = static_cast<double> (chord.divisions());
    auto y = chord.pitches();

    // Each cyclic "rotation" (drop the top voice an octave, put it at the bottom)
    // lowers the sum by one octave while keeping y1 <= y2 <= y3 <= y1 + n.
    // pitches are in [0, n) so the sum is in [0, 3n): at most two rotations.
    for (int i = 0; i < 2 && y[0] + y[1] + y[2] >= n - 1e-9; ++i)
        y = { y[2] - n, y[0], y[1] };

    return y;
}

Vec3 OrbifoldEmbedding::position (const Trichord& chord) noexcept
{
    const auto n = static_cast<double> (chord.divisions());
    const auto y = representative (chord);
    auto p = project ({ y[0] / n, y[1] / n, y[2] / n });
    p.z = std::clamp (p.z, 0.0, std::nextafter (height(), 0.0)); // absorb rounding at the faces
    return p;
}

// ---------------------------------------------------------------------------

std::vector<Trichord> enumerateTrichords (int divisions, bool includeDoublings)
{
    std::vector<Trichord> out;
    if (divisions < 1)
        return out;

    const int gap = includeDoublings ? 0 : 1;
    for (int a = 0; a < divisions; ++a)
        for (int b = a + gap; b < divisions; ++b)
            for (int c = b + gap; c < divisions; ++c)
                out.emplace_back (a, b, c, divisions);
    return out;
}

std::vector<LatticeEdge> voiceLeadingEdges (const std::vector<Trichord>& chords, double steps)
{
    std::unordered_map<std::uint64_t, std::uint32_t> index;
    index.reserve (chords.size() * 2);
    for (std::uint32_t i = 0; i < chords.size(); ++i)
        index.emplace (latticeKey (chords[i]), i);

    std::vector<LatticeEdge> edges;
    for (std::uint32_t i = 0; i < chords.size(); ++i)
    {
        const auto& p = chords[i].pitches();
        for (std::size_t voice = 0; voice < 3; ++voice)
        {
            for (const double dir : { -steps, steps })
            {
                auto moved = p;
                moved[voice] += dir;
                const Trichord neighbour (moved[0], moved[1], moved[2], chords[i].divisions());
                const auto it = index.find (latticeKey (neighbour));
                if (it != index.end() && it->second > i)
                    edges.push_back ({ i, it->second });
            }
        }
    }

    // Moving different voices can reach the same neighbour (e.g. from a doubled note).
    std::sort (edges.begin(), edges.end(), [] (LatticeEdge x, LatticeEdge y) { return x.a != y.a ? x.a < y.a : x.b < y.b; });
    edges.erase (std::unique (edges.begin(), edges.end(), [] (LatticeEdge x, LatticeEdge y) { return x.a == y.a && x.b == y.b; }),
                 edges.end());
    return edges;
}

} // namespace mbox::geometry
