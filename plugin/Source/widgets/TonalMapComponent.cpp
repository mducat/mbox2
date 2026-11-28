#include "TonalMapComponent.h"
#include "Widget.h"

#include <cmath>
#include <map>

namespace mbox
{

namespace
{
    // Motion colours (legend + arrows).
    const juce::Colour kStrong    { 0xffffd54f };
    const juce::Colour kCommon    { 0xffb0bec5 };
    const juce::Colour kDeceptive { 0xfff06292 };
    const juce::Colour kPath      { 0xffffd54f };

    juce::String originLabel (theory::ChordOrigin o)
    {
        switch (o)
        {
            case theory::ChordOrigin::Diatonic:  return juce::String::fromUTF8 ("DIATONIQUES");
            case theory::ChordOrigin::Secondary: return juce::String::fromUTF8 ("DOMINANTES SECONDAIRES");
            case theory::ChordOrigin::Borrowed:  return juce::String::fromUTF8 ("EMPRUNTS AU MODE PARALLÈLE");
            case theory::ChordOrigin::Chromatic: return juce::String::fromUTF8 ("CHROMATIQUES");
            case theory::ChordOrigin::Cadential: return juce::String::fromUTF8 ("CADENCE");
        }
        return {};
    }

    juce::String functionLabel (theory::HarmonicFunction f)
    {
        switch (f)
        {
            case theory::HarmonicFunction::Tonic:       return juce::String::fromUTF8 ("Tonique");
            case theory::HarmonicFunction::Subdominant: return juce::String::fromUTF8 ("Sous-dominante");
            case theory::HarmonicFunction::Dominant:    return juce::String::fromUTF8 ("Dominante");
        }
        return {};
    }

    juce::String functionRole (theory::HarmonicFunction f)
    {
        switch (f)
        {
            case theory::HarmonicFunction::Tonic:       return juce::String::fromUTF8 ("repos, stabilité");
            case theory::HarmonicFunction::Subdominant: return juce::String::fromUTF8 ("éloignement, préparation");
            case theory::HarmonicFunction::Dominant:    return juce::String::fromUTF8 ("tension vers la tonique");
        }
        return {};
    }

    juce::String strengthLabel (theory::MotionStrength s)
    {
        switch (s)
        {
            case theory::MotionStrength::Strong:    return juce::String::fromUTF8 ("forte");
            case theory::MotionStrength::Common:    return juce::String::fromUTF8 ("fréquente");
            case theory::MotionStrength::Deceptive: return juce::String::fromUTF8 ("rompue");
        }
        return {};
    }

    juce::Colour strengthColour (theory::MotionStrength s)
    {
        switch (s)
        {
            case theory::MotionStrength::Strong:    return kStrong;
            case theory::MotionStrength::Common:    return kCommon;
            case theory::MotionStrength::Deceptive: return kDeceptive;
        }
        return kCommon;
    }

    /** Same chord on the map: inversion ignored. */
    bool sameChord (const theory::RomanChord& a, const theory::RomanChord& b)
    {
        return a.withInversion (0) == b.withInversion (0);
    }

    /** Same chord up to its seventh (ii ~ ii7): used to fall back on visible cells. */
    bool sameFamily (const theory::RomanChord& a, const theory::RomanChord& b)
    {
        return a.degree == b.degree && a.alteration == b.alteration && a.origin == b.origin
            && a.targetDegree == b.targetDegree && a.special == b.special
            && a.chord.root() == b.chord.root();
    }

    /** Point where the segment from the rectangle's centre towards `towards` leaves the rectangle. */
    juce::Point<float> edgePoint (juce::Rectangle<float> r, juce::Point<float> towards)
    {
        const auto c = r.getCentre();
        const auto d = towards - c;
        const float ax = std::abs (d.x), ay = std::abs (d.y);
        if (ax < 1.0e-3f && ay < 1.0e-3f)
            return c;
        const float sx = ax >= 1.0e-3f ? (r.getWidth() * 0.5f) / ax : 1.0e9f;
        const float sy = ay >= 1.0e-3f ? (r.getHeight() * 0.5f) / ay : 1.0e9f;
        return c + d * juce::jmin (sx, sy, 1.0f);
    }

