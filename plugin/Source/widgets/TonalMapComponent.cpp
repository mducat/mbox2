#include "TonalMapComponent.h"
#include "Widget.h"

namespace mbox
{

namespace
{
    const juce::Colour kBackground = colours::background;
    const juce::Colour kPanel      = colours::panel;
    const juce::Colour kDim        = colours::dim;
    const juce::Colour kText       = colours::text;

    juce::String originLabel (theory::ChordOrigin o)
    {
        switch (o)
        {
            case theory::ChordOrigin::Diatonic:  return juce::String::fromUTF8 ("Diatoniques");
            case theory::ChordOrigin::Secondary: return juce::String::fromUTF8 ("Dominantes secondaires (V/x)");
            case theory::ChordOrigin::Borrowed:  return juce::String::fromUTF8 ("Empruntés au mode parallèle");
            case theory::ChordOrigin::Chromatic: return juce::String::fromUTF8 ("Chromatiques");
            case theory::ChordOrigin::Cadential: return juce::String::fromUTF8 ("Cadence");
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

    bool sameChord (const theory::RomanChord& a, const theory::RomanChord& b)
    {
        return a.withInversion (0) == b.withInversion (0);
    }

    constexpr int kToolbarHeight = 32;
    constexpr int kArrowHeight   = 34;
    constexpr int kHeaderHeight  = 30;
    constexpr int kGroupHeight   = 18;
    constexpr int kCellWidth     = 78;
    constexpr int kCellHeight    = 46;
    constexpr int kGap           = 6;
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

    void setHighlighted (bool h)
    {
        if (highlighted != h)
        {
            highlighted = h;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto colour = functionColour (chord.function);
        const auto r = getLocalBounds().toFloat().reduced (1.0f);
        const bool hover = isMouseOver();
        const bool diatonic = chord.origin == theory::ChordOrigin::Diatonic;

        g.setColour (highlighted ? colour : (hover ? kDim : kPanel));
        g.fillRoundedRectangle (r, 6.0f);

        g.setColour (colour.withAlpha (diatonic ? 1.0f : 0.55f));
        if (diatonic)
            g.drawRoundedRectangle (r, 6.0f, 1.5f);
        else
        {
            // Non-diatonic chords get a dashed outline.
            juce::Path outline;
            outline.addRoundedRectangle (r, 6.0f);
            juce::Path dashed;
            const float dashes[] { 4.0f, 3.0f };
            juce::PathStrokeType (1.2f).createDashedStroke (dashed, outline, dashes, 2);
            g.fillPath (dashed);
        }

        auto text = getLocalBounds().reduced (3, 2);
        g.setColour (highlighted ? kBackground : kText);
        g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        g.drawFittedText (juce::String (chord.numeral()), text.removeFromTop (text.getHeight() * 3 / 5),
                          juce::Justification::centred, 1, 0.6f);

        g.setColour (highlighted ? kBackground.withAlpha (0.8f) : juce::Colours::grey);
        g.setFont (juce::FontOptions (12.0f));
        g.drawFittedText (juce::String (chord.symbol()), text, juce::Justification::centred, 1, 0.6f);
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

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
};

class TonalMapComponent::Canvas final : public juce::Component
{
public:
    explicit Canvas (TonalMapComponent& o) : owner (o) {}
    void paint (juce::Graphics& g) override { owner.paintCanvas (g); }

private:
    TonalMapComponent& owner;
};

// ---------------------------------------------------------------------------
// Map
// ---------------------------------------------------------------------------

TonalMapComponent::TonalMapComponent()
    : canvas (std::make_unique<Canvas> (*this))
{
    viewport.setViewedComponent (canvas.get(), false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    columns[0].function = theory::HarmonicFunction::Tonic;
    columns[1].function = theory::HarmonicFunction::Subdominant;
    columns[2].function = theory::HarmonicFunction::Dominant;

    keyLabel.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    keyLabel.setColour (juce::Label::textColourId, kText);
    keyLabel.setText (keyDisplayName (key), juce::dontSendNotification);
    addAndMakeVisible (keyLabel);

    auto setupToggle = [this] (juce::ToggleButton& b, const char* text, bool& flag)
    {
        b.setButtonText (juce::String::fromUTF8 (text));
        b.setToggleState (flag, juce::dontSendNotification);
        b.onClick = [this, &b, &flag]
        {
            flag = b.getToggleState();
            rebuild();
        };
        addAndMakeVisible (b);
    };
    setupToggle (seventhsToggle,  "Septièmes",    options.sevenths);
    setupToggle (secondaryToggle, "Secondaires",  options.secondaryDominants);
    setupToggle (borrowedToggle,  "Emprunts",     options.borrowedChords);
    setupToggle (chromaticToggle, "Chromatiques", options.chromaticChords);

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
}

void TonalMapComponent::chooseChord (const theory::RomanChord& chord)
{
    if (onChordChosen)
        onChordChosen (chord);
}

void TonalMapComponent::rebuild()
{
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

    updateHighlights();
    resized();
    canvas->repaint();
}

void TonalMapComponent::updateHighlights()
{
    for (auto& cell : cells)
    {
        const auto& c = cell->getChord();
        cell->setHighlighted (std::any_of (selection.begin(), selection.end(),
                                           [&] (const theory::RomanChord& s) { return sameChord (s, c); }));
    }
}

void TonalMapComponent::resized()
{
    auto area = getLocalBounds().reduced (10);

    {
        auto bar = area.removeFromTop (kToolbarHeight);
        keyLabel.setBounds (bar.removeFromLeft (160));
        bar.removeFromLeft (kGap);
        for (auto* t : { &seventhsToggle, &secondaryToggle, &borrowedToggle, &chromaticToggle })
            t->setBounds (bar.removeFromLeft (110));
    }

    viewport.setBounds (area);

    const int width = area.getWidth() - viewport.getScrollBarThickness();
    auto content = juce::Rectangle<int> (0, 0, width, 0);
    arrowArea = content.withHeight (kArrowHeight);

    const int columnWidth = (width - 2 * kGap) / 3;
    int bottom = kArrowHeight;
    int x = 0;
    for (auto& column : columns)
    {
        column.bounds = { x, kArrowHeight, columnWidth, 0 };
        x += columnWidth + kGap;
        bottom = juce::jmax (bottom, layoutColumn (column));
    }

    // Columns share the same height: the viewport height, or the tallest content.
    const int height = juce::jmax (bottom + kGap, area.getHeight());
    for (auto& column : columns)
        column.bounds.setBottom (height - 1);

    canvas->setSize (width, height);
}

int TonalMapComponent::layoutColumn (Column& column)
{
    auto inner = column.bounds.withHeight (10000).reduced (kGap).withTrimmedTop (kHeaderHeight);
    // Even number of cells per row so a triad and its seventh stay side by side.
    int perRow = juce::jmax (1, (inner.getWidth() + kGap) / (kCellWidth + kGap));
    if (perRow > 1)
        perRow -= perRow % 2;
    const int cellWidth = (inner.getWidth() - (perRow - 1) * kGap) / perRow;

    int y = inner.getY();
    int x = 0;
    std::optional<theory::ChordOrigin> currentOrigin;
    std::optional<int> previousTarget;

    for (auto* cell : column.cells)
    {
        const auto origin = cell->getChord().origin;
        if (origin != currentOrigin)
        {
            if (currentOrigin)
                y += kCellHeight + kGap;
            y += kGroupHeight;
            x = 0;
            currentOrigin = origin;
        }
        else if (x == perRow || (origin == theory::ChordOrigin::Secondary && x > 0
                                 && cell->getChord().targetDegree != previousTarget))
        {
            // Rows wrap when full, and each group of applied chords (V/x, V7/x, vii°7/x) starts a row.
            y += kCellHeight + kGap;
            x = 0;
        }

        cell->setBounds (inner.getX() + x * (cellWidth + kGap), y, cellWidth, kCellHeight);
        previousTarget = cell->getChord().targetDegree;
        ++x;
    }

    return column.cells.empty() ? inner.getY() : y + kCellHeight + kGap;
}

void TonalMapComponent::paint (juce::Graphics& g)
{
    g.fillAll (kBackground);
}

void TonalMapComponent::paintCanvas (juce::Graphics& g)
{
    g.fillAll (kBackground);

    // Usual progression: T -> SD -> D -> T (and T -> D directly).
    {
        const auto mid = [&] (int i) { return static_cast<float> (columns[(std::size_t) i].bounds.getCentreX()); };
        const float yLow  = static_cast<float> (arrowArea.getBottom()) - 6.0f;
        const float yHigh = static_cast<float> (arrowArea.getY()) + 6.0f;

        g.setColour (kDim.brighter (0.4f));
        const auto arrow = [&] (float x1, float x2)
        {
            juce::Path p;
            p.addArrow ({ x1, yLow, x2, yLow }, 1.5f, 9.0f, 8.0f);
            g.fillPath (p);
        };
        arrow (mid (0) + 40.0f, mid (1) - 40.0f); // T -> SD
        arrow (mid (1) + 40.0f, mid (2) - 40.0f); // SD -> D

        juce::Path back;
        back.startNewSubPath (mid (2), yLow - 6.0f);
        back.cubicTo (mid (2), yHigh, mid (0), yHigh, mid (0), yLow - 6.0f);
        juce::Path stroked;
        const float dashes[] { 5.0f, 4.0f };
        juce::PathStrokeType (1.5f).createDashedStroke (stroked, back, dashes, 2);
        g.fillPath (stroked);
        juce::Path head;
        head.addArrow ({ mid (0), yLow - 12.0f, mid (0), yLow - 1.0f }, 1.5f, 9.0f, 8.0f);
        g.fillPath (head);

        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("resolution", juce::Rectangle<float> (mid (1) - 50.0f, yLow - 13.0f, 100.0f, 14.0f),
                    juce::Justification::centred);
    }

    for (const auto& column : columns)
    {
        const auto colour = functionColour (column.function);
        const auto r = column.bounds.toFloat();

        g.setColour (kPanel.withAlpha (0.6f));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (colour.withAlpha (0.35f));
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

        auto header = column.bounds.reduced (kGap, 0).removeFromTop (kHeaderHeight + kGap);
        g.setColour (colour);
        g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        g.drawText (functionLabel (column.function) + "  (" + theory::functionName (column.function, true) + ")",
                    header, juce::Justification::centredLeft);

        // Group labels above the first cell of each origin.
        std::optional<theory::ChordOrigin> currentOrigin;
        g.setFont (juce::FontOptions (11.0f));
        for (const auto* cell : column.cells)
        {
            const auto origin = cell->getChord().origin;
            if (origin == currentOrigin)
                continue;
            currentOrigin = origin;
            g.setColour (juce::Colours::grey);
            g.drawText (originLabel (origin),
                        juce::Rectangle<int> (column.bounds.getX() + kGap, cell->getY() - kGroupHeight,
                                              column.bounds.getWidth() - 2 * kGap, kGroupHeight),
                        juce::Justification::centredLeft);
        }
    }
}

} // namespace mbox
