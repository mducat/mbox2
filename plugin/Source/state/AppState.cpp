#include "AppState.h"

#include <algorithm>

namespace mbox
{

namespace
{
    namespace ids
    {
        const juce::Identifier state { "AppState" };
        const juce::Identifier tuning { "Tuning" };
        const juce::Identifier name { "name" };
        const juce::Identifier equal { "equal" };
        const juce::Identifier divisions { "divisions" };
        const juce::Identifier period { "period" };
        const juce::Identifier cents { "cents" };
        const juce::Identifier concertA { "concertA" };
        const juce::Identifier key { "Key" };
        const juce::Identifier tonic { "tonic" };
        const juce::Identifier scale { "scale" };
        const juce::Identifier progression { "Progression" };
        const juce::Identifier selected { "selected" };
        const juce::Identifier chord { "Chord" };
        const juce::Identifier root { "root" };
        const juce::Identifier quality { "quality" };
        const juce::Identifier notes { "notes" };
        const juce::Identifier degree { "degree" };
        const juce::Identifier symbol { "symbol" };
        const juce::Identifier manualNotes { "manualNotes" };
    } // namespace ids

    void sortUnique (std::vector<int>& v)
    {
        std::sort (v.begin(), v.end());
        v.erase (std::unique (v.begin(), v.end()), v.end());
    }

    juce::String intsToString (const std::vector<int>& v)
    {
        juce::StringArray s;
        for (const int i : v)
            s.add (juce::String (i));
        return s.joinIntoString (" ");
    }

    std::vector<int> intsFromString (const juce::String& text)
    {
        std::vector<int> v;
        for (const auto& t : juce::StringArray::fromTokens (text, " ", ""))
            if (t.isNotEmpty())
                v.push_back (t.getIntValue());
        return v;
    }

    const theory::ScaleType* findScale (const juce::String& name)
    {
        for (const auto* t : theory::scales::all())
            if (name == juce::String (std::string (t->name)))
                return t;
        return nullptr;
    }