    /** Curved arrow from a to b (bowing to the left of the direction), with an arrowhead. */
    void drawCurvedArrow (juce::Graphics& g, juce::Rectangle<float> from, juce::Rectangle<float> to,
                          juce::Colour colour, float thickness, bool dashed, float bend = 0.22f)
    {
        const auto start = edgePoint (from.expanded (3.0f), to.getCentre());
        const auto end   = edgePoint (to.expanded (4.0f), from.getCentre());
        const auto delta = end - start;
        const float length = delta.getDistanceFromOrigin();
        if (length < 4.0f)
            return;

        // Long arrows (across a whole column) bend less, so they stay close to the cells they join.
        const float effectiveBend = length > 380.0f ? bend * 0.45f : bend;
        const juce::Point<float> normal (-delta.y / length, delta.x / length);
        const auto control = (start + end) * 0.5f + normal * (length * effectiveBend);

        juce::Path curve;
        curve.startNewSubPath (start);
        curve.quadraticTo (control, end);

        juce::Path stroke;
        juce::PathStrokeType type (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        if (dashed)
        {
            const float dashes[] { 6.0f, 4.0f };
            type.createDashedStroke (stroke, curve, dashes, 2);
        }
        else
        {
            type.createStrokedPath (stroke, curve);
        }

        // Soft halo so arrows stay readable over the cells.
        g.setColour (colours::background.withAlpha (0.55f));
        g.strokePath (curve, juce::PathStrokeType (thickness + 3.0f));
        g.setColour (colour);
        g.fillPath (stroke);

        const auto dir = (end - control);
        const float dl = juce::jmax (0.001f, dir.getDistanceFromOrigin());
        const auto u = dir / dl;
        const juce::Point<float> n (-u.y, u.x);
        const float headLength = 7.0f + thickness * 2.0f, headWidth = 4.0f + thickness * 1.5f;

        juce::Path head;
        head.addTriangle (end, end - u * headLength + n * headWidth, end - u * headLength - n * headWidth);
        g.fillPath (head);
    }

    constexpr int kBannerHeight  = 40;
    constexpr int kHeaderHeight  = 52;
    constexpr int kGroupHeight   = 22;
    constexpr int kCellWidth     = 80;
    constexpr int kCellHeight    = 46;
    constexpr int kHeroHeight    = 60;
    constexpr int kGap           = 6;
    constexpr int kColumnPadding = 10;
    constexpr int kNoPatternId   = 1000;
} // namespace

juce::Colour functionColour (theory::HarmonicFunction f)
{
    switch (f)
    {
        case theory::HarmonicFunction::Tonic:       return juce::Colour (0xff4fc3f7);
        case theory::HarmonicFunction::Subdominant: return juce::Colour (0xff81c784);
        case theory::HarmonicFunction::Dominant:    return juce::Colour (0xffff8a65);
    }
    return colours::text;
}

juce::String keyDisplayName (const theory::Key& k)
{
    return juce::String (k.tonic.name (k.prefersFlats())) + (k.mode == theory::Mode::Major ? " majeur" : " mineur");
}

// ---------------------------------------------------------------------------
// One clickable chord
// ---------------------------------------------------------------------------

class TonalMapComponent::ChordCell final : public juce::Component,
                                           public juce::SettableTooltipClient
{
public:
    ChordCell (TonalMapComponent& o, theory::RomanChord c) : owner (o), chord (std::move (c))
    {
        updateTooltip();
    }

    const theory::RomanChord& getChord() const noexcept { return chord; }

    bool isHighlighted() const noexcept { return highlighted; }
    void setHighlighted (bool h)
    {
        if (highlighted != h)
        {
            highlighted = h;
            repaint();
        }
    }

    bool isHero() const noexcept { return hero; }
    void setHero (bool h) { hero = h; }

    void paint (juce::Graphics& g) override
    {
        const auto colour = functionColour (chord.function);
        const auto r = getLocalBounds().toFloat().reduced (1.0f);
        const bool hover = isMouseOver();
        const bool diatonic = chord.origin == theory::ChordOrigin::Diatonic;
        const float corner = hero ? 10.0f : 8.0f;

        if (highlighted)
        {
            g.setGradientFill (juce::ColourGradient (colour.brighter (0.15f), r.getTopLeft(),
                                                     colour.darker (0.25f), r.getBottomLeft(), false));
            g.fillRoundedRectangle (r, corner);
        }
        else
        {
            const auto top = hover ? colours::panel.interpolatedWith (colour, 0.22f) : colours::panel.brighter (0.06f);
            const auto bottom = hover ? colours::panel.interpolatedWith (colour, 0.10f) : colours::panel.darker (0.12f);
            g.setGradientFill (juce::ColourGradient (top, r.getTopLeft(), bottom, r.getBottomLeft(), false));
            g.fillRoundedRectangle (r, corner);

            if (diatonic)
            {
                // Coloured accent bar on the left of the key's own chords.
                juce::Path bar;
                bar.addRoundedRectangle (r.getX(), r.getY(), 4.0f, r.getHeight(), 2.0f, 2.0f, true, false, true, false);
                g.setColour (colour);
                g.fillPath (bar);
            }

            g.setColour (colour.withAlpha (hover ? 0.9f : (diatonic ? 0.45f : 0.28f)));
            if (diatonic || hover)
            {
                g.drawRoundedRectangle (r, corner, 1.0f);
            }
            else
            {
                juce::Path outline, dashed;
                outline.addRoundedRectangle (r, corner);
                const float dashes[] { 4.0f, 3.0f };
                juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);
                g.fillPath (dashed);
            }
        }

        auto text = getLocalBounds().reduced (6, 3);
        if (diatonic && ! highlighted)
            text.removeFromLeft (2);

        g.setColour (highlighted ? colours::background : colours::text);
        g.setFont (juce::FontOptions (hero ? 22.0f : 16.0f, juce::Font::bold));
        g.drawFittedText (juce::String (chord.numeral()), text.removeFromTop (text.getHeight() * 3 / 5),
                          juce::Justification::centredBottom, 1, 0.6f);

        g.setColour (highlighted ? colours::background.withAlpha (0.75f) : colours::textDim);
        g.setFont (juce::FontOptions (hero ? 13.0f : 11.5f));
        g.drawFittedText (juce::String (chord.symbol()), text, juce::Justification::centredTop, 1, 0.6f);
    }

