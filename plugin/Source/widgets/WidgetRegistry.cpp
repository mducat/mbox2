#include "WidgetRegistry.h"

#include "CircleOfFifthsWidget.h"
#include "InstrumentWidget.h"
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
        make<InstrumentWidget> ("instruments", "Piano / guitare"),
    };
    return list;
}

} // namespace mbox
