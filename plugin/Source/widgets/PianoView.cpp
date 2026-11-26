#include "PianoView.h"

using namespace instrument_ui;

PianoView::PianoView()
{
    setRepaintsOnMouseActivity (false);
}

void PianoView::setRange (int stepsPerOctave, int firstPitch, int lastPitch)
{
    if (stepsPerOctave != layout.stepsPerOctave())
        layout = mbox::instrument::KeyboardLayout::forEqualTemperament (stepsPerOctave);

    // Start and end on lower keys so the drawing has clean edges.
    while (firstPitch > 0 && layout.key (firstPitch).upper)
        --firstPitch;
    while (layout.key (lastPitch).upper)
        ++lastPitch;

    first = firstPitch;
    last = juce::jmax (firstPitch, lastPitch);
    repaint();
}

void PianoView::setSnapshot (const Snapshot& s)
{
    snapshot = s;
    repaint();
}

double PianoView::firstX() const
{
    return layout.xOfPitch (first);
}

double PianoView::unitWidth() const
{
    const auto k = layout.keyForPitch (last);
    const double span = k.x + k.width - firstX();
    return span > 0.0 ? getWidth() / span : 1.0;
}

juce::Rectangle<float> PianoView::keyBounds (int pitch) const
{
    const auto k = layout.keyForPitch (pitch);
    const auto unit = unitWidth();
    const auto x = static_cast<float> ((k.x - firstX()) * unit);
    const auto w = static_cast<float> (k.width * unit);
    const auto h = static_cast<float> (getHeight());
    return { x, 0.0f, w, k.upper ? h * upperHeight : h };
}

void PianoView::paint (juce::Graphics& g)
{
    auto drawKey = [&] (int pitch, bool upper)
    {
        const auto r = keyBounds (pitch);
        const bool isManualNote = snapshot.isManual (pitch);
        const bool isHeldNote = snapshot.isHeld (pitch);
        const bool active = isManualNote || isHeldNote;

        juce::Colour fill = upper ? juce::Colour (0xff23262e) : juce::Colour (0xffe9ecf1);
        if (active)
            fill = activeColour (snapshot, pitch);
        else if (snapshot.isChordClass (pitch))
            fill = fill.interpolatedWith (accent, upper ? 0.35f : 0.22f);

        const auto body = upper ? r.reduced (0.5f, 0.0f) : r.reduced (0.6f, 0.0f);
        g.setColour (fill);
        g.fillRoundedRectangle (body.withTrimmedTop (-4.0f), 3.0f);

        if (! upper)
        {
            g.setColour (dim);
            g.drawRect (r.withTrimmedRight (-0.5f), 0.6f);
        }

        if (active && snapshot.isRoot (pitch))
        {
            g.setColour (root);
            g.fillRect (body.withTop (body.getBottom() - 4.0f));
        }

        // Labels: active notes get their name, C's get an octave marker.
        const float labelH = 16.0f;
        const auto labelArea = body.withTop (body.getBottom() - labelH - 5.0f).withHeight (labelH);
        const float fontSize = juce::jlimit (8.0f, 12.0f, body.getWidth() * 0.55f);
        if (active && snapshot.nameOf)
        {
            g.setColour (background);
            g.setFont (juce::FontOptions (fontSize, juce::Font::bold));
            g.drawFittedText (snapshot.nameOf (pitch), labelArea.toNearestInt(), juce::Justification::centred, 1, 0.6f);
        }
        else if (! upper && snapshot.classOf (pitch) == 0)
        {
            g.setColour (juce::Colours::grey);
            g.setFont (juce::FontOptions (fontSize));
            const int octave = pitch / snapshot.stepsPerOctave - 1;
            g.drawText ("C" + juce::String (octave), labelArea, juce::Justification::centred);
        }
    };

    g.fillAll (background);
    for (int p = first; p <= last; ++p)
        if (! layout.key (p).upper)
            drawKey (p, false);
    for (int p = first; p <= last; ++p)
        if (layout.key (p).upper)
            drawKey (p, true);
}

void PianoView::mouseDown (const juce::MouseEvent& e)
{
    const auto unit = unitWidth();
    const double x = e.position.x / unit + firstX();
    const double y = e.position.y / juce::jmax (1, getHeight());
    const int pitch = layout.pitchAt (x, y, upperHeight, first, last);
    if (pitch >= 0 && onKeyClicked)
        onKeyClicked (pitch);
}
