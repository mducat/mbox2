#include "WidgetRegistry.h"

#include "CircleOfFifthsWidget.h"
#include "CompositionWidget.h"
#include "HarmonicSpectrumWidget.h"
#include "InstrumentWidget.h"
#include "TonalMapWidget.h"
#include "TriadSpaceWidget.h"
// One #include per widget, keep alphabetical to ease merges.

namespace mbox
{

namespace
{
    template <typename W>
    WidgetInfo make (const char* id, const char* title)
    {
        return { id, juce::String::fromUTF8 (title), [] (AppState& s) { return std::make_unique<W> (s); } };
    }
} // namespace

const std::vector<WidgetInfo>& allWidgets()
{
    // One line per widget, in tab order.
    static const std::vector<WidgetInfo> list {
        make<CircleOfFifthsWidget> ("circle-of-fifths", "Cercle des quintes"),
        make<TonalMapWidget>       ("tonal-map",        "Carte tonale"),
        make<CompositionWidget>    ("composition", "Composition"),
        make<InstrumentWidget> ("instruments", "Piano / guitare"),
        make<TriadSpaceWidget>     ("triad-space",      "Espace des accords 3D"),
        make<HarmonicSpectrumWidget> ("harmonic-spectrum", "Spectre harmonique"),
    };
    return list;
}

} // namespace mbox