    void mouseEnter (const juce::MouseEvent&) override { owner.setHoveredCell (this); repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { owner.setHoveredCell (nullptr); repaint(); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! getLocalBounds().contains (e.getPosition()))
            return;

        if (e.mods.isPopupMenu())
            showInversionMenu();
        else
            owner.chooseChord (chord);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        // Plain wheel scrolls the map; Ctrl/Cmd + wheel changes the inversion.
        if (! e.mods.isCommandDown())
        {
            juce::Component::mouseWheelMove (e, wheel);
            return;
        }
        const int n = chord.size();
        const int step = (wheel.deltaY < 0.0f) ? 1 : -1;
        setInversion ((chord.inversion + step + n) % n);
    }

private:
    void setInversion (int inversion)
    {
        chord = chord.withInversion (inversion);
        updateTooltip();
        repaint();
    }

    void showInversionMenu()
    {
        juce::PopupMenu menu;
        menu.addSectionHeader ("Renversement");
        for (int i = 0; i < chord.size(); ++i)
        {
            const auto candidate = chord.withInversion (i);
            if (candidate.inversion != i)
                break; // augmented sixths only exist in one position
            menu.addItem (i + 1, juce::String (candidate.numeral()) + "   (" + juce::String (candidate.symbol()) + ")",
                          true, i == chord.inversion);
        }

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [safe = juce::Component::SafePointer<ChordCell> (this)] (int result)
                            {
                                if (safe == nullptr || result == 0)
                                    return;
                                safe->setInversion (result - 1);
                                safe->owner.chooseChord (safe->chord);
                            });
    }

    void updateTooltip()
    {
        juce::String tip = juce::String (chord.numeral()) + " - " + juce::String (chord.symbol());
        tip << "\n" << functionLabel (chord.function);
        if (chord.alternateFunction)
            tip << juce::String::fromUTF8 (" (aussi ") << functionLabel (*chord.alternateFunction).toLowerCase() << ")";
        tip << ", " << originLabel (chord.origin).toLowerCase();
        tip << juce::String::fromUTF8 ("\nClic : ajouter  -  Clic droit ou Ctrl+molette : renversement");
        setTooltip (tip);
    }

    TonalMapComponent& owner;
    theory::RomanChord chord;
    bool highlighted = false;
    bool hero = false;
};

// ---------------------------------------------------------------------------
// Canvas (columns) and overlay (arrows)
// ---------------------------------------------------------------------------

class TonalMapComponent::Canvas final : public juce::Component
{
public:
    explicit Canvas (TonalMapComponent& o) : owner (o) {}
    void paint (juce::Graphics& g) override { owner.paintCanvas (g); }

private:
    TonalMapComponent& owner;
};

class TonalMapComponent::Overlay final : public juce::Component
{
public:
    explicit Overlay (TonalMapComponent& o) : owner (o) { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics& g) override { owner.paintOverlay (g); }

private:
    TonalMapComponent& owner;
};

// ---------------------------------------------------------------------------
// Map
// ---------------------------------------------------------------------------

