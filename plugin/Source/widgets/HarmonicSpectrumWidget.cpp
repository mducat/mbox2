#include "HarmonicSpectrumWidget.h"

#include "../PluginProcessor.h"

#include <cmath>

namespace mbox
{

using spectrum::Source;
using spectrum::SpectrumEngine;

namespace
{
    constexpr float kMinDb = -96.0f;
    constexpr float kMaxDb = 0.0f;

    const juce::Colour kGrid   { 0xff2c313c };
    const juce::Colour kShared { 0xffffd54f };

    /** Non-ASCII UI text is written with UTF-8 escapes so the source compiles the same on every compiler. */
    juce::String utf8 (const char* text) { return juce::String::fromUTF8 (text); }

    float normalise (float db) noexcept
    {
        return juce::jlimit (0.0f, 1.0f, (db - kMinDb) / (kMaxDb - kMinDb));
    }

    /** Colour of the n-th held note (stable hues, accent first). */
    juce::Colour sourceColour (int index)
    {
        static const juce::Colour palette[] = {
            juce::Colour (0xff4fc3f7), juce::Colour (0xff81c784), juce::Colour (0xffff8a65),
            juce::Colour (0xffba68c8), juce::Colour (0xfff06292), juce::Colour (0xff4db6ac),
        };
        return palette[static_cast<std::size_t> (index) % std::size (palette)];
    }
} // namespace

HarmonicSpectrumWidget::HarmonicSpectrumWidget (AppState& s) : Widget (s)
{
    // Colour map: black -> indigo -> magenta -> orange -> pale yellow
    juce::ColourGradient gradient (juce::Colour (0xff000004), 0.0f, 0.0f, juce::Colour (0xfffcfdbf), 1.0f, 0.0f, false);
    gradient.addColour (0.25, juce::Colour (0xff3b0f70));
    gradient.addColour (0.50, juce::Colour (0xff8c2981));
    gradient.addColour (0.75, juce::Colour (0xfffe9f6d));
    for (std::size_t i = 0; i < colourMap.size(); ++i)
        colourMap[i] = gradient.getColourAtPosition (static_cast<double> (i) / (colourMap.size() - 1)).getPixelARGB();

    sourceBox.addItem (utf8 ("Synth\xc3\xa9 interne"), 1 + static_cast<int> (Source::internalSynth));
    sourceBox.addItem (utf8 ("Entr\xc3\xa9" "e sidechain"), 1 + static_cast<int> (Source::sidechain));
    sourceBox.setTooltip (utf8 ("Signal analys\xc3\xa9 : le synth\xc3\xa9 additif interne (joue les notes actives) "
                                "ou l'audio envoy\xc3\xa9 par la DAW sur l'entr\xc3\xa9" "e sidechain du plugin."));
    sourceBox.onChange = [this]
    {
        if (engine != nullptr)
            engine->source.store (static_cast<Source> (sourceBox.getSelectedId() - 1));
    };
    addAndMakeVisible (sourceBox);

    audibleButton.setTooltip (utf8 ("Envoie le synth\xc3\xa9 interne sur la sortie audio du plugin."));
    audibleButton.onClick = [this]
    {
        if (engine != nullptr)
            engine->synthAudible.store (audibleButton.getToggleState());
    };
    addAndMakeVisible (audibleButton);

    partialsLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (partialsLabel);

    partialsSlider.setRange (1.0, spectrum::AdditiveSynth::kMaxPartials, 1.0);
    partialsSlider.setValue (12.0, juce::dontSendNotification);
    partialsSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 32, 20);
    partialsSlider.setTooltip (utf8 ("Nombre de partiels harmoniques du synth\xc3\xa9 interne et de la superposition th\xc3\xa9orique."));
    partialsSlider.onValueChange = [this]
    {
        if (engine != nullptr)
            engine->getSynth().settings().numPartials.store (static_cast<int> (partialsSlider.getValue()));
        theoryDirty = true;
    };
    addAndMakeVisible (partialsSlider);

    startTimerHz (30);
}

void HarmonicSpectrumWidget::parentHierarchyChanged()
{
    attachToEngine();
}

void HarmonicSpectrumWidget::attachToEngine()
{
    if (engine != nullptr)
        return;

    // The engine lives in the processor; reach it through the editor hosting this widget.
    if (auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>())
        if (auto* processor = dynamic_cast<MusicBox2Processor*> (editor->getAudioProcessor()))
            engine = &processor->getSpectrumEngine();

    if (engine == nullptr)
        return;

    sourceBox.setSelectedId (1 + static_cast<int> (engine->source.load()), juce::dontSendNotification);
    audibleButton.setToggleState (engine->synthAudible.load(), juce::dontSendNotification);
    partialsSlider.setValue (engine->getSynth().settings().numPartials.load(), juce::dontSendNotification);
    theoryDirty = true;
    rebuildRowMapping();
}

