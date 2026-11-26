#include "SpectrumStateSync.h"

#include <algorithm>
#include <array>

namespace mbox::spectrum
{

SpectrumStateSync::SpectrumStateSync (AppState& s, SpectrumEngine& e) : state (s), engine (e)
{
    state.addListener (this);
    updateTuning();
    updateManualNotes();
}

SpectrumStateSync::~SpectrumStateSync()
{
    state.removeListener (this);
}

void SpectrumStateSync::appStateChanged (AppState&, int changes)
{
    if ((changes & AppState::tuningChanged) != 0)
        updateTuning();

    if ((changes & (AppState::tuningChanged | AppState::activeNotesChanged)) != 0)
        updateManualNotes();
}

void SpectrumStateSync::updateTuning()
{
    const theory::Tuning midiTuning; // 12-TET: what MIDI keyboards send
    const auto& tuning = state.getTuning();

    std::array<double, 128> table {};
    for (int m = 0; m < 128; ++m)
        table[static_cast<std::size_t> (m)] = tuning.frequencyOf (tuning.convertNote (m, midiTuning));

    engine.getSynth().setNoteFrequencies (table);
}

void SpectrumStateSync::updateManualNotes()
{
    const auto& tuning = state.getTuning();
    const auto& held   = state.getHeldNotes();

    std::vector<double> hz;
    for (const auto note : state.getManualNotes())
        if (std::find (held.begin(), held.end(), note) == held.end()) // already sounding through MIDI
            hz.push_back (tuning.frequencyOf (note));

    engine.getSynth().setSustainedFrequencies (hz);
}

} // namespace mbox::spectrum
