#pragma once

#include "../state/AppState.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace mbox
{

/**
    Base class of every editor widget (a tab of the main window).

    A widget is a plain juce::Component that receives the shared AppState and
    is notified when it changes. By default a change repaints the widget;
    override appStateChanged() to react more finely (check the `changes` bits).

    To add a widget:
      1. create plugin/Source/widgets/MyWidget.{h,cpp} deriving from mbox::Widget
         (new .cpp files are picked up by CMake automatically);
      2. add one line to the list in WidgetRegistry.cpp.
*/
class Widget : public juce::Component,
               protected AppState::Listener
{
public:
    explicit Widget (AppState& s) : state (s) { state.addListener (this); }
    ~Widget() override { state.removeListener (this); }

protected:
    void appStateChanged (AppState&, int /*changes*/) override { repaint(); }

    AppState& state;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Widget)
};

/** Shared colours so widgets look alike. */
namespace colours
{
    inline const juce::Colour background { 0xff15171c };
    inline const juce::Colour panel      { 0xff1d2027 };
    inline const juce::Colour accent     { 0xff4fc3f7 };
    inline const juce::Colour secondary  { 0xffffb74d };
    inline const juce::Colour dim        { 0xff3a3f4b };
    inline const juce::Colour text       { 0xffe6e6e6 };
    inline const juce::Colour textDim    { 0xff8a8f99 };
} // namespace colours

} // namespace mbox
