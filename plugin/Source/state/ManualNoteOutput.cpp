#include "ManualNoteOutput.h"

#include <algorithm>
#include <iterator>

namespace mbox
{

ManualNoteOutput::ManualNoteOutput (AppState& s, Sender sender)
    : state (s), send (std::move (sender))
{
    state.addListener (this);
    sync();
}

ManualNoteOutput::~ManualNoteOutput()
{
    state.removeListener (this);
}

void ManualNoteOutput::setEnabled (bool shouldBeEnabled)
{
    if (enabled == shouldBeEnabled)
        return;
    enabled = shouldBeEnabled;
    sync();
}

void ManualNoteOutput::releaseAll()
{
    for (const int n : sounding)
        send (n, false);
    sounding.clear();
}

void ManualNoteOutput::appStateChanged (AppState&, int changes)
{
    if ((changes & (AppState::activeNotesChanged | AppState::tuningChanged)) != 0)
        sync();
}

void ManualNoteOutput::sync()
{
    std::vector<int> target;
    if (enabled && state.getTuning().isTwelveTet())
        for (const int n : state.getManualNotes())
            if (n >= 0 && n <= 127)
                target.push_back (n);

    std::vector<int> off, on;
    std::set_difference (sounding.begin(), sounding.end(), target.begin(), target.end(), std::back_inserter (off));
    std::set_difference (target.begin(), target.end(), sounding.begin(), sounding.end(), std::back_inserter (on));

    for (const int n : off) send (n, false);
    for (const int n : on)  send (n, true);
    sounding = std::move (target);
}

} // namespace mbox
