#include "TriadSpaceWidget.h"

#include <cmath>
#include <optional>

namespace mbox
{

namespace geo = mbox::geometry;
using namespace juce::gl;

namespace
{
    // Core coordinates have a prism of height 1/sqrt(3); scale it to 1 unit for display.
    const float kWorldScale = static_cast<float> (1.0 / geo::OrbifoldEmbedding::height());

    constexpr float kFieldOfView = 0.70f; // radians (~40 degrees)

    const juce::Colour kWireframe   { 0xff5a6170 };
    const juce::Colour kEdge        { 0xff8a93a6 };

    struct Category
    {
        const char* name;
        juce::Colour colour;
    };

    const std::array<Category, 7> kCategories { {
        { "Major",           juce::Colour (0xffff8a65) },
        { "Minor",           juce::Colour (0xff64b5f6) },
        { "Diminished",      juce::Colour (0xffba68c8) },
        { "Augmented",       juce::Colour (0xffffd54f) },
        { "Sus (2/4)",       juce::Colour (0xff81c784) },
        { "Other triads",    juce::Colour (0xff5b8c88) },
        { "Doubled / unison",juce::Colour (0xff6d7385) },
    } };

    /**
        Category of a trichord, by comparing its cyclic intervals (scaled to a
        12-step octave and rounded) with the familiar 12-TET triads. This keeps
        the colour code meaningful in other divisions: a 31-TET major triad
        still reads as "major".
    */
    int categoryOf (const geo::Trichord& chord)
    {
        if (chord.numDistinct() < 3)
            return 6;

        const auto iv = chord.intervals();
        const double scale = 12.0 / chord.divisions();
        std::array<int, 3> r {};
        for (std::size_t i = 0; i < 3; ++i)
            r[i] = static_cast<int> (std::lround (iv[i] * scale));

        auto isRotationOf = [&] (std::array<int, 3> pattern)
        {
            for (int k = 0; k < 3; ++k)
            {
                if (r == pattern)
                    return true;
                std::rotate (pattern.begin(), pattern.begin() + 1, pattern.end());
            }
            return false;
        };

        if (isRotationOf ({ 4, 3, 5 })) return 0;
        if (isRotationOf ({ 3, 4, 5 })) return 1;
        if (isRotationOf ({ 3, 3, 6 })) return 2;
        if (isRotationOf ({ 4, 4, 4 })) return 3;
        if (isRotationOf ({ 2, 5, 5 })) return 4;
        return 5;
    }

    using Mat4 = std::array<float, 16>;

    Mat4 multiply (const Mat4& a, const Mat4& b)
    {
        Mat4 c {};
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row)
            {
                float s = 0.0f;
                for (int k = 0; k < 4; ++k)
                    s += a[static_cast<std::size_t> (k * 4 + row)] * b[static_cast<std::size_t> (col * 4 + k)];
                c[static_cast<std::size_t> (col * 4 + row)] = s;
            }
        return c;
    }

    Mat4 perspective (float fovY, float aspect, float zNear, float zFar)
    {
        const float f = 1.0f / std::tan (fovY * 0.5f);
        Mat4 m {};
        m[0]  = f / aspect;
        m[5]  = f;
        m[10] = (zFar + zNear) / (zNear - zFar);
        m[11] = -1.0f;
        m[14] = 2.0f * zFar * zNear / (zNear - zFar);
        return m;
    }

    Mat4 lookAt (juce::Vector3D<float> eye, juce::Vector3D<float> target, juce::Vector3D<float> up)
    {
        const auto f = (target - eye).normalised();
        const auto s = (f ^ up).normalised();
        const auto u = s ^ f;
        Mat4 m {};
        m[0] = s.x;  m[4] = s.y;  m[8]  = s.z;
        m[1] = u.x;  m[5] = u.y;  m[9]  = u.z;
        m[2] = -f.x; m[6] = -f.y; m[10] = -f.z;
        m[12] = -(s * eye);
        m[13] = -(u * eye);
        m[14] = f * eye;
        m[15] = 1.0f;
        return m;
    }

    struct Vertex
    {
        float x, y, z;
        float r, g, b, a;
        float size;
    };

    Vertex makeVertex (juce::Vector3D<float> p, juce::Colour c, float size = 0.0f, float alpha = 1.0f)
    {
        return { p.x, p.y, p.z, c.getFloatRed(), c.getFloatGreen(), c.getFloatBlue(), c.getFloatAlpha() * alpha, size };
    }

