#include "ScoreComponent.h"

#include "SmuflGlyphs.h"

#include <cmath>

namespace mbox::composition
{

namespace theory = mbox::theory;

namespace
{
    namespace colours
    {
        const juce::Colour background { 0xff15171c };
        const juce::Colour lines      { 0xff7d8392 };
        const juce::Colour ink        { 0xffe8eaf0 };
        const juce::Colour accent     { 0xff4fc3f7 };
        const juce::Colour secondary  { 0xffa0a6b4 };
    } // namespace colours

    // Diatonic index (octave * 7 + letter) of the bottom line of each staff.
    constexpr int kTrebleBottomLine = 4 * 7 + 2; // E4
    constexpr int kBassBottomLine   = 2 * 7 + 4; // G2

    // Key signature positions, as staff steps above the bottom line of the treble staff
    // (the bass clef uses the same shapes two steps lower).
    constexpr int kSharpSteps[] { 8, 5, 9, 6, 3, 7, 4 };
    constexpr int kFlatSteps[]  { 4, 7, 3, 6, 2, 5, 1 };

    juce::String chordSymbol (const std::vector<theory::SpelledPitchClass>& tones, const theory::Chord& chord, int inversion)
    {
        if (tones.empty())
            return {};

        auto s = juce::String (tones.front().name()) + juce::String (std::string (chord.quality().symbol));
        if (inversion > 0 && inversion < static_cast<int> (tones.size()))
            s << "/" << tones[static_cast<std::size_t> (inversion)].name();
        return s;
    }

    std::optional<instrument::GuitarVoicing> pickFingering (const instrument::Fretboard& board,
                                                            const std::vector<int>& classes,
                                                            std::optional<int> bass,
                                                            int previousPosition)
    {
        if (classes.empty())
            return std::nullopt;

        auto candidates = instrument::findVoicings (board, classes, bass);
        if (candidates.empty() && bass)
            candidates = instrument::findVoicings (board, classes);
        if (candidates.empty())
            return std::nullopt;

        // Stay near the previous hand position among the best-ranked shapes.
        std::size_t best = 0;
        int bestCost = std::numeric_limits<int>::max();
        for (std::size_t i = 0; i < std::min<std::size_t> (candidates.size(), 8); ++i)
        {
            const int position = candidates[i].lowestFret();
            const int cost = static_cast<int> (i) + (previousPosition < 0 ? 0 : 2 * std::abs (position - previousPosition));
            if (cost < bestCost)
            {
                bestCost = cost;
                best = i;
            }
        }
        return candidates[best];
    }