TonalMapComponent::TonalMapComponent()
    : canvas (std::make_unique<Canvas> (*this)),
      overlay (std::make_unique<Overlay> (*this))
{
    columns[0].function = theory::HarmonicFunction::Tonic;
    columns[1].function = theory::HarmonicFunction::Subdominant;
    columns[2].function = theory::HarmonicFunction::Dominant;

    canvas->addAndMakeVisible (*overlay);
    viewport.setViewedComponent (canvas.get(), false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);

    keyLabel.setFont (juce::FontOptions (17.0f, juce::Font::bold));
    keyLabel.setColour (juce::Label::textColourId, colours::text);
    keyLabel.setText (keyDisplayName (key), juce::dontSendNotification);
    addAndMakeVisible (keyLabel);

    auto styleToggle = [this] (juce::TextButton& b, const char* text, const char* tip)
    {
        b.setButtonText (juce::String::fromUTF8 (text));
        b.setTooltip (juce::String::fromUTF8 (tip));
        b.setClickingTogglesState (true);
        b.setColour (juce::TextButton::buttonColourId, colours::panel);
        b.setColour (juce::TextButton::buttonOnColourId, colours::accent.withAlpha (0.28f));
        b.setColour (juce::TextButton::textColourOffId, colours::textDim);
        b.setColour (juce::TextButton::textColourOnId, colours::text);
        b.setColour (juce::ComboBox::outlineColourId, colours::dim);
        addAndMakeVisible (b);
    };

    auto setupFamily = [this, styleToggle] (juce::TextButton& b, const char* text, const char* tip, bool& flag)
    {
        styleToggle (b, text, tip);
        b.setToggleState (flag, juce::dontSendNotification);
        b.onClick = [this, &b, &flag]
        {
            flag = b.getToggleState();
            rebuild();
        };
    };
    setupFamily (seventhsToggle,  "Septièmes",    "Afficher les accords de septième", options.sevenths);
    setupFamily (secondaryToggle, "Secondaires",  "Dominantes secondaires (V/x, V7/x, vii°7/x)", options.secondaryDominants);
    setupFamily (borrowedToggle,  "Emprunts",     "Accords empruntés au mode parallèle", options.borrowedChords);
    setupFamily (chromaticToggle, "Chromatiques", "Sixte napolitaine et sixtes augmentées", options.chromaticChords);

    styleToggle (motionsToggle, "Mouvements",
                 "Flèches vers les accords qui suivent naturellement l'accord survolé (ou le dernier de la progression)");
    motionsToggle.setColour (juce::TextButton::buttonOnColourId, kStrong.withAlpha (0.30f));
    motionsToggle.setToggleState (true, juce::dontSendNotification);
    motionsToggle.onClick = [this] { updateFocus(); };

    patternBox.setTextWhenNothingSelected (juce::String::fromUTF8 ("Enchaînement..."));
    patternBox.setTooltip (juce::String::fromUTF8 ("Afficher un enchaînement courant sur la carte (numéroté), à écouter ou ajouter à la progression"));
    patternBox.onChange = [this] { patternChanged(); };
    addAndMakeVisible (patternBox);

    for (auto* b : { &playPatternButton, &addPatternButton })
    {
        b->setColour (juce::TextButton::buttonColourId, colours::panel);
        b->setColour (juce::TextButton::textColourOffId, colours::text);
        addAndMakeVisible (b);
    }
    playPatternButton.setButtonText (juce::String::fromUTF8 ("Écouter"));
    addPatternButton.setButtonText (juce::String::fromUTF8 ("Ajouter"));
    playPatternButton.setTooltip (juce::String::fromUTF8 ("Jouer l'enchaînement"));
    addPatternButton.setTooltip (juce::String::fromUTF8 ("Ajouter l'enchaînement à la progression"));
    playPatternButton.onClick = [this] { if (onPatternPlay) if (auto p = currentPattern(); ! p.empty()) onPatternPlay (p); };
    addPatternButton.onClick  = [this] { if (onPatternAdd)  if (auto p = currentPattern(); ! p.empty()) onPatternAdd (p); };

    rebuild();
}

TonalMapComponent::~TonalMapComponent() = default;

void TonalMapComponent::setKey (const theory::Key& newKey)
{
    if (newKey == key)
        return;

    key = newKey;
    keyLabel.setText (keyDisplayName (key), juce::dontSendNotification);
    rebuild();
}

void TonalMapComponent::setSelection (const std::vector<theory::RomanChord>& chords)
{
    selection = chords;
    updateHighlights();
    updateFocus();
}

void TonalMapComponent::chooseChord (const theory::RomanChord& chord)
{
    if (onChordChosen)
        onChordChosen (chord);
}

void TonalMapComponent::setHoveredCell (ChordCell* cell)
{
    if (cell == nullptr && hoveredCell != nullptr && hoveredCell->isMouseOver())
        return; // mouseExit of a cell after the next one's mouseEnter
    if (hoveredCell != cell)
    {
        hoveredCell = cell;
        updateFocus();
    }
}

