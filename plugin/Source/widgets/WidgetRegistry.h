#pragma once

#include "Widget.h"

#include <functional>
#include <memory>
#include <vector>

namespace mbox
{

struct WidgetInfo
{
    juce::String id;     // stable identifier, saved with the editor layout ("circle-of-fifths")
    juce::String title;  // tab title, in French ("Cercle des quintes")
    std::function<std::unique_ptr<Widget> (AppState&)> create;
};

/** Every widget of the editor, in tab order. */
const std::vector<WidgetInfo>& allWidgets();

} // namespace mbox
