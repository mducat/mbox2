#pragma once

#include "../state/AppState.h"

#include <mbox/sound/Instruments.h>

#include <juce_gui_basics/juce_gui_basics.h>

#include <optional>
#include <vector>

class MusicBox2Processor;

namespace mbox
{

/** Choice of the shared "Son" selectors (ComboBox ids). */
enum SoundChoice
{
    soundPiano  = 1,
    soundGuitar = 2,
    soundMuted  = 3
};

/** The plugin processor owning `c`'s editor, or nullptr while `c` is not on screen. */
MusicBox2Processor* findProcessor (const juce::Component& c);

/**
    "Son" combo box: Piano / Guitare / Muet. Every selector of the editor is
    bound to the same processor value, so changing one changes them all (and
    the choice is saved with the project).
*/
class SoundSelector final : public juce::ComboBox
{
public:
    SoundSelector();
    void parentHierarchyChanged() override;

    /** Instrument picked by the shared choice, nullopt when muted. */
    static std::optional<sound::Instrument> chosenInstrument (const juce::Component& anyWidget);

private:
    bool bound = false;
};

/**
    Plays notes of the current tuning (note numbers, as in AppState) through
    the processor's piano / guitar voices. `instrument` nullopt = the shared
    choice; nothing happens when the sound is muted or the widget is not on
    screen. Several notes make a chord (strummed on the guitar).
*/
void playNotes (const juce::Component& from, const AppState& state, std::vector<int> notes,
                std::optional<sound::Instrument> instrument = std::nullopt,
                float velocity = 0.8f, double delaySeconds = 0.0, double lengthSeconds = 0.0);

/** Cancels everything scheduled from the widgets (e.g. "Stop" of the composition). */
void stopAllNotes (const juce::Component& from);

} // namespace mbox
