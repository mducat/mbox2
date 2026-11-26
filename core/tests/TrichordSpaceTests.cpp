#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "mbox/geometry/TrichordSpace.h"

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace mbox::geometry;
using Catch::Approx;

namespace
{
    double radius (Vec3 p) { return std::hypot (p.x, p.y); }
    double angle (Vec3 p) { return std::atan2 (p.y, p.x); }

    /** Signed distance from p to the line through a, b (2D, horizontal plane). */
    double distanceToEdge (Vec3 p, Vec3 a, Vec3 b)
    {
        const double ex = b.x - a.x, ey = b.y - a.y;
        return ((p.x - a.x) * ey - (p.y - a.y) * ex) / std::hypot (ex, ey);
    }

    std::size_t indexOf (const std::vector<Trichord>& chords, const Trichord& c)
    {
        const auto it = std::find_if (chords.begin(), chords.end(), [&] (const Trichord& x) { return x.sameAs (c); });
        return static_cast<std::size_t> (it - chords.begin());
    }
} // namespace

TEST_CASE ("Trichord normalises pitch classes")
{
    const Trichord c (19, -5, 4); // G, G, E -> E G G
    CHECK (c.pitches() == std::array<double, 3> { 4, 7, 7 });
    CHECK (c.numDistinct() == 2);
    CHECK (Trichord (0, 4, 7).intervals() == std::array<double, 3> { 4, 3, 5 });
    CHECK (Trichord (0, 0, 0).numDistinct() == 1);
    CHECK (Trichord (0, 4, 7).transposed (2).sameAs (Trichord (2, 6, 9)));
    CHECK (Trichord (0, 11, 18, 31).sumClass() == Approx (29.0));
}

TEST_CASE ("Enumerating N-TET trichords")
{
    CHECK (enumerateTrichords (12).size() == 364);        // C(14, 3)
    CHECK (enumerateTrichords (12, false).size() == 220); // C(12, 3)
    CHECK (enumerateTrichords (19).size() == 1330);       // C(21, 3)
    CHECK (enumerateTrichords (31, false).size() == 4495);
}

TEST_CASE ("Minimal voice leadings")
{
    const Trichord cMajor (0, 4, 7);
    CHECK (voiceLeadingDistance (cMajor, Trichord (9, 0, 4)) == Approx (2)); // Am: G -> A
    CHECK (voiceLeadingDistance (cMajor, Trichord (4, 7, 11)) == Approx (1)); // Em: C -> B
    CHECK (voiceLeadingDistance (cMajor, Trichord (0, 3, 7)) == Approx (1));  // Cm: E -> Eb
    CHECK (voiceLeadingDistance (cMajor, Trichord (5, 8, 0)) == Approx (2));  // Fm
    CHECK (voiceLeadingDistance (cMajor, Trichord (1, 5, 8)) == Approx (3));  // C#
    CHECK (voiceLeadingDistance (cMajor, Trichord (6, 10, 1)) == Approx (6)); // F#: tritone away
    CHECK (voiceLeadingDistance (cMajor, cMajor) == Approx (0));

    const auto vl = minimalVoiceLeading (cMajor, Trichord (5, 9, 0)); // C -> F: E->F, G->A
    CHECK (vl.size == Approx (3));
    CHECK (vl.displacement == std::array<double, 3> { 0, 1, 2 });

    CHECK (voiceLeadingDistance (cMajor, Trichord (6, 10, 1), VoiceLeadingNorm::maximum) == Approx (3));
    CHECK (voiceLeadingDistance (cMajor, Trichord (0, 3, 7), VoiceLeadingNorm::euclidean) == Approx (1));

    // Quarter tones (24-TET): neutral triad between major and minor
    CHECK (voiceLeadingDistance (Trichord (0, 8, 14, 24), Trichord (0, 7, 14, 24)) == Approx (1));
}

