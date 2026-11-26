#pragma once

#include "ProgressionStrip.h"
#include "SoundSupport.h"
#include "TonalMapComponent.h"
#include "Widget.h"

namespace mbox
{

/**
    "Carte tonale" tab: the tonal function map of the current key (Tonic /
    Subdominant / Dominant columns) above the shared progression.

    - The key follows the editor header (AppState key). Scales whose third is
      minor (minor, dorian, phrygian...) are shown as the minor key.
    - Clicking a chord appends it to the AppState progression with its Roman
      numeral ("V6/5/V") and a close voicing in the current tuning.
    - Chords already in the progression are highlighted on the map.
    - A chosen chord is also played (piano or guitar, "Son" selector).
*/
class TonalMapWidget final : public Widget
{
public:
    explicit TonalMapWidget (AppState&);

    void resized() override;
    void paint (juce::Graphics&) override;

    /** Major / minor key closest to an AppState key (any scale type, any tuning). */
    static theory::Key keyFromScale (const theory::Scale&);

private:
    void appStateChanged (AppState&, int changes) override;
    void syncKey();
    void syncSelection();

    /** Map chord matching a progression chord (same chord and numeral in the current key), if any. */
    std::optional<theory::RomanChord> findInKey (const ProgressionChord&) const;

    TonalMapComponent map;
    ProgressionStrip strip;
    SoundSelector soundBox;
    std::vector<theory::RomanChord> allChords; // every chord of the key, all inversions, for lookups

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TonalMapWidget)
};

} // namespace mbox