    // Spells a written note: as a chord tone when it is one, else by its degree in the key.
    theory::SpelledNote spellNote (int midi, const std::vector<theory::SpelledPitchClass>& tones, const theory::Key& key)
    {
        const auto pc = theory::PitchClass::fromMidi (midi);
        for (const auto& t : tones)
            if (t.pitchClass() == pc)
                return theory::SpelledNote::fromMidi (midi, t);
        return theory::SpelledNote::fromMidi (midi, key.spellDegree (pc));
    }
} // namespace

// ---------------------------------------------------------------------------

ScoreComponent::ScoreComponent()
    : tuningPreset (&instrument::stringTunings::guitarStandard),
      fretboard (instrument::Fretboard::fromPreset (instrument::stringTunings::guitarStandard))
{
    setOpaque (true);
}

void ScoreComponent::setInput (const ScoreInput& newInput)
{
    const bool divisionsChanged = newInput.divisions != input.divisions;
    input = newInput;
    if (divisionsChanged)
        rebuildFretboard();
    rebuild();
}

void ScoreComponent::setVoiceLeading (bool shouldVoiceLead)
{
    voiceLeading = shouldVoiceLead;
    rebuild();
}

void ScoreComponent::setSelectedIndex (int index)
{
    if (index != selectedBar)
    {
        selectedBar = index;
        repaint();
    }
}

void ScoreComponent::rebuildFretboard()
{
    fretboard = instrument::Fretboard::fromPreset (*tuningPreset, 22, input.divisions);
}

void ScoreComponent::setShowTablature (bool shouldShow)
{
    showTab = shouldShow;
    setSize (preferredWidth(), preferredHeight());
    repaint();
}

void ScoreComponent::setTuning (const instrument::TuningPreset& preset)
{
    tuningPreset = &preset;
    rebuildFretboard();
    rebuild();
}

void ScoreComponent::setStaffSpace (float pixels)
{
    staffSpace = juce::jlimit (5.0f, 20.0f, pixels);
    setSize (preferredWidth(), preferredHeight());
    repaint();
}

void ScoreComponent::rebuild()
{
    engraved.clear();
    theory::KeyboardVoicer voicer;
    int previousPosition = -1;

    for (const auto& item : input.chords)
    {
        EngravedChord e;
        e.tones = theory::spellChord (item.chord, input.key);

        std::vector<theory::SpelledNote> notes;
        if (voiceLeading || item.notes.empty())
        {
            const auto v = voicer.next (e.tones, item.inversion);
            notes.push_back (v.bass);
            notes.insert (notes.end(), v.upper.begin(), v.upper.end());
        }
        else
        {
            for (const int midi : item.notes)
                notes.push_back (spellNote (midi, e.tones, input.key));
        }

        // Below middle C goes on the bass staff; so does the lowest note unless it is
        // high enough to need several ledger lines there.
        for (std::size_t i = 0; i < notes.size(); ++i)
            (notes[i].midi() < 60 || (i == 0 && notes[i].midi() < 62) ? e.bassNotes : e.trebleNotes).push_back (notes[i]);

        e.fingering = pickFingering (fretboard, item.tabClasses, item.tabBass, previousPosition);
        if (e.fingering)
            previousPosition = e.fingering->lowestFret();

        e.symbol = item.symbol.empty() ? chordSymbol (e.tones, item.chord, item.inversion) : juce::String::fromUTF8 (item.symbol.c_str());
        e.roman = juce::String::fromUTF8 (item.roman.c_str());
        engraved.push_back (std::move (e));
    }

    setSize (preferredWidth(), preferredHeight());
    repaint();
}

// ---------------------------------------------------------------------------

ScoreComponent::Metrics ScoreComponent::metrics() const
{
    Metrics m {};
    m.sp = staffSpace;
    m.trebleTop = 6.0f * m.sp;
    m.bassTop = m.trebleTop + 10.0f * m.sp;
    m.romanBaseline = m.bassTop + 7.0f * m.sp;
    m.tabLineGap = 1.5f * m.sp;
    m.tabTop = m.romanBaseline + 2.5f * m.sp;

    const int numStrings = fretboard.numStrings();
    m.height = showTab ? m.tabTop + static_cast<float> (numStrings - 1) * m.tabLineGap + 2.5f * m.sp
                       : m.romanBaseline + 2.0f * m.sp;

    m.staffLeft = 2.2f * m.sp;
    m.clefX = m.staffLeft + 0.8f * m.sp;
    m.keySigX = m.clefX + 3.6f * m.sp;
    const int numAccidentals = std::abs (input.key.signature().fifths());
    m.firstBarX = m.keySigX + static_cast<float> (numAccidentals) * 1.1f * m.sp + 1.0f * m.sp;
    m.barWidth = 11.0f * m.sp;
    m.right = m.firstBarX + m.barWidth * static_cast<float> (juce::jmax<std::size_t> (1, engraved.size()));
    return m;
}

int ScoreComponent::preferredHeight() const { return static_cast<int> (std::ceil (metrics().height)); }
int ScoreComponent::preferredWidth() const  { return static_cast<int> (std::ceil (metrics().right + 1.5f * staffSpace)); }

int ScoreComponent::barAt (juce::Point<float> p) const
{
    const auto m = metrics();
    if (p.x < m.firstBarX || p.x >= m.right)
        return -1;
    const int i = static_cast<int> ((p.x - m.firstBarX) / m.barWidth);
    return i < static_cast<int> (engraved.size()) ? i : -1;
}

void ScoreComponent::mouseMove (const juce::MouseEvent& e)
{
    if (const int bar = barAt (e.position); bar != hoveredBar)
    {
        hoveredBar = bar;
        repaint();
    }
}

void ScoreComponent::mouseExit (const juce::MouseEvent&)
{
    hoveredBar = -1;
    repaint();
}

void ScoreComponent::mouseDown (const juce::MouseEvent& e)
{
    selectedBar = barAt (e.position);
    repaint();
    if (selectedBar >= 0 && onChordClicked)
        onChordClicked (selectedBar);
}

// ---------------------------------------------------------------------------

void ScoreComponent::drawStaffLines (juce::Graphics& g, const Metrics& m, float top, int numLines, float gap) const
{
    g.setColour (colours::lines);
    const float thickness = juce::jmax (1.0f, 0.13f * m.sp);
    for (int i = 0; i < numLines; ++i)
        g.fillRect (juce::Rectangle<float> (m.staffLeft, top + static_cast<float> (i) * gap - thickness * 0.5f,
                                            m.right - m.staffLeft, thickness));
}

void ScoreComponent::drawKeySignature (juce::Graphics& g, const Metrics& m, float staffBottom, int stepOffset) const
{
    const auto sig = input.key.signature();
    const bool sharps = sig.fifths() > 0;
    const auto glyph = sharps ? smufl::accidentalSharp : smufl::accidentalFlat;

    g.setColour (colours::ink);
    float x = m.keySigX;
    for (std::size_t i = 0; i < sig.accidentalLetters().size(); ++i)
    {
        const int step = (sharps ? kSharpSteps[i] : kFlatSteps[i]) + stepOffset;
        SmuflFont::instance().draw (g, glyph, x, staffBottom - static_cast<float> (step) * m.sp * 0.5f, m.sp);
        x += 1.1f * m.sp;
    }
}

void ScoreComponent::drawNotes (juce::Graphics& g, const Metrics& m, float x, float staffBottom, int bottomLineIndex,
                                const std::vector<theory::SpelledNote>& notes) const
{
    auto& font = SmuflFont::instance();
    const float headWidth = smufl::advanceOf (smufl::noteheadWhole) * m.sp;
    const auto sig = input.key.signature();

    struct Placed { int step; int alter; bool showAccidental; float headX; bool shifted; };
    std::vector<Placed> placed;

    for (const auto& n : notes)
    {
        Placed p { n.diatonicIndex() - bottomLineIndex, n.pitch.alter,
                   n.pitch.alter != sig.alterationOf (n.pitch.letter), x, false };

        // Seconds can't share a column: the upper note moves to the right of the stem side.
        if (! placed.empty() && p.step - placed.back().step == 1 && ! placed.back().shifted)
        {
            p.headX = x + headWidth;
            p.shifted = true;
        }
        placed.push_back (p);
    }

    auto yOf = [&] (int step) { return staffBottom - static_cast<float> (step) * m.sp * 0.5f; };

    // Ledger lines
    g.setColour (colours::lines);
    const float ledgerThickness = juce::jmax (1.0f, 0.16f * m.sp);
    for (const auto& p : placed)
    {
        const float x0 = p.headX - 0.4f * m.sp;
        const float x1 = p.headX + headWidth + 0.4f * m.sp;
        for (int s = -2; s >= p.step; s -= 2)
            g.fillRect (juce::Rectangle<float> (x0, yOf (s) - ledgerThickness * 0.5f, x1 - x0, ledgerThickness));
        for (int s = 10; s <= p.step; s += 2)
            g.fillRect (juce::Rectangle<float> (x0, yOf (s) - ledgerThickness * 0.5f, x1 - x0, ledgerThickness));
    }

    // Noteheads
    g.setColour (colours::ink);
    for (const auto& p : placed)
        font.draw (g, smufl::noteheadWhole, p.headX, yOf (p.step), m.sp);

    // Accidentals, stacked in columns from the top so that they never collide.
    std::vector<std::pair<int, int>> columns; // (column, step)
    for (auto it = placed.rbegin(); it != placed.rend(); ++it)
    {
        if (! it->showAccidental)
            continue;

        int column = 0;
        for (bool clash = true; clash; )
        {
            clash = false;
            for (const auto& [c, s] : columns)
                if (c == column && std::abs (s - it->step) < 6)
                {
                    clash = true;
                    ++column;
                    break;
                }
        }
        columns.emplace_back (column, it->step);

        const auto glyph = smufl::accidentalFor (it->alter);
        const float ax = x - 0.25f * m.sp - static_cast<float> (column + 1) * 1.25f * m.sp
                         - (glyph == smufl::accidentalDoubleFlat ? 0.6f * m.sp : 0.0f);
        font.draw (g, glyph, ax, yOf (it->step), m.sp);
    }
}

void ScoreComponent::drawTablature (juce::Graphics& g, const Metrics& m) const
{
    const int numStrings = fretboard.numStrings();
    const float tabBottom = m.tabTop + static_cast<float> (numStrings - 1) * m.tabLineGap;

    drawStaffLines (g, m, m.tabTop, numStrings, m.tabLineGap);

    // Clef: the SMuFL glyph for 6 strings, plain letters otherwise.
    g.setColour (colours::ink);
    if (numStrings == 6)
    {
        SmuflFont::instance().draw (g, smufl::tabClef6String, m.clefX + 0.4f * m.sp, (m.tabTop + tabBottom) * 0.5f, m.sp);
    }
    else
    {
        g.setFont (juce::FontOptions (1.3f * m.sp, juce::Font::bold));
        const auto box = juce::Rectangle<float> (m.clefX, m.tabTop, 2.0f * m.sp, tabBottom - m.tabTop);
        g.drawFittedText ("T\nA\nB", box.toNearestInt(), juce::Justification::centred, 3);
    }

    // Bar lines
    g.setColour (colours::lines);
    const float barThickness = juce::jmax (1.0f, 0.16f * m.sp);
    g.fillRect (juce::Rectangle<float> (m.staffLeft, m.tabTop, barThickness, tabBottom - m.tabTop));
    for (std::size_t i = 1; i <= engraved.size(); ++i)
    {
        const float bx = m.firstBarX + static_cast<float> (i) * m.barWidth;
        g.fillRect (juce::Rectangle<float> (bx - barThickness, m.tabTop, barThickness, tabBottom - m.tabTop));
    }

    // Fret numbers
    g.setFont (juce::FontOptions (1.35f * m.sp, juce::Font::bold));
    const float headCentre = 4.0f * m.sp + smufl::advanceOf (smufl::noteheadWhole) * m.sp * 0.5f;

    for (std::size_t i = 0; i < engraved.size(); ++i)
    {
        const float cx = m.firstBarX + static_cast<float> (i) * m.barWidth + headCentre;
        const auto& fingering = engraved[i].fingering;

        if (! fingering)
        {
            g.setColour (colours::secondary);
            g.drawText ("?", juce::Rectangle<float> (cx - m.sp, (m.tabTop + tabBottom) * 0.5f - m.sp, 2.0f * m.sp, 2.0f * m.sp),
                        juce::Justification::centred);
            continue;
        }

        for (int s = 0; s < numStrings && s < static_cast<int> (fingering->frets.size()); ++s)
        {
            const int fret = fingering->frets[static_cast<std::size_t> (s)];
            if (fret < 0)
                continue;

            const float y = tabBottom - static_cast<float> (s) * m.tabLineGap;
            const auto text = juce::String (fret);
            const float w = (text.length() > 1 ? 1.7f : 1.1f) * m.sp;
            const auto box = juce::Rectangle<float> (cx - w * 0.5f, y - 0.7f * m.sp, w, 1.4f * m.sp);

            g.setColour (colours::background);
            g.fillRect (box);
            g.setColour (colours::ink);
            g.drawText (text, box, juce::Justification::centred, false);
        }
    }
}

void ScoreComponent::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    const auto m = metrics();
    auto& font = SmuflFont::instance();