void TonalMapComponent::rebuild()
{
    hoveredCell = nullptr;
    for (auto& column : columns)
        column.cells.clear();
    cells.clear();

    for (auto& chord : theory::chordsInKey (key, options))
    {
        auto cell = std::make_unique<ChordCell> (*this, chord);
        canvas->addAndMakeVisible (*cell);
        columns[static_cast<std::size_t> (chord.function)].cells.push_back (cell.get());
        cells.push_back (std::move (cell));
    }
    overlay->toFront (false);

    // Progressions of the mode (the selected one is kept when the mode stays the same).
    const auto previous = patternBox.getSelectedId();
    patternBox.clear (juce::dontSendNotification);
    patternBox.addItem (juce::String::fromUTF8 ("Aucun enchaînement"), kNoPatternId);
    patternBox.addSeparator();
    const auto& patterns = theory::commonProgressions (key.mode);
    for (int i = 0; i < static_cast<int> (patterns.size()); ++i)
        patternBox.addItem (juce::String::fromUTF8 (patterns[(std::size_t) i].name.c_str()), i + 1);
    if (previous != kNoPatternId && previous > 0 && previous <= static_cast<int> (patterns.size()))
        patternBox.setSelectedId (previous, juce::dontSendNotification);

    updateHighlights();
    resized();
    patternChanged();
}

int TonalMapComponent::currentPatternIndex() const
{
    const int id = patternBox.getSelectedId();
    const int count = static_cast<int> (theory::commonProgressions (key.mode).size());
    return (id >= 1 && id <= count) ? id - 1 : -1;
}

std::vector<theory::RomanChord> TonalMapComponent::currentPattern() const
{
    const int i = currentPatternIndex();
    const auto& patterns = theory::commonProgressions (key.mode);
    if (i < 0 || i >= static_cast<int> (patterns.size()))
        return {};
    return theory::realise (key, patterns[static_cast<std::size_t> (i)]);
}

void TonalMapComponent::patternChanged()
{
    const int i = currentPatternIndex();
    playPatternButton.setEnabled (i >= 0);
    addPatternButton.setEnabled (i >= 0);
    repaint(); // legend / description
    updateFocus();
}

void TonalMapComponent::updateHighlights()
{
    for (auto& cell : cells)
    {
        const auto& c = cell->getChord();
        cell->setHighlighted (std::any_of (selection.begin(), selection.end(),
                                           [&] (const theory::RomanChord& s) { return sameChord (s, c); }));
    }
    canvas->repaint();
}

TonalMapComponent::ChordCell* TonalMapComponent::findCell (const theory::RomanChord& chord) const
{
    for (const auto& cell : cells)
        if (sameChord (cell->getChord(), chord))
            return cell.get();
    for (const auto& cell : cells)
        if (sameFamily (cell->getChord(), chord))
            return cell.get();
    return nullptr;
}

std::vector<std::pair<TonalMapComponent::ChordCell*, theory::MotionStrength>>
TonalMapComponent::motionsFrom (const theory::RomanChord& from) const
{
    std::vector<std::pair<ChordCell*, theory::MotionStrength>> out;
    for (const auto& cell : cells)
    {
        const auto& to = cell->getChord();

        // One arrow per chord: skip the seventh when its triad is on the map.
        if (to.size() == 4 && std::any_of (cells.begin(), cells.end(), [&] (const auto& other)
                { return other->getChord().size() == 3 && sameFamily (other->getChord(), to); }))
            continue;

        if (const auto s = theory::motionStrength (from, to))
            out.emplace_back (cell.get(), *s);
    }
    return out;
}

void TonalMapComponent::updateFocus()
{
    // Dim every cell that is not part of what the arrows show.
    std::vector<ChordCell*> focused;

    if (const auto path = currentPattern(); ! path.empty())
    {
        for (const auto& c : path)
            if (auto* cell = findCell (c))
                focused.push_back (cell);
    }
    else if (motionsToggle.getToggleState() && hoveredCell != nullptr)
    {
        focused.push_back (hoveredCell);
        for (const auto& [cell, strength] : motionsFrom (hoveredCell->getChord()))
            focused.push_back (cell);
    }

    for (auto& cell : cells)
    {
        const bool on = focused.empty() || cell->isHighlighted()
                     || std::find (focused.begin(), focused.end(), cell.get()) != focused.end();
        cell->setAlpha (on ? 1.0f : 0.32f);
    }

    overlay->repaint();
    canvas->repaint();
}