void HarmonicSpectrumWidget::appStateChanged (AppState&, int changes)
{
    if ((changes & (AppState::tuningChanged | AppState::activeNotesChanged)) != 0)
        theoryDirty = true;
}

juce::String HarmonicSpectrumWidget::noteLabel (double hz, bool withCents) const
{
    // Nearest note of the current tuning ("A4", "C#3 +14c", N-TET names...).
    const auto& tuning = state.getTuning();
    double cents = 0.0;
    const auto note = tuning.nearestNote (hz, &cents);
    auto label = utf8 (tuning.noteName (note).c_str());

    const auto rounded = static_cast<int> (std::lround (cents));
    if (withCents && rounded != 0)
        label << (rounded > 0 ? " +" : " ") << rounded << "c";
    return label;
}

//==============================================================================
void HarmonicSpectrumWidget::resized()
{
    auto area = getLocalBounds().reduced (8);

    auto bar = area.removeFromTop (26);
    area.removeFromTop (6);
    sourceBox.setBounds (bar.removeFromLeft (150));
    bar.removeFromLeft (8);
    audibleButton.setBounds (bar.removeFromLeft (80));
    partialsSlider.setBounds (bar.removeFromRight (130));
    partialsLabel.setBounds (bar.removeFromRight (60));

    infoArea = area.removeFromBottom (40);
    area.removeFromBottom (4);

    axisArea      = area.removeFromLeft (44);
    spectrumArea  = area.removeFromRight (juce::jmax (120, area.getWidth() / 3));
    area.removeFromRight (4);
    waterfallArea = area;

    if (waterfallArea.getWidth() > 0 && waterfallArea.getHeight() > 0)
    {
        waterfall = juce::Image (juce::Image::RGB, waterfallArea.getWidth(), waterfallArea.getHeight(), true);
        waterfall.clear (waterfall.getBounds(), juce::Colour (colourMap.front()));
    }
    rebuildRowMapping();
}

void HarmonicSpectrumWidget::rebuildRowMapping()
{
    if (engine == nullptr)
        return;

    mappedSampleRate = engine->getSampleRate();
    maxFrequency     = juce::jmin (12000.0, 0.45 * mappedSampleRate);

    const auto h       = waterfallArea.getHeight();
    const auto binWidth = engine->getBinWidth();
    rowBinLow.assign (static_cast<std::size_t> (juce::jmax (0, h)), 0.0f);
    rowBinHigh.assign (rowBinLow.size(), 0.0f);

    const auto freqAt = [&] (double y) { return minFrequency * std::pow (maxFrequency / minFrequency, 1.0 - y / h); };
    for (int r = 0; r < h; ++r)
    {
        rowBinHigh[static_cast<std::size_t> (r)] = static_cast<float> (freqAt (r) / binWidth);
        rowBinLow[static_cast<std::size_t> (r)]  = static_cast<float> (freqAt (r + 1) / binWidth);
    }
}

float HarmonicSpectrumWidget::yForFrequency (double hz, juce::Rectangle<float> area) const noexcept
{
    const auto t = std::log (hz / minFrequency) / std::log (maxFrequency / minFrequency);
    return area.getBottom() - static_cast<float> (t) * area.getHeight();
}

float HarmonicSpectrumWidget::levelForRow (const std::vector<float>& mags, int row) const noexcept
{
    if (mags.empty() || row < 0 || row >= static_cast<int> (rowBinLow.size()))
        return kMinDb;

    const auto lo   = rowBinLow[static_cast<std::size_t> (row)];
    const auto hi   = rowBinHigh[static_cast<std::size_t> (row)];
    const auto last = static_cast<int> (mags.size()) - 1;

    if (hi - lo < 1.0f)
    {
        // Low frequencies: several rows per bin, interpolate.
        const auto pos  = 0.5f * (lo + hi);
        const auto i0   = juce::jlimit (0, last, static_cast<int> (pos));
        const auto i1   = juce::jmin (last, i0 + 1);
        const auto frac = pos - static_cast<float> (i0);
        return mags[static_cast<std::size_t> (i0)] * (1.0f - frac) + mags[static_cast<std::size_t> (i1)] * frac;
    }

    // High frequencies: several bins per row, keep the strongest.
    auto level = kMinDb;
    for (int k = juce::jlimit (0, last, static_cast<int> (lo)); k <= juce::jmin (last, static_cast<int> (std::ceil (hi))); ++k)
        level = juce::jmax (level, mags[static_cast<std::size_t> (k)]);
    return level;
}

