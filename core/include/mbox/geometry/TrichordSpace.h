#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace mbox::geometry
{

/** Minimal 3D vector, kept here so core stays free of any UI/graphics dependency. */
struct Vec3
{
    double x = 0.0, y = 0.0, z = 0.0;

    friend Vec3 operator+ (Vec3 a, Vec3 b) noexcept { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
    friend Vec3 operator- (Vec3 a, Vec3 b) noexcept { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
    friend Vec3 operator* (Vec3 a, double s) noexcept { return { a.x * s, a.y * s, a.z * s }; }
    double length() const noexcept;
};

/**
    A three-note chord as an unordered multiset of pitch classes, measured in
    steps of an equal division of the octave (N-TET). Pitches may be fractional,
    so the same object describes 12-TET triads, 19/31/53-TET chords, or any
    continuous (just, detuned...) three-note sonority.

    Invariant: pitches are sorted ascending and lie in [0, divisions).
*/
class Trichord
{
public:
    Trichord (double a, double b, double c, int divisions = 12) noexcept;

    int divisions() const noexcept { return edo; }
    const std::array<double, 3>& pitches() const noexcept { return pcs; }

    /** Sum of the pitch classes modulo the octave, in [0, divisions): the transposition coordinate. */
    double sumClass() const noexcept;

    /** Cyclic step intervals (b-a, c-b, a+N-c). They add up to divisions. */
    std::array<double, 3> intervals() const noexcept;

    /** Transposition by `steps` (wraps around the octave). */
    Trichord transposed (double steps) const noexcept;

    /** Number of distinct pitch classes (1 = unison, 2 = one doubled note, 3 = proper triad). */
    int numDistinct (double tolerance = 1e-9) const noexcept;

    /** Exact equality of the multisets, within `tolerance` steps. */
    bool sameAs (const Trichord& other, double tolerance = 1e-9) const noexcept;

private:
    std::array<double, 3> pcs {};
    int edo = 12;
};

/** Norm used to measure the size of a voice leading. */
enum class VoiceLeadingNorm
{
    taxicab,   // L1: total number of steps moved by all voices
    euclidean, // L2: straight-line distance in the orbifold
    maximum    // L-infinity: largest single voice movement
};

/** The smallest voice leading between two trichords (each voice moves to one note of the target). */
struct VoiceLeading
{
    /** For each voice i of the source (sorted order): signed displacement in steps (shortest path). */
    std::array<double, 3> displacement {};
    /** For each voice i of the source: index of the target pitch it moves to. */
    std::array<int, 3> targetIndex {};
    double size = 0.0;
};

/** Minimal voice leading from `from` to `to` (both must share the same division). */
VoiceLeading minimalVoiceLeading (const Trichord& from, const Trichord& to,
                                  VoiceLeadingNorm norm = VoiceLeadingNorm::taxicab) noexcept;

inline double voiceLeadingDistance (const Trichord& a, const Trichord& b,
                                    VoiceLeadingNorm norm = VoiceLeadingNorm::taxicab) noexcept
{
    return minimalVoiceLeading (a, b, norm).size;
}

/**
    Embedding of three-note chord space T^3/S_3 (Tymoczko's orbifold, "The
    Geometry of Musical Chords", Science 2006) into R^3.

    Units: one octave = 1, whatever the division, so 12-TET and 31-TET chords
    live in the same solid.

    - The vertical axis (z) is the sum of the pitch classes, i.e. transposition:
      transposing a chord moves it straight up, and the space wraps around
      after an octave of total motion, with the top face glued to the bottom
      face after a 1/3 turn.
    - The horizontal cross-section is an equilateral triangle describing the
      chord's shape: augmented triads (evenly spaced chords) sit on the central
      axis (x = y = 0), triple unisons at the vertices, chords with a doubled
      note on the edges. Major and minor triads circle near the axis.

    The coordinates are an isometric projection: Euclidean distance between
    two points equals the L2 size of the voice leading between them, as long as
    that voice leading does not cross the wrap-around face.
*/
class OrbifoldEmbedding
{
public:
    /** Height of the prism (one octave of total motion along the (1,1,1) direction). */
    static double height() noexcept;

    /** Distance from the central axis to a vertex (triple unison) of the cross-section. */
    static double circumradius() noexcept;

    /** The three vertices of the cross-section triangle at height z. */
    static std::array<Vec3, 3> crossSection (double z) noexcept;

    /** Position of a chord in the prism (x, y horizontal, z in [0, height())). */
    static Vec3 position (const Trichord& chord) noexcept;

    /**
        The ordered representative (y1 <= y2 <= y3 <= y1 + 1 octave, in steps) whose
        sum lies in [0, divisions). This is the "lift" of the chord that the
        embedding places in the prism.
    */
    static std::array<double, 3> representative (const Trichord& chord) noexcept;
};

/** All 3-note multisets of pitch classes of an N-TET (C(N+2, 3) chords; 364 for 12-TET). */
std::vector<Trichord> enumerateTrichords (int divisions, bool includeDoublings = true);

/** A pair of lattice chords joined by a single voice moving by one step. */
struct LatticeEdge
{
    std::uint32_t a = 0, b = 0;
};

/** Edges of the voice-leading lattice among `chords` (pairs at taxicab distance exactly `steps`). */
std::vector<LatticeEdge> voiceLeadingEdges (const std::vector<Trichord>& chords, double steps = 1.0);

} // namespace mbox::geometry