    const char* const kVertexShader = R"(
        attribute vec3 position;
        attribute vec4 colour;
        attribute float pointSize;
        uniform mat4 viewProjection;
        uniform float pointScale;
        varying vec4 vColour;
        void main()
        {
            gl_Position = viewProjection * vec4 (position, 1.0);
            gl_PointSize = max (1.0, pointSize * pointScale / gl_Position.w);
            vColour = colour;
        }
    )";

    const char* const kFragmentShader = R"(
        varying vec4 vColour;
        uniform float roundPoints;
        void main()
        {
            if (roundPoints > 0.5)
            {
                vec2 d = gl_PointCoord - vec2 (0.5, 0.5);
                float r2 = dot (d, d);
                if (r2 > 0.25)
                    discard;
                float shade = 1.0 - r2 * 1.4;
                gl_FragColor = vec4 (vColour.rgb * shade, vColour.a);
            }
            else
            {
                gl_FragColor = vColour;
            }
        }
    )";
} // namespace

// ---------------------------------------------------------------------------

struct TriadSpaceWidget::GLState
{
    std::unique_ptr<juce::OpenGLShaderProgram> shader;
    GLuint vao = 0, vbo = 0;
    std::vector<Vertex> lines, edgeLines, points, pathLines, highlightPoints;
};

TriadSpaceWidget::TriadSpaceWidget (AppState& s)
    : Widget (s)
{
    edgesToggle.setToggleState (true, juce::dontSendNotification);
    edgesToggle.onClick = [this] { setShowEdges (edgesToggle.getToggleState()); };
    addAndMakeVisible (edgesToggle);

    rebuildScene (state.getDivisions());
    syncFromState (AppState::allChanged);

    glContext.setOpenGLVersionRequired (juce::OpenGLContext::openGL3_2);
    glContext.setRenderer (this);
    glContext.setComponentPaintingEnabled (true); // labels + controls drawn by paint()
    glContext.setContinuousRepainting (false);
    glContext.setMultisamplingEnabled (true);
    juce::OpenGLPixelFormat format;
    format.multisamplingLevel = 4;
    glContext.setPixelFormat (format);
    glContext.attachTo (*this);
}

TriadSpaceWidget::~TriadSpaceWidget()
{
    glContext.detach();
}

// ---------------------------------------------------------------------------
// Scene

juce::Vector3D<float> TriadSpaceWidget::worldPosition (const geo::Trichord& chord) const
{
    const auto p = geo::OrbifoldEmbedding::position (chord);
    // Core: z = transposition (up). Display: y up, centred on the origin.
    return { static_cast<float> (p.x) * kWorldScale,
             static_cast<float> (p.z) * kWorldScale - 0.5f,
             static_cast<float> (p.y) * kWorldScale };
}

void TriadSpaceWidget::rebuildScene (int newDivisions)
{
    divisions = juce::jlimit (1, theory::kMaxDivisions, newDivisions);
    auto newChords = geo::enumerateTrichords (divisions);
    auto newEdges = geo::voiceLeadingEdges (newChords);

    std::vector<juce::Vector3D<float>> newPositions;
    std::vector<juce::Colour> newColours;
    newPositions.reserve (newChords.size());
    newColours.reserve (newChords.size());
    for (const auto& c : newChords)
    {
        newPositions.push_back (worldPosition (c));
        auto colour = kCategories[static_cast<std::size_t> (categoryOf (c))].colour;
        if (c.numDistinct() == 1)
            colour = colour.brighter (0.6f);
        newColours.push_back (colour);
    }

    {
        const std::scoped_lock sl (lock);
        chords = std::move (newChords);
        edges = std::move (newEdges);
        positions = std::move (newPositions);
        colours = std::move (newColours);
        highlighted.clear();
        path.clear();
        hovered = clickedChord = pathSelected = -1;
        geometryDirty = highlightDirty = true;
    }
    glContext.triggerRepaint();
    repaint();
}

int TriadSpaceWidget::indexOf (const geo::Trichord& chord) const
{
    for (std::size_t i = 0; i < chords.size(); ++i)
        if (chords[i].sameAs (chord))
            return static_cast<int> (i);
    return -1;
}