//==============================================================================
void HarmonicSpectrumWidget::timerCallback()
{
    attachToEngine();
    if (engine == nullptr)
        return;

    if (! juce::exactlyEqual (engine->getSampleRate(), mappedSampleRate))
        rebuildRowMapping();

    bool gotFrame = false;
    while (engine->pullFrame (frame))
    {
        pushColumn (frame);
        gotFrame = true;
    }

    if (gotFrame)
    {
        if (smoothedFrame.size() != frame.size())
            smoothedFrame = frame;
        for (std::size_t k = 0; k < frame.size(); ++k)
            smoothedFrame[k] = juce::jmax (frame[k], smoothedFrame[k] - 3.0f); // fast attack, slow fall

        peaks = analysis::findPeaks (frame, engine->getBinWidth(), -60.0f, 12);
    }

    updateTheory();
    if (isShowing())
        repaint();
}

void HarmonicSpectrumWidget::pushColumn (const std::vector<float>& mags)
{
    if (! waterfall.isValid())
        return;

    const auto w = waterfall.getWidth();
    const auto h = waterfall.getHeight();
    waterfall.moveImageSection (0, 0, 1, 0, w - 1, h);

    juce::Image::BitmapData pixels (waterfall, w - 1, 0, 1, h, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < h; ++y)
    {
        const auto index = static_cast<std::size_t> (normalise (levelForRow (mags, y)) * (colourMap.size() - 1));
        pixels.setPixelColour (0, y, juce::Colour (colourMap[index]));
    }
}

void HarmonicSpectrumWidget::updateTheory()
{
    const auto nPartial = static_cast<int> (partialsSlider.getValue());
    if (! theoryDirty && nPartial == lastNumPartials)
        return;

    theoryDirty     = false;
    lastNumPartials = nPartial;

    const auto& tuning = state.getTuning();
    fundamentals.clear();
    for (const auto note : state.getActiveNotes())
        fundamentals.push_back (tuning.frequencyOf (note));

    partials       = analysis::harmonicPartials (fundamentals, nPartial, 1.0, maxFrequency);
    coincidences   = analysis::findCoincidences (partials, 10.0);
    chordRoughness = analysis::roughness (partials);
    virtualRoot    = fundamentals.size() >= 2 ? analysis::virtualFundamental (fundamentals, 20.0, 32, 16.0)
                                              : std::optional<double> {};
}

//==============================================================================
void HarmonicSpectrumWidget::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);

    if (engine == nullptr)
    {
        g.setColour (colours::textDim);
        g.drawText ("Spectrum engine unavailable", getLocalBounds(), juce::Justification::centred);
        return;
    }

    if (waterfall.isValid())
        g.drawImageAt (waterfall, waterfallArea.getX(), waterfallArea.getY());

    const auto wf = waterfallArea.toFloat();

    // Faint guide lines for the held notes' partials across the spectrogram.
    for (const auto& p : partials)
    {
        const auto y = yForFrequency (p.frequency, wf);
        g.setColour (sourceColour (p.source).withAlpha (p.harmonic == 1 ? 0.55f : 0.18f));
        g.drawHorizontalLine (juce::roundToInt (y), wf.getX(), wf.getRight());
    }

    paintFrequencyAxis (g, axisArea.toFloat());
    paintSpectrum (g, spectrumArea.toFloat());
    paintInfo (g, infoArea);
}

void HarmonicSpectrumWidget::paintFrequencyAxis (juce::Graphics& g, juce::Rectangle<float> area)
{
    // One tick per period (octave) of the current tuning, on C.
    const auto& tuning = state.getTuning();
    g.setFont (juce::FontOptions (11.0f));
    for (int octave = -4; octave <= 5; ++octave)
    {
        const auto note = tuning.middleC() + octave * tuning.divisions();
        const auto hz   = tuning.frequencyOf (note);
        if (hz < minFrequency || hz > maxFrequency)
            continue;

        const auto y = yForFrequency (hz, area);
        g.setColour (kGrid);
        g.drawHorizontalLine (juce::roundToInt (y), area.getRight() - 4.0f, static_cast<float> (spectrumArea.getRight()));
        g.setColour (colours::text);
        g.drawText (utf8 (tuning.noteName (note).c_str()), juce::Rectangle<float> (area.getX(), y - 7.0f, area.getWidth() - 6.0f, 14.0f),
                    juce::Justification::centredRight);
    }
}