    const float trebleBottom = m.trebleTop + 4.0f * m.sp;
    const float bassBottom = m.bassTop + 4.0f * m.sp;
    const float systemBottom = showTab ? m.tabTop + static_cast<float> (fretboard.numStrings() - 1) * m.tabLineGap : bassBottom;

    drawStaffLines (g, m, m.trebleTop, 5, m.sp);
    drawStaffLines (g, m, m.bassTop, 5, m.sp);

    // Brace + systemic bar line + bar lines across the grand staff
    g.setColour (colours::ink);
    const float braceScale = (bassBottom - m.trebleTop) / (4.0f * m.sp);
    font.draw (g, smufl::brace, m.staffLeft - 1.3f * m.sp, bassBottom, m.sp * braceScale);

    g.setColour (colours::lines);
    const float thin = juce::jmax (1.0f, 0.16f * m.sp);
    g.fillRect (juce::Rectangle<float> (m.staffLeft, m.trebleTop, thin, bassBottom - m.trebleTop));
    for (std::size_t i = 1; i <= engraved.size(); ++i)
    {
        const float bx = m.firstBarX + static_cast<float> (i) * m.barWidth;
        const bool last = i == engraved.size();
        if (last)
        {
            g.fillRect (juce::Rectangle<float> (bx - 0.5f * m.sp, m.trebleTop, 0.5f * m.sp, bassBottom - m.trebleTop));
            g.fillRect (juce::Rectangle<float> (bx - 0.9f * m.sp - thin, m.trebleTop, thin, bassBottom - m.trebleTop));
        }
        else
        {
            g.fillRect (juce::Rectangle<float> (bx - thin, m.trebleTop, thin, bassBottom - m.trebleTop));
        }
    }