void TonalMapComponent::resized()
{
    auto area = getLocalBounds().reduced (10, 8);

    {
        auto row = area.removeFromTop (kToolbarRowHeight);
        row.removeFromRight (kToolbarReserve);
        keyLabel.setBounds (row.removeFromLeft (150));
        for (auto* t : { &seventhsToggle, &secondaryToggle, &borrowedToggle, &chromaticToggle })
        {
            t->setBounds (row.removeFromLeft (104).reduced (2, 3));
            row.removeFromLeft (2);
        }
    }
    area.removeFromTop (4);
    {
        auto row = area.removeFromTop (kToolbarRowHeight);
        motionsToggle.setBounds (row.removeFromLeft (110).reduced (2, 3));
        row.removeFromLeft (10);
        patternBox.setBounds (row.removeFromLeft (230).reduced (2, 3));
        playPatternButton.setBounds (row.removeFromLeft (80).reduced (2, 3));
        addPatternButton.setBounds (row.removeFromLeft (80).reduced (2, 3));
        row.removeFromLeft (12);
        legendArea = row;
    }
    area.removeFromTop (6);

    viewport.setBounds (area);

    const int width = area.getWidth() - viewport.getScrollBarThickness() - 2;
    bannerArea = { 0, 0, width, kBannerHeight };

    const int columnWidth = (width - 2 * kGap) / 3;
    int bottom = kBannerHeight;
    int x = 0;
    for (auto& column : columns)
    {
        column.bounds = { x, kBannerHeight, columnWidth, 0 };
        x += columnWidth + kGap;
        bottom = juce::jmax (bottom, layoutColumn (column));
    }

    // Columns share the same height: the viewport height, or the tallest content.
    const int height = juce::jmax (bottom + kGap, area.getHeight());
    for (auto& column : columns)
        column.bounds.setBottom (height - 1);

    canvas->setSize (width, height);
    overlay->setBounds (canvas->getLocalBounds());
}

int TonalMapComponent::layoutColumn (Column& column)
{
    auto inner = column.bounds.withHeight (10000).reduced (kColumnPadding).withTrimmedTop (kHeaderHeight - kColumnPadding + 4);

    // Even number of cells per row so a triad and its seventh stay side by side.
    int perRow = juce::jmax (1, (inner.getWidth() + kGap) / (kCellWidth + kGap));
    if (perRow > 1)
        perRow -= perRow % 2;
    const int cellWidth = (inner.getWidth() - (perRow - 1) * kGap) / perRow;

    int y = inner.getY();
    int x = 0;
    int rowHeight = kCellHeight;
    int rowIndex = 0;
    std::optional<theory::ChordOrigin> currentOrigin;
    std::optional<int> previousTarget;

    for (auto* cell : column.cells)
    {
        const auto origin = cell->getChord().origin;
        if (origin != currentOrigin)
        {
            if (currentOrigin)
                y += rowHeight + kGap;
            y += kGroupHeight;
            x = 0;
            rowIndex = 0;
            currentOrigin = origin;
        }
        else if (x == perRow
                 || (rowIndex == 0 && origin == theory::ChordOrigin::Diatonic && x == juce::jmin (2, perRow))
                 || (origin == theory::ChordOrigin::Secondary && x > 0
                     && cell->getChord().targetDegree != previousTarget))
        {
            // Rows wrap when full, after the primary chord and its seventh, and before
            // each group of applied chords (V/x, V7/x, vii°7/x).
            y += rowHeight + kGap;
            x = 0;
            ++rowIndex;
        }

        // The column's primary chord (I, IV, V and their sevenths) is drawn larger.
        const bool hero = origin == theory::ChordOrigin::Diatonic && rowIndex == 0;
        rowHeight = hero ? kHeroHeight : kCellHeight;
        cell->setHero (hero);

        cell->setBounds (inner.getX() + x * (cellWidth + kGap), y, cellWidth, rowHeight);
        previousTarget = cell->getChord().targetDegree;
        ++x;
    }

    return column.cells.empty() ? inner.getY() : y + rowHeight + kColumnPadding;
}