TEST_CASE ("Orbifold: chord types sit where the theory says")
{
    const auto r = OrbifoldEmbedding::circumradius();
    const auto h = OrbifoldEmbedding::height();

    // Evenly spaced chords lie on the central axis, in any division.
    CHECK (radius (OrbifoldEmbedding::position (Trichord (0, 4, 8))) == Approx (0).margin (1e-12));
    CHECK (radius (OrbifoldEmbedding::position (Trichord (2, 6, 10))) == Approx (0).margin (1e-12));
    CHECK (radius (OrbifoldEmbedding::position (Trichord (0, 5, 10, 15))) == Approx (0).margin (1e-12));
    CHECK (radius (OrbifoldEmbedding::position (Trichord (0, 1.0 / 3.0, 2.0 / 3.0, 1))) == Approx (0).margin (1e-12));

    // Triple unisons are at the vertices.
    for (int pc = 0; pc < 12; ++pc)
        CHECK (radius (OrbifoldEmbedding::position (Trichord (pc, pc, pc))) == Approx (r));

    // Major and minor triads are all at the same (small) distance from the axis.
    const auto rMajor = radius (OrbifoldEmbedding::position (Trichord (0, 4, 7)));
    CHECK (rMajor > 0.0);
    CHECK (rMajor < r * 0.5);
    for (int pc = 0; pc < 12; ++pc)
    {
        CHECK (radius (OrbifoldEmbedding::position (Trichord (pc, pc + 4, pc + 7))) == Approx (rMajor));
        CHECK (radius (OrbifoldEmbedding::position (Trichord (pc, pc + 3, pc + 7))) == Approx (rMajor));
    }

    // Every lattice point lies inside the prism; doubled chords lie on its faces.
    const auto tri = OrbifoldEmbedding::crossSection (0.0);
    for (const auto& c : enumerateTrichords (12))
    {
        const auto p = OrbifoldEmbedding::position (c);
        CHECK (p.z >= 0.0);
        CHECK (p.z < h);

        double minEdge = 1e9;
        for (std::size_t i = 0; i < 3; ++i)
            minEdge = std::min (minEdge, std::abs (distanceToEdge (p, tri[i], tri[(i + 1) % 3])));
        CHECK (radius (p) <= r + 1e-12);
        if (c.numDistinct() < 3)
            CHECK (minEdge == Approx (0).margin (1e-12));
        else
            CHECK (minEdge > 1e-6);
    }
}

TEST_CASE ("Orbifold: transposition is vertical, with a 1/3 twist at the wrap")
{
    const auto h = OrbifoldEmbedding::height();
    const auto c  = OrbifoldEmbedding::position (Trichord (0, 4, 7));  // sum 11
    const auto bb = OrbifoldEmbedding::position (Trichord (10, 2, 5)); // Bb: sum 17 = 5

    CHECK (bb.x == Approx (c.x).margin (1e-12));
    CHECK (bb.y == Approx (c.y).margin (1e-12));
    CHECK (c.z - bb.z == Approx (h * 6.0 / 12.0));

    // Moving C major up by a third of a semitone takes it through the top face:
    // it comes back at the bottom, rotated by 120 degrees around the axis.
    const auto wrapped = OrbifoldEmbedding::position (Trichord (0, 4, 7).transposed (1.0 / 3.0 + 1e-9));
    CHECK (wrapped.z == Approx (0).margin (1e-6));
    CHECK (radius (wrapped) == Approx (radius (c)));
    double turn = std::remainder (angle (wrapped) - angle (c), 2.0 * std::numbers::pi);
    CHECK (std::abs (turn) == Approx (2.0 * std::numbers::pi / 3.0).margin (1e-6));
}

TEST_CASE ("Orbifold: distances are voice-leading sizes")
{
    // One voice moving by a semitone = 1/12 octave, when no wrap is involved.
    const auto c  = OrbifoldEmbedding::position (Trichord (0, 4, 7));
    const auto cm = OrbifoldEmbedding::position (Trichord (0, 3, 7));
    CHECK ((c - cm).length() == Approx (1.0 / 12.0));

    // Same in 31-TET: one step = 1/31 octave.
    const auto a = OrbifoldEmbedding::position (Trichord (0, 10, 18, 31));
    const auto b = OrbifoldEmbedding::position (Trichord (0, 9, 18, 31));
    CHECK ((a - b).length() == Approx (1.0 / 31.0));
}

TEST_CASE ("Voice-leading lattice")
{
    const auto chords = enumerateTrichords (12);
    const auto edges = voiceLeadingEdges (chords);

    std::vector<int> degree (chords.size(), 0);
    for (const auto& e : edges)
    {
        CHECK (e.a < e.b);
        CHECK (voiceLeadingDistance (chords[e.a], chords[e.b]) == Approx (1));
        ++degree[e.a];
        ++degree[e.b];
    }

    // C major: Em (C->B), C#dim (C->C#), Cm, Csus4, (C E F#), Caug
    CHECK (degree[indexOf (chords, Trichord (0, 4, 7))] == 6);
    // Triple unison: only one voice can move, up or down.
    CHECK (degree[indexOf (chords, Trichord (0, 0, 0))] == 2);

    // Same lattice exists for any division.
    const auto chords19 = enumerateTrichords (19);
    for (const auto& e : voiceLeadingEdges (chords19))
        CHECK (voiceLeadingDistance (chords19[e.a], chords19[e.b]) == Approx (1));
}