void TriadSpaceWidget::appStateChanged (AppState&, int changes)
{
    syncFromState (changes);
}

void TriadSpaceWidget::syncFromState (int changes)
{
    if ((changes & AppState::tuningChanged) != 0 && state.getDivisions() != divisions)
        rebuildScene (state.getDivisions());

    const auto active = trichordsIn (state.getActivePitchClasses());

    // Progression chords that are three-note chords (triads, sus...), in order.
    std::vector<geo::Trichord> progression;
    std::vector<bool> isSelected;
    const auto& prog = state.getProgression();
    for (std::size_t i = 0; i < prog.size(); ++i)
    {
        const auto pcs = prog[i].chord.pitchClasses().toVector();
        if (pcs.size() != 3)
            continue;
        progression.emplace_back (pcs[0].value(), pcs[1].value(), pcs[2].value(), divisions);
        isSelected.push_back (static_cast<int> (i) == state.getSelectedChordIndex());
    }

    {
        const std::scoped_lock sl (lock);
        highlighted.clear();
        for (const auto& t : active)
            if (const int i = indexOf (t); i >= 0)
                highlighted.push_back (i);

        if (std::find (highlighted.begin(), highlighted.end(), clickedChord) == highlighted.end())
            clickedChord = -1; // the clicked chord stopped sounding

        path.clear();
        pathSelected = -1;
        for (std::size_t i = 0; i < progression.size(); ++i)
        {
            path.push_back (indexOf (progression[i]));
            if (isSelected[i])
                pathSelected = path.back();
        }
        highlightDirty = true;
    }
    glContext.triggerRepaint();
    repaint();
}

std::vector<geo::Trichord> TriadSpaceWidget::trichordsIn (const theory::PitchClassSet& set)
{
    std::vector<geo::Trichord> out;
    const int d = set.divisions();
    const auto pcs = set.toVector();
    const auto n = pcs.size();
    if (n == 1)
    {
        out.emplace_back (pcs[0].value(), pcs[0].value(), pcs[0].value(), d);
    }
    else if (n == 2)
    {
        out.emplace_back (pcs[0].value(), pcs[0].value(), pcs[1].value(), d);
        out.emplace_back (pcs[0].value(), pcs[1].value(), pcs[1].value(), d);
    }
    else if (n <= 6)
    {
        for (std::size_t a = 0; a < n; ++a)
            for (std::size_t b = a + 1; b < n; ++b)
                for (std::size_t c = b + 1; c < n; ++c)
                    out.emplace_back (pcs[a].value(), pcs[b].value(), pcs[c].value(), d);
    }
    return out;
}

void TriadSpaceWidget::setShowEdges (bool shouldShow)
{
    {
        const std::scoped_lock sl (lock);
        showEdges = shouldShow;
    }
    edgesToggle.setToggleState (shouldShow, juce::dontSendNotification);
    glContext.triggerRepaint();
}

juce::String TriadSpaceWidget::describe (const geo::Trichord& chord)
{
    const auto& p = chord.pitches();
    const int d = chord.divisions();

    const bool onGrid = std::all_of (p.begin(), p.end(), [] (double v) { return std::abs (v - std::round (v)) < 1e-6; });
    if (! onGrid)
        return juce::String (p[0], 2) + " / " + juce::String (p[1], 2) + " / " + juce::String (p[2], 2) + " (" + juce::String (d) + "-TET)";

    juce::String notes;
    auto set = theory::PitchClassSet::withDivisions (d);
    for (const double v : p)
    {
        const theory::PitchClass pc (juce::roundToInt (v), d);
        set.insert (pc);
        notes << (notes.isEmpty() ? "" : " ") << pc.name();
    }

    if (chord.numDistinct() == 3)
    {
        const auto found = theory::identifyChords (set);
        if (! found.empty())
            return juce::String (found.front().symbol()) + "  (" + notes + ")";
    }
    return notes;
}

// ---------------------------------------------------------------------------
// Camera

