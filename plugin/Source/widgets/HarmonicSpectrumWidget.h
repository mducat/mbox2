#pragma once

#include "Widget.h"
#include "../spectrum/SpectrumEngine.h"

#include <mbox/analysis/Harmonics.h>

#include <array>
#include <optional>
#include <vector>

namespace mbox
{

/**
    Harmonic decomposition widget.

    - Left: scrolling spectrogram (time -> x, log frequency -> y).
    - Right: current spectrum, with the ideal harmonic partials of the held
      notes overlaid (fundamentals, harmonics, and partials shared by several
      notes highlighted) and the detected spectral peaks.
    - Bottom: virtual fundamental, roughness (sensory dissonance) and number
      of shared partials of the active chord.

    The analysed signal comes from the processor's SpectrumEngine: either its
    built-in additive synth (playing the active notes) or the sidechain input.
    Everything is computed in Hz from AppState's tuning, so it works the same
    in any N-TET or table tuning.
*/
class HarmonicSpectrumWidget final : public Widget,
                                     private juce::Timer
{
public:
    explicit HarmonicSpectrumWidget (AppState&);
    ~HarmonicSpectrumWidget() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;

private:
    void appStateChanged (AppState&, int changes) override;
    void timerCallback() override;
    void attachToEngine();
    juce::String noteLabel (double hz, bool withCents) const;
    void pushColumn (const std::vector<float>& magnitudesDb);
    void updateTheory();
    void rebuildRowMapping();

    float yForFrequency (double hz, juce::Rectangle<float> area) const noexcept;
    float levelForRow (const std::vector<float>& magnitudesDb, int row) const noexcept;

    void paintFrequencyAxis (juce::Graphics&, juce::Rectangle<float> area);
    void paintSpectrum (juce::Graphics&, juce::Rectangle<float> area);
    void paintInfo (juce::Graphics&, juce::Rectangle<int> area);

    spectrum::SpectrumEngine* engine = nullptr; // owned by the processor, found through the editor

    juce::ComboBox sourceBox;
    juce::ToggleButton audibleButton { "Audible" };
    juce::Label partialsLabel { {}, "Partiels" };
    juce::Slider partialsSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::TooltipWindow tooltips { this, 600 };

    // Layout
    juce::Rectangle<int> axisArea, waterfallArea, spectrumArea, infoArea;

    // Spectrogram image and frequency mapping
    juce::Image waterfall;
    std::vector<float> rowBinLow, rowBinHigh; // fractional FFT bins covered by each image row
    std::vector<float> frame, smoothedFrame;
    std::array<juce::PixelARGB, 256> colourMap {};
    double minFrequency = 30.0, maxFrequency = 12000.0;
    double mappedSampleRate = 0.0;

    // Theory overlay for the active notes
    bool theoryDirty = true;
    int lastNumPartials = -1;
    std::vector<double> fundamentals;
    std::vector<mbox::analysis::Partial> partials;
    std::vector<mbox::analysis::Coincidence> coincidences;
    std::optional<double> virtualRoot;
    double chordRoughness = 0.0;
    std::vector<mbox::analysis::SpectralPeak> peaks;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonicSpectrumWidget)
};

} // namespace mbox