void TonalMapComponent::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    // Description of the chosen progression, else the legend of the motion arrows.
    if (const int i = currentPatternIndex(); i >= 0)
    {
        const auto& p = theory::commonProgressions (key.mode)[static_cast<std::size_t> (i)];
        g.setColour (kPath);
        g.setFont (juce::FontOptions (12.5f).withStyle ("Italic"));
        g.drawFittedText (juce::String::fromUTF8 (p.description.c_str()), legendArea, juce::Justification::centredLeft, 1);
    }
    else if (! legendArea.isEmpty() && motionsToggle.getToggleState())
    {
        auto r = legendArea.reduced (0, 6);
        g.setFont (juce::FontOptions (11.5f));
        for (const auto s : { theory::MotionStrength::Strong, theory::MotionStrength::Common, theory::MotionStrength::Deceptive })
        {
            if (r.getWidth() < 80)
                break;
            auto item = r.removeFromLeft (90);
            const auto lineArea = item.removeFromLeft (26).toFloat();
            juce::Path line;
            line.startNewSubPath (lineArea.getX(), lineArea.getCentreY());
            line.lineTo (lineArea.getRight() - 6.0f, lineArea.getCentreY());
            juce::Path stroke;
            juce::PathStrokeType type (s == theory::MotionStrength::Strong ? 2.6f : 1.8f);
            if (s == theory::MotionStrength::Deceptive)
            {
                const float dashes[] { 5.0f, 3.0f };
                type.createDashedStroke (stroke, line, dashes, 2);
            }
            else
            {
                type.createStrokedPath (stroke, line);
            }
            g.setColour (strengthColour (s));
            g.fillPath (stroke);
            juce::Path head;
            const float hx = lineArea.getRight(), hy = lineArea.getCentreY();
            head.addTriangle (hx, hy, hx - 7.0f, hy - 4.0f, hx - 7.0f, hy + 4.0f);
            g.fillPath (head);

            g.setColour (colours::textDim);
            g.drawText (strengthLabel (s), item.withTrimmedLeft (4), juce::Justification::centredLeft);
        }
    }
}

void TonalMapComponent::paintCanvas (juce::Graphics& g)
{
    g.fillAll (colours::background);

    // Banner: T -> SD -> D, and back to T (resolution).
    {
        const auto mid = [&] (int i) { return static_cast<float> (columns[(std::size_t) i].bounds.getCentreX()); };
        const float y  = static_cast<float> (bannerArea.getBottom()) - 10.0f;
        const float yTop = static_cast<float> (bannerArea.getY()) + 8.0f;
        const auto tc = functionColour (theory::HarmonicFunction::Tonic);
        const auto sc = functionColour (theory::HarmonicFunction::Subdominant);
        const auto dc = functionColour (theory::HarmonicFunction::Dominant);

        auto straight = [&] (float x1, float x2, juce::Colour c1, juce::Colour c2)
        {
            juce::Path p;
            p.addArrow ({ x1, y, x2, y }, 2.0f, 10.0f, 9.0f);
            g.setGradientFill (juce::ColourGradient (c1.withAlpha (0.8f), x1, y, c2.withAlpha (0.8f), x2, y, false));
            g.fillPath (p);
        };
        straight (mid (0) + 60.0f, mid (1) - 60.0f, tc, sc);
        straight (mid (1) + 60.0f, mid (2) - 60.0f, sc, dc);

        juce::Path back;
        back.startNewSubPath (mid (2), y - 6.0f);
        back.cubicTo (mid (2), yTop - 4.0f, mid (0), yTop - 4.0f, mid (0), y - 6.0f);
        juce::Path stroked;
        const float dashes[] { 6.0f, 4.0f };
        juce::PathStrokeType (1.6f).createDashedStroke (stroked, back, dashes, 2);
        g.setGradientFill (juce::ColourGradient (dc.withAlpha (0.7f), mid (2), y, tc.withAlpha (0.7f), mid (0), y, false));
        g.fillPath (stroked);
        juce::Path head;
        head.addTriangle (mid (0), y - 1.0f, mid (0) - 5.0f, y - 10.0f, mid (0) + 5.0f, y - 10.0f);
        g.setColour (tc.withAlpha (0.8f));
        g.fillPath (head);

        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (11.0f).withStyle ("Italic"));
        g.drawText (juce::String::fromUTF8 ("résolution"),
                    juce::Rectangle<float> (mid (1) - 60.0f, yTop + 2.0f, 120.0f, 14.0f), juce::Justification::centred);
    }

    for (const auto& column : columns)
    {
        const auto colour = functionColour (column.function);
        const auto r = column.bounds.toFloat();

        g.setGradientFill (juce::ColourGradient (colours::panel.interpolatedWith (colour, 0.10f), r.getX(), r.getY(),
                                                 colours::panel.withAlpha (0.55f), r.getX(), r.getY() + 260.0f, false));
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (colour.withAlpha (0.28f));
        g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);

        // Header: badge + name + role.
        auto header = column.bounds.reduced (kColumnPadding, 0).withHeight (kHeaderHeight).withTrimmedTop (6);
        const auto badge = header.removeFromLeft (38).withSizeKeepingCentre (34, 34).toFloat();
        g.setColour (colour);
        g.fillEllipse (badge);
        g.setColour (colours::background);
        g.setFont (juce::FontOptions (column.function == theory::HarmonicFunction::Subdominant ? 13.0f : 16.0f, juce::Font::bold));
        g.drawText (theory::functionName (column.function, true), badge, juce::Justification::centred);

        header.removeFromLeft (6);
        g.setColour (colour);
        g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
        g.drawText (functionLabel (column.function), header.removeFromTop (header.getHeight() / 2 + 2),
                    juce::Justification::bottomLeft);
        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (11.5f));
        g.drawText (functionRole (column.function), header, juce::Justification::topLeft);

        // Group labels above the first cell of each origin, with a hairline.
        std::optional<theory::ChordOrigin> currentOrigin;
        for (const auto* cell : column.cells)
        {
            const auto origin = cell->getChord().origin;
            if (origin == currentOrigin)
                continue;
            currentOrigin = origin;

            auto labelArea = juce::Rectangle<int> (column.bounds.getX() + kColumnPadding, cell->getY() - kGroupHeight,
                                                   column.bounds.getWidth() - 2 * kColumnPadding, kGroupHeight - 4);
            const auto text = originLabel (origin);
            const auto font = juce::FontOptions (9.5f, juce::Font::bold).withKerningFactor (0.08f);
            g.setFont (font);
            const int textWidth = juce::GlyphArrangement::getStringWidthInt (juce::Font (font), text) + 8;
            g.setColour (colours::textDim);
            g.drawText (text, labelArea, juce::Justification::bottomLeft);
            g.setColour (colours::dim.withAlpha (0.8f));
            g.drawHorizontalLine (labelArea.getBottom() - 4, static_cast<float> (labelArea.getX() + textWidth),
                                  static_cast<float> (labelArea.getRight()));
        }
    }

    // Glow under the chords of the progression.
    for (const auto& cell : cells)
        if (cell->isHighlighted() && cell->isVisible())
        {
            juce::Path p;
            p.addRoundedRectangle (cell->getBounds().toFloat(), 8.0f);
            juce::DropShadow (functionColour (cell->getChord().function).withAlpha (0.55f), 14, {}).drawForPath (g, p);
        }
}