TriadSpaceWidget::Mat4 TriadSpaceWidget::viewProjection (const Camera& cam, float aspect) const
{
    const float cp = std::cos (cam.pitch);
    const juce::Vector3D<float> dir { cp * std::sin (cam.yaw), std::sin (cam.pitch), cp * std::cos (cam.yaw) };
    const juce::Vector3D<float> worldUp { 0.0f, 1.0f, 0.0f };
    const auto right = (worldUp ^ dir).normalised();
    const auto up = dir ^ right;

    const auto target = right * cam.pan.x + up * cam.pan.y;
    const auto eye = target + dir * cam.distance;

    return multiply (perspective (kFieldOfView, juce::jmax (0.1f, aspect), 0.05f, 50.0f),
                     lookAt (eye, target, worldUp));
}

juce::Point<float> TriadSpaceWidget::toScreen (const Mat4& vp, juce::Vector3D<float> p, bool& visible) const
{
    const float cx = vp[0] * p.x + vp[4] * p.y + vp[8]  * p.z + vp[12];
    const float cy = vp[1] * p.x + vp[5] * p.y + vp[9]  * p.z + vp[13];
    const float cw = vp[3] * p.x + vp[7] * p.y + vp[11] * p.z + vp[15];
    visible = cw > 1e-4f;
    if (! visible)
        return {};
    return { (cx / cw + 1.0f) * 0.5f * static_cast<float> (getWidth()),
             (1.0f - cy / cw) * 0.5f * static_cast<float> (getHeight()) };
}

int TriadSpaceWidget::pickChord (juce::Point<float> screenPos) const
{
    const std::scoped_lock sl (lock);
    const auto vp = viewProjection (camera, static_cast<float> (getWidth()) / juce::jmax (1, getHeight()));

    int best = -1;
    float bestDist = 10.0f; // pixels
    for (std::size_t i = 0; i < positions.size(); ++i)
    {
        bool visible = false;
        const auto s = toScreen (vp, positions[i], visible);
        const auto d = s.getDistanceFrom (screenPos);
        if (visible && d < bestDist)
        {
            bestDist = d;
            best = static_cast<int> (i);
        }
    }
    return best;
}

void TriadSpaceWidget::mouseDown (const juce::MouseEvent&)
{
    const std::scoped_lock sl (lock);
    dragStartCamera = camera;
    dragMoved = false;
}

void TriadSpaceWidget::mouseDrag (const juce::MouseEvent& e)
{
    const auto offset = e.getOffsetFromDragStart().toFloat();
    if (offset.getDistanceFromOrigin() > 3.0f)
        dragMoved = true;
    if (! dragMoved)
        return;

    {
        const std::scoped_lock sl (lock);
        camera = dragStartCamera;
        if (e.mods.isRightButtonDown() || e.mods.isShiftDown() || e.mods.isMiddleButtonDown())
        {
            const float unitsPerPixel = 2.0f * camera.distance * std::tan (kFieldOfView * 0.5f) / juce::jmax (1, getHeight());
            camera.pan = dragStartCamera.pan + juce::Point<float> (-offset.x, offset.y) * unitsPerPixel;
        }
        else
        {
            camera.yaw = dragStartCamera.yaw - offset.x * 0.008f;
            camera.pitch = juce::jlimit (-1.5f, 1.5f, dragStartCamera.pitch + offset.y * 0.008f);
        }
    }
    glContext.triggerRepaint();
    repaint();
}

void TriadSpaceWidget::mouseUp (const juce::MouseEvent& e)
{
    if (dragMoved || ! e.mods.isLeftButtonDown())
        return;

    const int picked = pickChord (e.position);
    if (picked < 0)
        return;

    std::optional<geo::Trichord> clicked;
    {
        const std::scoped_lock sl (lock);
        if (picked != clickedChord)
            clicked = chords[static_cast<std::size_t> (picked)];
        clickedChord = clicked ? picked : -1;
    }

    if (! clicked)
    {
        state.clearManualNotes();
        return;
    }

    // Close voicing upwards from the lowest pitch class, starting at octave 4.
    const auto& tuning = state.getTuning();
    std::vector<int> notes;
    for (const double v : clicked->pitches())
    {
        int note = tuning.noteOf (theory::PitchClass (juce::roundToInt (v), divisions), 4);
        while (! notes.empty() && note <= notes.back())
            note += divisions;
        notes.push_back (note);
    }
    state.setManualNotes (std::move (notes));
}

void TriadSpaceWidget::mouseMove (const juce::MouseEvent& e)
{
    const int picked = pickChord (e.position);
    {
        const std::scoped_lock sl (lock);
        if (picked == hovered)
            return;
        hovered = picked;
        highlightDirty = true;
    }
    glContext.triggerRepaint();
    repaint();
}

