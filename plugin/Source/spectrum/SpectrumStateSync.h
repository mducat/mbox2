#pragma once

#include "SpectrumEngine.h"

#include "../state/AppState.h"

namespace mbox::spectrum
{

/**
    Keeps the spectrum engine in line with the shared AppState (message thread):
      - the synth's MIDI note -> Hz table follows the current tuning (a MIDI key
        sounds the closest note of the tuning, like AppState::setHeldMidiNotes);
      - notes toggled by hand in widgets are sustained by the synth, so they
        show up in the spectrogram too.
*/
class SpectrumStateSync final : private AppState::Listener
{
public:
    SpectrumStateSync (AppState& state, SpectrumEngine& engine);
    ~SpectrumStateSync() override;

private:
    void appStateChanged (AppState&, int changes) override;
    void updateTuning();
    void updateManualNotes();

    AppState& state;
    SpectrumEngine& engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumStateSync)
};

} // namespace mbox::spectrum