    // Clefs and key signatures
    g.setColour (colours::ink);
    font.draw (g, smufl::gClef, m.clefX, trebleBottom - 1.0f * m.sp, m.sp); // on the G line
    font.draw (g, smufl::fClef, m.clefX, bassBottom - 3.0f * m.sp, m.sp);   // on the F line
    drawKeySignature (g, m, trebleBottom, 0);
    drawKeySignature (g, m, bassBottom, -2);

    // Chords
    for (std::size_t i = 0; i < engraved.size(); ++i)
    {
        const auto& e = engraved[i];
        const float barX = m.firstBarX + static_cast<float> (i) * m.barWidth;
        const float noteX = barX + 4.0f * m.sp;

        drawNotes (g, m, noteX, trebleBottom, kTrebleBottomLine, e.trebleNotes);
        drawNotes (g, m, noteX, bassBottom, kBassBottomLine, e.bassNotes);

        const auto labelBox = [&] (float baseline, float size)
        {
            return juce::Rectangle<float> (barX, baseline - size, m.barWidth, size * 1.3f);
        };

        g.setColour (colours::accent);
        g.setFont (juce::FontOptions (1.7f * m.sp, juce::Font::bold));
        g.drawText (e.symbol, labelBox (m.trebleTop - 3.2f * m.sp, 1.7f * m.sp), juce::Justification::centred, false);

        g.setColour (colours::secondary);
        g.setFont (juce::FontOptions (1.6f * m.sp, juce::Font::italic));
        g.drawText (e.roman, labelBox (m.romanBaseline, 1.6f * m.sp), juce::Justification::centred, false);
    }

    if (showTab)
        drawTablature (g, m);

    // Hover / selection highlight, drawn over the content
    for (const auto& [bar, alpha] : { std::pair { hoveredBar, 0.06f }, std::pair { selectedBar, 0.14f } })
    {
        if (bar < 0)
            continue;
        const float bx = m.firstBarX + static_cast<float> (bar) * m.barWidth;
        g.setColour (colours::accent.withAlpha (alpha));
        g.fillRoundedRectangle (juce::Rectangle<float> (bx + 0.2f * m.sp, m.trebleTop - 4.5f * m.sp,
                                                        m.barWidth - 0.4f * m.sp, systemBottom - m.trebleTop + 5.5f * m.sp),
                                0.6f * m.sp);
    }
}

} // namespace mbox::composition