void TonalMapComponent::paintOverlay (juce::Graphics& g)
{
    auto boundsOf = [this] (const ChordCell* c) { return c->getBounds().toFloat(); };

    // A chosen progression: numbered path.
    if (const auto path = currentPattern(); ! path.empty())
    {
        std::vector<ChordCell*> pathCells;
        for (const auto& c : path)
            pathCells.push_back (findCell (c));

        for (std::size_t i = 1; i < pathCells.size(); ++i)
            if (pathCells[i - 1] != nullptr && pathCells[i] != nullptr && pathCells[i - 1] != pathCells[i])
                drawCurvedArrow (g, boundsOf (pathCells[i - 1]), boundsOf (pathCells[i]), kPath, 3.0f, false, 0.18f);

        // Numbers (a chord used twice gets both, side by side).
        std::map<ChordCell*, int> usedOnCell;
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        for (std::size_t i = 0; i < pathCells.size(); ++i)
        {
            auto* cell = pathCells[i];
            if (cell == nullptr)
                continue;
            const int k = usedOnCell[cell]++;
            const auto r = boundsOf (cell);
            const auto badge = juce::Rectangle<float> (19.0f, 19.0f)
                                   .withCentre ({ r.getX() + 4.0f + 21.0f * static_cast<float> (k), r.getY() + 3.0f });
            g.setColour (colours::background);
            g.fillEllipse (badge.expanded (2.0f));
            g.setColour (kPath);
            g.fillEllipse (badge);
            g.setColour (colours::background);
            g.drawText (juce::String (static_cast<int> (i) + 1), badge, juce::Justification::centred);
        }

        return;
    }

    if (! motionsToggle.getToggleState())
        return;

    // Hovered chord, else the last chord of the progression: where can it go?
    const ChordCell* source = hoveredCell;
    float alpha = 1.0f;
    if (source == nullptr && ! selection.empty())
    {
        source = findCell (selection.back());
        alpha = 0.55f;
    }
    if (source == nullptr)
        return;

    // Weak arrows first so the strong ones stay on top.
    auto motions = motionsFrom (source->getChord());
    std::stable_sort (motions.begin(), motions.end(), [] (const auto& a, const auto& b)
                      { return static_cast<int> (a.second) > static_cast<int> (b.second); });

    for (const auto& [cell, strength] : motions)
        drawCurvedArrow (g, boundsOf (source), boundsOf (cell), strengthColour (strength).withMultipliedAlpha (alpha),
                         strength == theory::MotionStrength::Strong ? 2.6f : 1.8f,
                         strength == theory::MotionStrength::Deceptive);

    // Outline the source.
    g.setColour (juce::Colours::white.withAlpha (0.8f * alpha));
    g.drawRoundedRectangle (boundsOf (source).expanded (2.0f), 9.0f, 1.5f);
}

} // namespace mbox
