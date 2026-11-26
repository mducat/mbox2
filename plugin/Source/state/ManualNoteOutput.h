#pragma once

#include "AppState.h"

#include <functional>
#include <vector>

namespace mbox
{

/**
    Plays the notes switched on by hand in the widgets (AppState manual notes)
    on the plugin's MIDI output: watches the state and emits note-on/off for
    every change. Owned by the processor; message thread only.

    Only 12-TET notes are sent for now (N-TET would need pitch bend / MPE).
*/
class ManualNoteOutput final : private AppState::Listener
{
public:
    using Sender = std::function<void (int midiNote, bool on)>;

    ManualNoteOutput (AppState& state, Sender sender);
    ~ManualNoteOutput() override;

    /** When disabled, every note sent so far is released. */
    void setEnabled (bool shouldBeEnabled);
    bool isEnabled() const noexcept { return enabled; }

    /** Releases every note sent so far (keeps the enabled flag). */
    void releaseAll();

private:
    void appStateChanged (AppState&, int changes) override;
    void sync();

    AppState& state;
    Sender send;
    bool enabled = true;
    std::vector<int> sounding; // MIDI notes currently on, ascending

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ManualNoteOutput)
};

} // namespace mbox
