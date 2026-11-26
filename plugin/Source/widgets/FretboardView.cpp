#include "FretboardView.h"

using namespace instrument_ui;
namespace inst = mbox::instrument;

FretboardView::FretboardView() = default;

void FretboardView::setFretboard (const inst::Fretboard& b)
{
    board = b;
    repaint();
}

void FretboardView::setSnapshot (const Snapshot& s)
{
    snapshot = s;
    repaint();
}

juce::Rectangle<float> FretboardView::neckArea() const
{
    auto r = getLocalBounds().toFloat();
    r.removeFromBottom (18.0f);          // fret numbers
    r.removeFromLeft (labelColumn + openColumn);
    r.removeFromRight (6.0f);
    return r.reduced (0.0f, 6.0f);
}

float FretboardView::fretX (int fret) const
{
    const auto neck = neckArea();
    const double full = board.fretDistance (board.numFrets());
    const double d = full > 0.0 ? board.fretDistance (fret) / full : 0.0;
    return neck.getX() + static_cast<float> (d) * neck.getWidth();
}

float FretboardView::noteX (int fret) const
{
    if (fret == 0)
        return neckArea().getX() - openColumn * 0.5f;
    return (fretX (fret - 1) + fretX (fret)) * 0.5f;
}

float FretboardView::stringY (int string) const
{
    const auto neck = neckArea();
    const int n = board.numStrings();
    if (n <= 1)
        return neck.getCentreY();
    const float gap = neck.getHeight() / static_cast<float> (n);
    // Lowest string at the bottom.
    return neck.getBottom() - gap * (static_cast<float> (string) + 0.5f);
}

