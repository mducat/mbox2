#pragma once

#include "Widget.h"

#include <juce_opengl/juce_opengl.h>

#include <mbox/geometry/TrichordSpace.h>

#include <array>
#include <functional>
#include <mutex>
#include <vector>

namespace mbox
{

/**
    Navigable 3D view of three-note chord space (Tymoczko's orbifold).

    Every 3-note pitch-class multiset of the current N-TET is drawn as a point
    inside a triangular prism: transposition runs along the vertical axis,
    augmented (evenly spaced) chords sit on the central axis, chords with a
    doubled note on the faces and triple unisons on the edges. Thin lines join
    chords that are one step of voice leading apart.

    Shared state (AppState):
      - the division follows the current tuning;
      - the active notes (MIDI input + notes toggled in other widgets) light up
        every 3-note chord they contain (a seventh chord shows its four triads);
      - the progression's three-note chords are drawn as a path, the selected
        one emphasised;
      - clicking a point plays that chord as manual notes (click again to clear).

    Rendering uses OpenGL on its own thread; the geometry itself comes from
    mbox::geometry (core), so the same view works for any equal division.

    Mouse: drag = orbit, right-drag / shift-drag = pan, wheel = zoom,
    double-click = reset camera, click on a point = play it.
*/
class TriadSpaceWidget final : public Widget,
                               private juce::OpenGLRenderer
{
public:
    explicit TriadSpaceWidget (AppState&);
    ~TriadSpaceWidget() override;

    /** Label shown for a chord: chord name when recognised, else its notes (or steps if fractional). */
    static juce::String describe (const geometry::Trichord&);

    /** The 3-note chords contained in a pitch-class set (subsets of up to 6 notes, doublings for dyads). */
    static std::vector<geometry::Trichord> trichordsIn (const theory::PitchClassSet&);

    void resized() override;
    void paint (juce::Graphics&) override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void appStateChanged (AppState&, int changes) override;
    void syncFromState (int changes);
    void setShowEdges (bool shouldShow);

    struct Camera
    {
        float yaw = 0.6f, pitch = 0.3f, distance = 3.1f;
        juce::Point<float> pan;
    };

    using Mat4 = std::array<float, 16>;

    // OpenGLRenderer (GL thread)
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

    void rebuildScene (int newDivisions);
    int indexOf (const geometry::Trichord&) const; // caller holds lock
    Mat4 viewProjection (const Camera&, float aspect) const;
    juce::Point<float> toScreen (const Mat4& vp, juce::Vector3D<float> world, bool& visible) const;
    int pickChord (juce::Point<float> screenPos) const;
    juce::Vector3D<float> worldPosition (const geometry::Trichord&) const;

    juce::OpenGLContext glContext;

    // --- Scene, shared between the message and GL threads (guarded by lock) ---
    mutable std::mutex lock;
    int divisions = 12;
    std::vector<geometry::Trichord> chords;
    std::vector<juce::Vector3D<float>> positions;
    std::vector<juce::Colour> colours;
    std::vector<geometry::LatticeEdge> edges;
    std::vector<int> highlighted; // indices into chords: active notes
    std::vector<int> path;        // progression chords, in order (-1 = not a 3-note chord)
    int pathSelected = -1;        // index into chords of the selected progression chord
    int hovered = -1, clickedChord = -1;
    bool showEdges = true;
    bool geometryDirty = true, highlightDirty = true;
    Camera camera;

    // --- GL-thread-only state ---
    struct GLState;
    std::unique_ptr<GLState> gl;

    // --- Message-thread interaction state ---
    juce::ToggleButton edgesToggle { "Voice-leading lattice" };
    Camera dragStartCamera;
    bool dragMoved = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TriadSpaceWidget)
};

} // namespace mbox