void HarmonicSpectrumWidget::paintSpectrum (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (colours::background);
    g.fillRect (area);

    // dB grid
    g.setColour (kGrid);
    for (float db = kMinDb; db <= kMaxDb; db += 24.0f)
        g.drawVerticalLine (juce::roundToInt (area.getX() + normalise (db) * area.getWidth()), area.getY(), area.getBottom());

    // Measured spectrum
    if (! smoothedFrame.empty())
    {
        juce::Path curve;
        const auto h = juce::jmin (static_cast<int> (area.getHeight()), static_cast<int> (rowBinLow.size()));
        for (int y = 0; y < h; ++y)
        {
            const auto x = area.getX() + normalise (levelForRow (smoothedFrame, y)) * area.getWidth();
            const auto py = area.getY() + static_cast<float> (y);
            if (y == 0) curve.startNewSubPath (x, py);
            else        curve.lineTo (x, py);
        }

        auto fill = curve;
        fill.lineTo (area.getX(), area.getY() + static_cast<float> (h));
        fill.lineTo (area.getX(), area.getY());
        fill.closeSubPath();
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillPath (fill);
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.strokePath (curve, juce::PathStrokeType (1.2f));
    }

    // Ideal partials of the held notes: tick length ~ amplitude.
    for (const auto& p : partials)
    {
        const auto y   = yForFrequency (p.frequency, area);
        const auto len = juce::jmax (6.0f, static_cast<float> (p.amplitude) * area.getWidth() * 0.35f);
        g.setColour (sourceColour (p.source).withAlpha (p.harmonic == 1 ? 1.0f : 0.75f));
        g.fillRect (juce::Rectangle<float> (area.getRight() - len, y - (p.harmonic == 1 ? 1.5f : 0.75f),
                                            len, p.harmonic == 1 ? 3.0f : 1.5f));
    }

    // Partials shared by several notes (what makes intervals sound consonant).
    g.setColour (kShared);
    for (const auto& c : coincidences)
    {
        const auto y = yForFrequency (0.5 * (c.a.frequency + c.b.frequency), area);
        g.drawEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ area.getRight() - 6.0f, y }), 1.5f);
    }

    // Fundamental labels
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    for (std::size_t i = 0; i < fundamentals.size(); ++i)
    {
        const auto y = yForFrequency (fundamentals[i], area);
        g.setColour (sourceColour (static_cast<int> (i)));
        g.drawText (noteLabel (fundamentals[i], true), juce::Rectangle<float> (area.getX() + 4.0f, y - 13.0f, 70.0f, 12.0f),
                    juce::Justification::centredLeft);
    }

    // Detected peaks
    g.setColour (juce::Colours::white);
    for (const auto& peak : peaks)
    {
        if (peak.frequency < minFrequency || peak.frequency > maxFrequency)
            continue;
        const auto x = area.getX() + normalise (static_cast<float> (peak.magnitude)) * area.getWidth();
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ x, yForFrequency (peak.frequency, area) }));
    }

    g.setColour (kGrid);
    g.drawRect (area);
}

void HarmonicSpectrumWidget::paintInfo (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (colours::text);
    g.setFont (juce::FontOptions (13.0f));

    if (fundamentals.empty())
    {
        g.drawText (engine != nullptr && engine->source.load() == Source::sidechain
                        ? (utf8 ("Analyse de l'entr\xc3\xa9" "e sidechain. Jouez ou cliquez des notes pour superposer leurs partiels th\xc3\xa9oriques."))
                        : (utf8 ("Jouez des notes : le synth\xc3\xa9 interne les fait sonner avec leurs partiels harmoniques.")),
                    area, juce::Justification::centredLeft);
        return;
    }

    juce::String root = "-";
    if (virtualRoot)
        root = noteLabel (*virtualRoot, true) + " (" + juce::String (*virtualRoot, 1) + " Hz)";

    auto top = area.removeFromTop (area.getHeight() / 2);
    g.drawText ("Fondamentale virtuelle : " + root, top, juce::Justification::centredLeft);
    g.drawText (utf8 ("Rugosit\xc3\xa9 : ") + juce::String (chordRoughness, 2)
                    + "   |   Partiels communs : " + juce::String (static_cast<int> (coincidences.size())),
                area, juce::Justification::centredLeft);
}

} // namespace mbox