void FretboardView::paint (juce::Graphics& g)
{
    g.fillAll (background);

    const auto neck = neckArea();
    const int numStrings = board.numStrings();
    const float gap = neck.getHeight() / static_cast<float> (juce::jmax (1, numStrings));
    const float dotSize = juce::jlimit (10.0f, 22.0f, gap * 0.78f);

    // Wood + inlays
    g.setColour (panel);
    g.fillRect (neck);

    g.setColour (dim.withAlpha (0.7f));
    for (const int f : board.inlayFrets())
    {
        const float x = noteX (f);
        const bool octave = f % board.stepsPerOctave() == 0;
        const float s = juce::jmin (9.0f, gap * 0.45f);
        if (octave)
        {
            g.fillEllipse (juce::Rectangle<float> (s, s).withCentre ({ x, neck.getY() + neck.getHeight() * 0.25f }));
            g.fillEllipse (juce::Rectangle<float> (s, s).withCentre ({ x, neck.getY() + neck.getHeight() * 0.75f }));
        }
        else
        {
            g.fillEllipse (juce::Rectangle<float> (s, s).withCentre ({ x, neck.getCentreY() }));
        }
    }

    // Frets
    for (int f = 1; f <= board.numFrets(); ++f)
    {
        const float x = fretX (f);
        g.setColour (juce::Colour (0xff6b7180));
        g.drawLine (x, neck.getY(), x, neck.getBottom(), 1.2f);
    }

    // Fret numbers (inlay frets only, to stay readable when frets get narrow)
    g.setFont (juce::FontOptions (10.0f));
    g.setColour (juce::Colours::grey);
    for (const int f : board.inlayFrets())
        g.drawText (juce::String (f), juce::Rectangle<float> (30.0f, 14.0f).withCentre ({ noteX (f), static_cast<float> (getHeight()) - 9.0f }),
                    juce::Justification::centred);

    // Nut
    g.setColour (juce::Colour (0xffd8d2c0));
    g.fillRect (juce::Rectangle<float> (neck.getX() - 3.0f, neck.getY(), 4.0f, neck.getHeight()));

    // Strings (thicker for lower ones) + string names
    for (int s = 0; s < numStrings; ++s)
    {
        const float y = stringY (s);
        const float thickness = juce::jmap (static_cast<float> (s), 0.0f, static_cast<float> (juce::jmax (1, numStrings - 1)), 2.4f, 0.9f);
        g.setColour (juce::Colour (0xffb8bcc6));
        g.drawLine (neck.getX() - openColumn + 6.0f, y, neck.getRight(), y, thickness);

        g.setColour (text);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        const auto name = snapshot.nameOf ? snapshot.nameOf (board.pitchAt (s, 0)) : juce::String (board.pitchAt (s, 0));
        g.drawText (name, juce::Rectangle<float> (0.0f, y - 8.0f, labelColumn - 4.0f, 16.0f), juce::Justification::centredRight);
    }

    const bool anyStringChosen = std::any_of (snapshot.stringFrets.begin(), snapshot.stringFrets.end(), [] (int f) { return f >= 0; });

    auto stringFret = [&] (int s)
    {
        return s < static_cast<int> (snapshot.stringFrets.size()) ? snapshot.stringFrets[static_cast<std::size_t> (s)] : -1;
    };

    // Pitches that are on but not placed on a string: shown at every exact position.
    std::set<int> placed;
    for (int s = 0; s < numStrings; ++s)
        if (const int f = stringFret (s); f >= 0)
            placed.insert (board.pitchAt (s, f));

    auto drawDot = [&] (float x, float y, float size, juce::Colour fill, bool isRoot, const juce::String& label)
    {
        const auto r = juce::Rectangle<float> (size, size).withCentre ({ x, y });
        g.setColour (fill);
        g.fillEllipse (r);
        if (isRoot)
        {
            g.setColour (root);
            g.drawEllipse (r.expanded (1.5f), 2.0f);
        }
        if (label.isNotEmpty())
        {
            g.setColour (background);
            g.setFont (juce::FontOptions (juce::jmin (11.0f, size * 0.55f), juce::Font::bold));
            g.drawFittedText (label, r.toNearestInt(), juce::Justification::centred, 1, 0.5f);
        }
    };

    for (int s = 0; s < numStrings; ++s)
    {
        const float y = stringY (s);
        const int chosen = stringFret (s);

        for (int f = 0; f <= board.numFrets(); ++f)
        {
            const int pitch = board.pitchAt (s, f);
            const float x = noteX (f);
            const bool on = snapshot.isManual (pitch) || snapshot.isHeld (pitch);

            if (f == chosen)
                drawDot (x, y, dotSize, activeColour (snapshot, pitch), snapshot.isRoot (pitch),
                         snapshot.nameOf ? snapshot.nameOf (pitch) : juce::String());
            else if (on && placed.count (pitch) == 0)
                drawDot (x, y, dotSize * 0.62f, activeColour (snapshot, pitch).withAlpha (0.85f), snapshot.isRoot (pitch), {});
            else if (snapshot.isChordClass (pitch) && ! (f == 0 && anyStringChosen && chosen < 0))
                drawDot (x, y, dotSize * 0.38f, (snapshot.isRoot (pitch) ? root : accent).withAlpha (0.35f), false, {});
        }

        const int openPitch = board.pitchAt (s, 0);
        if (anyStringChosen && chosen < 0 && ! snapshot.isManual (openPitch) && ! snapshot.isHeld (openPitch))
        {
            g.setColour (text);
            g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
            g.drawText ("x", juce::Rectangle<float> (16.0f, 16.0f).withCentre ({ noteX (0), y }), juce::Justification::centred);
        }
    }
}

void FretboardView::mouseDown (const juce::MouseEvent& e)
{
    const int numStrings = board.numStrings();
    if (numStrings == 0)
        return;

    int string = 0;
    float best = std::numeric_limits<float>::max();
    for (int s = 0; s < numStrings; ++s)
        if (const float d = std::abs (stringY (s) - e.position.y); d < best)
        {
            best = d;
            string = s;
        }

    int fret = -1;
    const auto neck = neckArea();
    if (e.position.x < neck.getX() - openColumn)
        return;
    if (e.position.x < neck.getX())
        fret = 0;
    else
        for (int f = 1; f <= board.numFrets(); ++f)
            if (e.position.x < fretX (f))
            {
                fret = f;
                break;
            }

    if (fret >= 0 && onPositionClicked)
        onPositionClicked ({ string, fret });
}