    const theory::ChordQuality* findQuality (const juce::String& name)
    {
        for (const auto* q : theory::chords::all())
            if (name == juce::String (std::string (q->name)))
                return q;
        return nullptr;
    }
} // namespace

AppState::AppState()
    : key (theory::PitchClass (0), theory::scales::major)
{
}

void AppState::notify (int changes)
{
    JUCE_ASSERT_MESSAGE_THREAD
    listeners.call ([this, changes] (Listener& l) { l.appStateChanged (*this, changes); });
}

// ---------------------------------------------------------------------------
void AppState::setTuning (theory::Tuning newTuning)
{
    if (newTuning == tuning)
        return;

    const auto old = tuning;
    tuning = std::move (newTuning);
    const int n = tuning.divisions();

    key = theory::Scale (key.tonic().convertedTo (n), key.type());

    for (auto& p : progression)
    {
        p.chord = theory::Chord (p.chord.root().convertedTo (n), p.chord.quality());
        for (auto& note : p.notes)
            note = tuning.convertNote (note, old);
        sortUnique (p.notes);
    }

    for (auto& note : manualNotes)
        note = tuning.convertNote (note, old);
    sortUnique (manualNotes);

    for (auto& note : heldNotes)
        note = tuning.convertNote (note, old);
    sortUnique (heldNotes);

    notify (allChanged);
}

// ---------------------------------------------------------------------------
void AppState::setKey (theory::PitchClass tonic, const theory::ScaleType& type)
{
    const theory::Scale newKey (tonic.convertedTo (tuning.divisions()), type);
    if (newKey.tonic() == key.tonic() && &newKey.type() == &key.type())
        return;

    key = newKey;
    notify (keyChanged);
}

// ---------------------------------------------------------------------------
ProgressionChord AppState::normalised (ProgressionChord c) const
{
    const int n = tuning.divisions();
    if (c.chord.divisions() != n)
        c.chord = theory::Chord (c.chord.root().convertedTo (n), c.chord.quality());

    if (c.notes.empty())
    {
        // Root position, root in the octave starting at middle C.
        c.notes = c.chord.voicing (tuning.noteOf (c.chord.root(), 4));
    }
    sortUnique (c.notes);

    if (c.symbol.empty())
        c.symbol = c.chord.symbol();
    return c;
}

int AppState::appendChord (ProgressionChord chord)
{
    progression.push_back (normalised (std::move (chord)));
    notify (progressionChanged);
    return static_cast<int> (progression.size()) - 1;
}

void AppState::insertChord (int index, ProgressionChord chord)
{
    index = juce::jlimit (0, static_cast<int> (progression.size()), index);
    progression.insert (progression.begin() + index, normalised (std::move (chord)));

    int changes = progressionChanged;
    if (selectedIndex >= index)
    {
        ++selectedIndex;
        changes |= selectionChanged;
    }
    notify (changes);
}

void AppState::replaceChord (int index, ProgressionChord chord)
{
    if (! juce::isPositiveAndBelow (index, static_cast<int> (progression.size())))
        return;
    progression[static_cast<std::size_t> (index)] = normalised (std::move (chord));
    notify (progressionChanged);
}

void AppState::removeChord (int index)
{
    if (! juce::isPositiveAndBelow (index, static_cast<int> (progression.size())))
        return;
    progression.erase (progression.begin() + index);

    int changes = progressionChanged;
    if (selectedIndex == index)
    {
        selectedIndex = -1;
        changes |= selectionChanged;
    }
    else if (selectedIndex > index)
    {
        --selectedIndex;
        changes |= selectionChanged;
    }
    notify (changes);
}

void AppState::moveChord (int fromIndex, int toIndex)
{
    const int size = static_cast<int> (progression.size());
    if (! juce::isPositiveAndBelow (fromIndex, size) || ! juce::isPositiveAndBelow (toIndex, size) || fromIndex == toIndex)
        return;

    auto c = std::move (progression[static_cast<std::size_t> (fromIndex)]);
    progression.erase (progression.begin() + fromIndex);
    progression.insert (progression.begin() + toIndex, std::move (c));

    int changes = progressionChanged;
    if (selectedIndex == fromIndex)
    {
        selectedIndex = toIndex;
        changes |= selectionChanged;
    }
    else if (selectedIndex >= 0)
    {
        const int before = selectedIndex;
        if (fromIndex < selectedIndex && toIndex >= selectedIndex) --selectedIndex;
        else if (fromIndex > selectedIndex && toIndex <= selectedIndex) ++selectedIndex;
        if (before != selectedIndex)
            changes |= selectionChanged;
    }
    notify (changes);
}

void AppState::clearProgression()
{
    if (progression.empty())
        return;
    progression.clear();
    const bool hadSelection = selectedIndex >= 0;
    selectedIndex = -1;
    notify (progressionChanged | (hadSelection ? selectionChanged : 0));
}

void AppState::setSelectedChordIndex (int index)
{
    if (! juce::isPositiveAndBelow (index, static_cast<int> (progression.size())))
        index = -1;
    if (index == selectedIndex)
        return;
    selectedIndex = index;
    notify (selectionChanged);
}

const ProgressionChord* AppState::getSelectedChord() const noexcept
{
    return selectedIndex >= 0 ? &progression[static_cast<std::size_t> (selectedIndex)] : nullptr;
}

// ---------------------------------------------------------------------------
void AppState::setHeldMidiNotes (const std::vector<int>& midiNotes)
{
    static const theory::Tuning midiTuning; // MIDI keyboards are 12-TET

    std::vector<int> converted;
    converted.reserve (midiNotes.size());
    for (const int n : midiNotes)
        converted.push_back (tuning.convertNote (n, midiTuning));
    sortUnique (converted);

    if (converted == heldNotes)
        return;
    heldNotes = std::move (converted);
    notify (activeNotesChanged);
}

void AppState::setManualNotes (std::vector<int> notes)
{
    sortUnique (notes);
    if (notes == manualNotes)
        return;
    manualNotes = std::move (notes);
    notify (activeNotesChanged);
}

void AppState::toggleManualNote (int note)
{
    if (const auto it = std::lower_bound (manualNotes.begin(), manualNotes.end(), note);
        it != manualNotes.end() && *it == note)
        manualNotes.erase (it);
    else
        manualNotes.insert (it, note);
    notify (activeNotesChanged);
}

void AppState::clearManualNotes()
{
    setManualNotes ({});
}

std::vector<int> AppState::getActiveNotes() const
{
    std::vector<int> all;
    all.reserve (heldNotes.size() + manualNotes.size());
    std::set_union (heldNotes.begin(), heldNotes.end(), manualNotes.begin(), manualNotes.end(), std::back_inserter (all));
    return all;
}

theory::PitchClassSet AppState::getActivePitchClasses() const
{
    return theory::PitchClassSet::fromNotes (getActiveNotes(), tuning.divisions());
}

std::vector<theory::Chord> AppState::getRecognisedChords() const
{
    const auto notes = getActiveNotes();
    if (notes.empty())
        return {};
    return theory::identifyChords (theory::PitchClassSet::fromNotes (notes, tuning.divisions()),
                                   tuning.pitchClassOf (notes.front()));
}

std::string AppState::describeActiveChord() const
{
    const auto notes = getActiveNotes();
    if (notes.empty())
        return {};
    return theory::describeChord (theory::PitchClassSet::fromNotes (notes, tuning.divisions()),
                                  tuning.pitchClassOf (notes.front()));
}

// ---------------------------------------------------------------------------
juce::ValueTree AppState::toValueTree() const
{
    juce::ValueTree root (ids::state);

    juce::ValueTree t (ids::tuning);
    t.setProperty (ids::name, juce::String (tuning.name()), nullptr);
    t.setProperty (ids::equal, tuning.isEqual(), nullptr);
    t.setProperty (ids::divisions, tuning.divisions(), nullptr);
    t.setProperty (ids::period, tuning.periodCents(), nullptr);
    t.setProperty (ids::concertA, tuning.concertA(), nullptr);
    if (! tuning.isEqual())
    {
        juce::StringArray cents;
        for (int i = 0; i < tuning.divisions(); ++i)
            cents.add (juce::String (tuning.stepCents (i), 6));
        t.setProperty (ids::cents, cents.joinIntoString (" "), nullptr);
    }
    root.appendChild (t, nullptr);

    juce::ValueTree k (ids::key);
    k.setProperty (ids::tonic, key.tonic().value(), nullptr);
    k.setProperty (ids::scale, juce::String (std::string (key.type().name)), nullptr);
    root.appendChild (k, nullptr);

    juce::ValueTree p (ids::progression);
    p.setProperty (ids::selected, selectedIndex, nullptr);
    for (const auto& c : progression)
    {
        juce::ValueTree ct (ids::chord);
        ct.setProperty (ids::root, c.chord.root().value(), nullptr);
        ct.setProperty (ids::quality, juce::String (std::string (c.chord.quality().name)), nullptr);
        ct.setProperty (ids::notes, intsToString (c.notes), nullptr);
        ct.setProperty (ids::degree, juce::String (c.degree), nullptr);
        ct.setProperty (ids::symbol, juce::String (c.symbol), nullptr);
        p.appendChild (ct, nullptr);
    }
    root.appendChild (p, nullptr);

    root.setProperty (ids::manualNotes, intsToString (manualNotes), nullptr);
    return root;
}

void AppState::restoreFromValueTree (const juce::ValueTree& tree)
{
    if (! tree.hasType (ids::state))
        return;

    if (const auto t = tree.getChildWithName (ids::tuning); t.isValid())
    {
        const double period = t.getProperty (ids::period, 1200.0);
        theory::Tuning restored;
        if (static_cast<bool> (t.getProperty (ids::equal, true)))
        {
            restored = theory::Tuning::equal (t.getProperty (ids::divisions, theory::kDefaultDivisions), period);
        }
        else
        {
            std::vector<double> cents;
            for (const auto& s : juce::StringArray::fromTokens (t.getProperty (ids::cents).toString(), " ", ""))
                if (s.isNotEmpty())
                    cents.push_back (s.getDoubleValue());
            restored = theory::Tuning::fromCents (t.getProperty (ids::name).toString().toStdString(), cents, period);
        }
        tuning = restored.withConcertA (t.getProperty (ids::concertA, 440.0));
    }

    const int n = tuning.divisions();

    if (const auto k = tree.getChildWithName (ids::key); k.isValid())
        if (const auto* type = findScale (k.getProperty (ids::scale).toString()))
            key = theory::Scale (theory::PitchClass (k.getProperty (ids::tonic), n), *type);

    progression.clear();
    selectedIndex = -1;
    if (const auto p = tree.getChildWithName (ids::progression); p.isValid())
    {
        for (const auto& ct : p)
        {
            if (! ct.hasType (ids::chord))
                continue;
            const auto* quality = findQuality (ct.getProperty (ids::quality).toString());
            if (quality == nullptr)
                continue; // custom quality not known to this build

            ProgressionChord c { theory::Chord (theory::PitchClass (ct.getProperty (ids::root), n), *quality),
                                 intsFromString (ct.getProperty (ids::notes).toString()),
                                 ct.getProperty (ids::degree).toString().toStdString(),
                                 ct.getProperty (ids::symbol).toString().toStdString() };
            progression.push_back (normalised (std::move (c)));
        }

        const int sel = p.getProperty (ids::selected, -1);
        selectedIndex = juce::isPositiveAndBelow (sel, static_cast<int> (progression.size())) ? sel : -1;
    }

    manualNotes = intsFromString (tree.getProperty (ids::manualNotes).toString());
    sortUnique (manualNotes);
    heldNotes.clear();

    notify (allChanged);
}

} // namespace mbox