void TriadSpaceWidget::mouseExit (const juce::MouseEvent&)
{
    {
        const std::scoped_lock sl (lock);
        if (hovered < 0)
            return;
        hovered = -1;
        highlightDirty = true;
    }
    glContext.triggerRepaint();
    repaint();
}

void TriadSpaceWidget::mouseDoubleClick (const juce::MouseEvent&)
{
    {
        const std::scoped_lock sl (lock);
        camera = {};
    }
    glContext.triggerRepaint();
    repaint();
}

void TriadSpaceWidget::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    {
        const std::scoped_lock sl (lock);
        camera.distance = juce::jlimit (0.4f, 10.0f, camera.distance * std::exp (-wheel.deltaY * 0.8f));
    }
    glContext.triggerRepaint();
    repaint();
}

// ---------------------------------------------------------------------------
// Layout + 2D overlay (painted on top of the GL scene)

void TriadSpaceWidget::resized()
{
    auto top = getLocalBounds().reduced (8).removeFromTop (24);
    edgesToggle.setBounds (top.removeFromRight (170));
}

void TriadSpaceWidget::paint (juce::Graphics& g)
{
    // No background fill: the OpenGL scene is underneath.
    struct Label { juce::Point<float> at; juce::String text; juce::Colour colour; };
    std::vector<Label> labels;
    int numChords = 0;

    {
        const std::scoped_lock sl (lock);
        numChords = static_cast<int> (chords.size());
        const auto vp = viewProjection (camera, static_cast<float> (getWidth()) / juce::jmax (1, getHeight()));

        auto addLabel = [&] (int index, juce::Colour colour)
        {
            if (index < 0 || index >= numChords)
                return;
            bool visible = false;
            const auto s = toScreen (vp, positions[static_cast<std::size_t> (index)], visible);
            if (visible)
                labels.push_back ({ s, describe (chords[static_cast<std::size_t> (index)]), colour });
        };

        for (const int h : highlighted)
            addLabel (h, colours::accent);
        if (std::find (highlighted.begin(), highlighted.end(), pathSelected) == highlighted.end())
            addLabel (pathSelected, colours::secondary);
        addLabel (hovered, juce::Colours::white);

        // Axis caption next to the top of the central axis.
        bool visible = false;
        const auto axisTop = toScreen (vp, { 0.0f, 0.5f, 0.0f }, visible);
        if (visible)
            labels.push_back ({ axisTop, juce::String (juce::CharPointer_UTF8 ("transposition \xe2\x86\x91")), kWireframe.brighter (0.5f) });
    }

    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    for (const auto& l : labels)
    {
        const auto width = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), l.text) + 10.0f;
        const auto box = juce::Rectangle<float> (width, 18.0f).withPosition (l.at + juce::Point<float> (8.0f, -22.0f));
        g.setColour (colours::background.withAlpha (0.75f));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (l.colour);
        g.drawText (l.text, box, juce::Justification::centred);
    }

    // Title + legend
    auto area = getLocalBounds().reduced (10);
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText ("Three-note chord space", area.removeFromTop (20), juce::Justification::centredLeft);
    g.setColour (juce::Colours::grey);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (juce::String (numChords) + " chords in " + juce::String (divisions) + "-TET - Tymoczko orbifold", area.removeFromTop (16), juce::Justification::centredLeft);

    auto legend = area.removeFromBottom (static_cast<int> (kCategories.size()) * 16);
    for (const auto& cat : kCategories)
    {
        auto row = legend.removeFromTop (16);
        g.setColour (cat.colour);
        g.fillEllipse (row.removeFromLeft (16).toFloat().withSizeKeepingCentre (9.0f, 9.0f));
        g.setColour (juce::Colours::lightgrey);
        g.drawText (cat.name, row, juce::Justification::centredLeft);
    }

    g.setColour (juce::Colours::grey);
    g.setFont (juce::FontOptions (11.0f));
    g.drawFittedText ("click: play chord\ndrag: orbit\nshift/right-drag: pan\nwheel: zoom\ndouble-click: reset",
                      getLocalBounds().reduced (10).removeFromBottom (75).removeFromRight (130),
                      juce::Justification::bottomRight, 5);
}

// ---------------------------------------------------------------------------
// OpenGL (render thread)

void TriadSpaceWidget::newOpenGLContextCreated()
{
    gl = std::make_unique<GLState>();

    auto shader = std::make_unique<juce::OpenGLShaderProgram> (glContext);
    const bool ok = shader->addVertexShader (juce::OpenGLHelpers::translateVertexShaderToV3 (kVertexShader))
                 && shader->addFragmentShader (juce::OpenGLHelpers::translateFragmentShaderToV3 (kFragmentShader))
                 && shader->link();
    if (ok)
    {
        gl->shader = std::move (shader);
    }
    else
    {
        DBG ("TriadSpaceView shader error: " << shader->getLastError());
    }

    glGenVertexArrays (1, &gl->vao);
    glGenBuffers (1, &gl->vbo);

    const std::scoped_lock sl (lock);
    geometryDirty = highlightDirty = true;
}

void TriadSpaceWidget::openGLContextClosing()
{
    if (gl != nullptr)
    {
        glDeleteBuffers (1, &gl->vbo);
        glDeleteVertexArrays (1, &gl->vao);
        gl->shader.reset();
    }
    gl.reset();
}

void TriadSpaceWidget::renderOpenGL()
{
    const float scale = static_cast<float> (glContext.getRenderingScale());
    const int widthPx = juce::roundToInt (scale * static_cast<float> (getWidth()));
    const int heightPx = juce::roundToInt (scale * static_cast<float> (getHeight()));

    glViewport (0, 0, widthPx, heightPx);
    juce::OpenGLHelpers::clear (colours::background);

    if (gl == nullptr || gl->shader == nullptr)
        return;

    Mat4 vp;
    bool drawEdges = true;
    float pointWorldSize = 0.0f;
    {
        const std::scoped_lock sl (lock);
        vp = viewProjection (camera, static_cast<float> (widthPx) / juce::jmax (1, heightPx));
        drawEdges = showEdges;
        pointWorldSize = juce::jlimit (0.006f, 0.035f, 0.32f / static_cast<float> (divisions));

        if (geometryDirty)
        {
            geometryDirty = false;

            // Prism wireframe: bottom and top triangles, vertical edges, central axis.
            gl->lines.clear();
            const auto h = geo::OrbifoldEmbedding::height();
            const auto bottom = geo::OrbifoldEmbedding::crossSection (0.0);
            const auto top = geo::OrbifoldEmbedding::crossSection (h);
            auto toWorld = [] (geo::Vec3 v)
            {
                return juce::Vector3D<float> (static_cast<float> (v.x) * kWorldScale,
                                              static_cast<float> (v.z) * kWorldScale - 0.5f,
                                              static_cast<float> (v.y) * kWorldScale);
            };
            for (std::size_t i = 0; i < 3; ++i)
            {
                const auto j = (i + 1) % 3;
                for (const auto& tri : { bottom, top })
                {
                    gl->lines.push_back (makeVertex (toWorld (tri[i]), kWireframe));
                    gl->lines.push_back (makeVertex (toWorld (tri[j]), kWireframe));
                }
                gl->lines.push_back (makeVertex (toWorld (bottom[i]), kWireframe));
                gl->lines.push_back (makeVertex (toWorld (top[i]), kWireframe));
            }
            gl->lines.push_back (makeVertex ({ 0.0f, -0.5f, 0.0f }, kWireframe, 0.0f, 0.5f));
            gl->lines.push_back (makeVertex ({ 0.0f, 0.5f, 0.0f }, kWireframe, 0.0f, 0.5f));

            gl->edgeLines.clear();
            gl->edgeLines.reserve (edges.size() * 2);
            const float edgeAlpha = divisions <= 12 ? 0.22f : (divisions <= 24 ? 0.12f : 0.05f);
            for (const auto& e : edges)
            {
                gl->edgeLines.push_back (makeVertex (positions[e.a], kEdge, 0.0f, edgeAlpha));
                gl->edgeLines.push_back (makeVertex (positions[e.b], kEdge, 0.0f, edgeAlpha));
            }

            gl->points.clear();
            gl->points.reserve (positions.size());
            for (std::size_t i = 0; i < positions.size(); ++i)
                gl->points.push_back (makeVertex (positions[i], colours[i], pointWorldSize));
        }

        if (highlightDirty)
        {
            highlightDirty = false;
            gl->highlightPoints.clear();
            gl->pathLines.clear();

            // Progression path: consecutive three-note chords joined by a line.
            int previous = -1;
            for (const int p : path)
            {
                if (p >= 0)
                {
                    if (previous >= 0 && previous != p)
                    {
                        gl->pathLines.push_back (makeVertex (positions[static_cast<std::size_t> (previous)], colours::secondary, 0.0f, 0.8f));
                        gl->pathLines.push_back (makeVertex (positions[static_cast<std::size_t> (p)], colours::secondary, 0.0f, 0.8f));
                    }
                    previous = p;
                }
            }

            auto emphasise = [&] (int index, juce::Colour c, float sizeFactor)
            {
                if (index < 0 || index >= static_cast<int> (positions.size()))
                    return;
                const auto& p = positions[static_cast<std::size_t> (index)];
                const auto worldSize = juce::jmax (pointWorldSize, 0.02f);
                gl->highlightPoints.push_back (makeVertex (p, c, worldSize * sizeFactor * 1.8f, 0.25f)); // halo
                gl->highlightPoints.push_back (makeVertex (p, c, worldSize * sizeFactor));
            };

            for (const int p : path)
                emphasise (p, colours::secondary, p == pathSelected ? 1.5f : 1.0f);
            for (const int h : highlighted)
                emphasise (h, colours::accent, 1.5f);
            emphasise (hovered, juce::Colours::white, 1.2f);
        }
    }

    auto& shader = *gl->shader;
    shader.use();
    shader.setUniformMat4 ("viewProjection", vp.data(), 1, false);
    shader.setUniform ("pointScale", static_cast<float> (heightPx) * 0.5f / std::tan (kFieldOfView * 0.5f));

    glBindVertexArray (gl->vao);
    glBindBuffer (GL_ARRAY_BUFFER, gl->vbo);

    const auto stride = static_cast<GLsizei> (sizeof (Vertex));
    const auto posAttr = static_cast<GLuint> (glGetAttribLocation (shader.getProgramID(), "position"));
    const auto colAttr = static_cast<GLuint> (glGetAttribLocation (shader.getProgramID(), "colour"));
    const auto sizeAttr = static_cast<GLuint> (glGetAttribLocation (shader.getProgramID(), "pointSize"));

    auto draw = [&] (const std::vector<Vertex>& vertices, GLenum mode)
    {
        if (vertices.empty())
            return;
        glBufferData (GL_ARRAY_BUFFER, static_cast<GLsizeiptr> (vertices.size() * sizeof (Vertex)), vertices.data(), GL_STREAM_DRAW);
        glVertexAttribPointer (posAttr, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
        glVertexAttribPointer (colAttr, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*> (offsetof (Vertex, r)));
        glVertexAttribPointer (sizeAttr, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*> (offsetof (Vertex, size)));
        glEnableVertexAttribArray (posAttr);
        glEnableVertexAttribArray (colAttr);
        glEnableVertexAttribArray (sizeAttr);
        glDrawArrays (mode, 0, static_cast<GLsizei> (vertices.size()));
    };

    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable (GL_PROGRAM_POINT_SIZE);
    glEnable (GL_DEPTH_TEST);
    glDepthFunc (GL_LEQUAL);
    glClear (GL_DEPTH_BUFFER_BIT);

    // Translucent lines first, without writing depth.
    glDepthMask (GL_FALSE);
    shader.setUniform ("roundPoints", 0.0f);
    draw (gl->lines, GL_LINES);
    if (drawEdges)
        draw (gl->edgeLines, GL_LINES);

    // Opaque round points.
    glDepthMask (GL_TRUE);
    shader.setUniform ("roundPoints", 1.0f);
    draw (gl->points, GL_POINTS);

    // Progression path and highlights always on top.
    glDisable (GL_DEPTH_TEST);
    shader.setUniform ("roundPoints", 0.0f);
    draw (gl->pathLines, GL_LINES);
    shader.setUniform ("roundPoints", 1.0f);
    draw (gl->highlightPoints, GL_POINTS);

    glDisableVertexAttribArray (posAttr);
    glDisableVertexAttribArray (colAttr);
    glDisableVertexAttribArray (sizeAttr);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glBindVertexArray (0);
    glDisable (GL_PROGRAM_POINT_SIZE);
    glUseProgram (0);
}

} // namespace mbox
