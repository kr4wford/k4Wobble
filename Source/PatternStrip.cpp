#include "PatternStrip.h"
#include "ParamIDs.h"
#include <cmath>

namespace
{
    constexpr float tabRowHeight = 17.0f;

    const char* const laneNames[4] = { "Rate", "Depth", "Cutoff", "Shape" };

    // Short names that fit a 16-cell grid.
    const char* const shapeAbbrev[7] = { "-", "Sin", "Tri", "SwD", "SwU", "Sqr", "Rnd" };
}

PatternStrip::PatternStrip (K4WobbleProcessor& p) : processor (p)
{
    buildLane (Lane::rate,   "step");
    buildLane (Lane::depth,  "dep");
    buildLane (Lane::cutoff, "cut");
    buildLane (Lane::shape,  "shp");

    patternParam = processor.apvts.getRawParameterValue (ids::pattern);
    stepsParam   = processor.apvts.getRawParameterValue (ids::steps);

    startTimerHz (20);
}

PatternStrip::~PatternStrip()
{
    stopTimer();
}

PatternStrip::Cell& PatternStrip::cellFor (Lane l, int step)
{
    switch (l)
    {
        case Lane::depth:  return depthCells[(size_t) step];
        case Lane::cutoff: return cutCells[(size_t) step];
        case Lane::shape:  return shapeCells[(size_t) step];
        case Lane::rate:
        default:           return rateCells[(size_t) step];
    }
}

const PatternStrip::Cell& PatternStrip::cellFor (Lane l, int step) const
{
    return const_cast<PatternStrip*> (this)->cellFor (l, step);
}

void PatternStrip::buildLane (Lane l, const juce::String& prefix)
{
    for (int i = 0; i < wobble::maxSteps; ++i)
    {
        auto& cell = cellFor (l, i);
        cell.param = processor.apvts.getParameter (prefix + juce::String (i + 1));
        jassert (cell.param != nullptr);

        cell.attachment = std::make_unique<juce::ParameterAttachment> (
            *cell.param,
            [this, l, i] (float newValue)
            {
                cellFor (l, i).value = newValue;
                if (l == lane)
                    repaint();
            });
        cell.attachment->sendInitialUpdate();
    }
}

int PatternStrip::laneNumChoices() const
{
    if (lane == Lane::rate)
        return wobble::numDivisions + 1;   // + Rest
    return 7;                              // Global + 6 shapes
}

void PatternStrip::setAdvanced (bool shouldShowLanes)
{
    if (advanced == shouldShowLanes)
        return;
    advanced = shouldShowLanes;
    if (! advanced)
        lane = Lane::rate;
    repaint();
}

void PatternStrip::timerCallback()
{
    const bool on    = patternParam->load() > 0.5f;
    const int  steps = 1 + (int) stepsParam->load();
    const int  play  = processor.engine.getCurrentStep();

    if (on != patternOn || steps != activeSteps || play != playStep)
    {
        patternOn   = on;
        activeSteps = steps;
        playStep    = play;
        repaint();
    }
}

float PatternStrip::cellsTop() const
{
    return advanced ? tabRowHeight + 2.0f : 0.0f;
}

juce::Rectangle<float> PatternStrip::tabBounds (int tab) const
{
    const float w = (float) getWidth() / 4.0f;
    return { w * (float) tab, 0.0f, w, tabRowHeight };
}

juce::Rectangle<float> PatternStrip::cellBounds (int index) const
{
    const float top = cellsTop();
    const float w   = (float) getWidth() / (float) wobble::maxSteps;
    return { w * (float) index, top, w, (float) getHeight() - top };
}

int PatternStrip::cellAt (juce::Point<int> pos) const
{
    if (! getLocalBounds().contains (pos) || getWidth() == 0 || (float) pos.y < cellsTop())
        return -1;
    return juce::jlimit (0, wobble::maxSteps - 1,
                         pos.x * wobble::maxSteps / getWidth());
}

bool PatternStrip::inTabRow (juce::Point<int> pos) const
{
    return advanced && (float) pos.y < tabRowHeight && getLocalBounds().contains (pos);
}

void PatternStrip::mouseDown (const juce::MouseEvent& e)
{
    if (inTabRow (e.getPosition()))
    {
        const int tab = juce::jlimit (0, 3, e.getPosition().x * 4 / juce::jmax (1, getWidth()));
        if ((Lane) tab != lane)
        {
            lane = (Lane) tab;
            repaint();
        }
        dragCell = -1;
        return;
    }

    dragCell = cellAt (e.getPosition());
    dragging = false;
    if (dragCell >= 0)
    {
        dragStartValue = cellFor (lane, dragCell).value;
        dragStartPos   = e.getPosition();
    }
}

void PatternStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (dragCell < 0)
        return;

    const int dy = dragStartPos.y - e.getPosition().y; // up = more / faster
    if (! dragging && std::abs (dy) > 4)
    {
        dragging = true;
        cellFor (lane, dragCell).attachment->beginGesture();
    }

    if (! dragging)
        return;

    float value;
    if (laneIsChoice())
        value = (float) juce::jlimit (0, laneNumChoices() - 1,
                                      (int) dragStartValue + dy / 12);
    else
        value = juce::jlimit (laneMin(), laneMax(),
                              dragStartValue + (float) dy * (laneMax() - laneMin()) / 160.0f);

    cellFor (lane, dragCell).attachment->setValueAsPartOfGesture (value);
    if (onUserChange)
        onUserChange();
}

void PatternStrip::mouseUp (const juce::MouseEvent& e)
{
    if (dragCell < 0)
        return;

    auto& cell = cellFor (lane, dragCell);

    if (dragging)
    {
        cell.attachment->endGesture();
    }
    else if (laneIsChoice())
    {
        // Plain click cycles forward through the choices; right-click back.
        const int dir   = e.mods.isPopupMenu() ? -1 : 1;
        const int num   = laneNumChoices();
        const int value = (((int) cell.value + dir) % num + num) % num;
        cell.attachment->setValueAsCompleteGesture ((float) value);
        if (onUserChange)
            onUserChange();
    }
    else if (e.mods.isPopupMenu())
    {
        // Right-click resets a continuous step to its neutral value.
        cell.attachment->setValueAsCompleteGesture (laneDefault());
        if (onUserChange)
            onUserChange();
    }

    dragCell = -1;
    dragging = false;
}

void PatternStrip::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int idx = cellAt (e.getPosition());
    if (idx < 0 || wheel.deltaY == 0.0f)
        return;

    auto& cell = cellFor (lane, idx);
    const float direction = wheel.deltaY > 0.0f ? 1.0f : -1.0f;

    float value;
    if (laneIsChoice())
        value = (float) juce::jlimit (0, laneNumChoices() - 1, (int) cell.value + (int) direction);
    else
        value = juce::jlimit (laneMin(), laneMax(), cell.value + direction * 5.0f);

    if (! juce::exactlyEqual (value, cell.value))
    {
        cell.attachment->setValueAsCompleteGesture (value);
        if (onUserChange)
            onUserChange();
    }
}

void PatternStrip::paint (juce::Graphics& g)
{
    const float dim = patternOn ? 1.0f : 0.4f;

    if (advanced)
    {
        for (int t = 0; t < 4; ++t)
        {
            auto r = tabBounds (t).reduced (2.0f, 1.0f);
            const bool selected = ((Lane) t == lane);
            g.setColour (juce::Colour (selected ? 0xff35517a : 0xff1a1626)
                             .withAlpha ((selected ? 0.95f : 0.6f) * dim));
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (juce::Colours::white.withAlpha ((selected ? 0.9f : 0.45f) * dim));
            g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
            g.drawText (laneNames[t], r, juce::Justification::centred);
        }
    }

    const float fontSize = wobble::maxSteps > 8 ? 10.0f : 12.0f;

    for (int i = 0; i < wobble::maxSteps; ++i)
    {
        auto r = cellBounds (i).reduced (1.5f, 2.0f);
        const bool  inactive = (i >= activeSteps);
        const bool  playing  = patternOn && (i == playStep) && ! inactive;
        const auto& cell     = cellFor (lane, i);

        auto fill = playing ? juce::Colour (0xff35517a) : juce::Colour (0xff1d1830);
        g.setColour (fill.withAlpha ((inactive ? 0.35f : 0.85f) * dim));
        g.fillRoundedRectangle (r, 3.0f);

        // Continuous lanes draw a value bar behind the text.
        if (lane == Lane::depth)
        {
            const float f = juce::jlimit (0.0f, 1.0f, cell.value / 100.0f);
            auto bar = r.reduced (2.0f);
            bar = bar.removeFromBottom (bar.getHeight() * f);
            g.setColour (juce::Colour (0xff4cc2ff).withAlpha (0.30f * dim));
            g.fillRoundedRectangle (bar, 2.0f);
        }
        else if (lane == Lane::cutoff)
        {
            const float f = juce::jlimit (-1.0f, 1.0f, cell.value / 100.0f);
            auto bar = r.reduced (2.0f);
            const float midY = bar.getCentreY();
            const float h    = bar.getHeight() * 0.5f * std::abs (f);
            g.setColour ((f >= 0.0f ? juce::Colour (0xff4cc2ff) : juce::Colour (0xff6b2fd6))
                             .withAlpha (0.35f * dim));
            g.fillRect (juce::Rectangle<float> (bar.getX(), f >= 0.0f ? midY - h : midY,
                                                bar.getWidth(), h));
        }

        g.setColour ((playing ? juce::Colour (0xff4cc2ff) : juce::Colours::white.withAlpha (0.12f))
                         .withAlpha ((playing ? 0.9f : 0.12f) * dim));
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, playing ? 1.5f : 1.0f);

        juce::String text;
        bool faint = false;
        switch (lane)
        {
            case Lane::rate:
                if ((int) cell.value >= wobble::numDivisions)
                {
                    text  = juce::String::fromUTF8 ("\xE2\x80\x94"); // em dash = rest
                    faint = true;
                }
                else
                {
                    text = wobble::divisions[(int) cell.value].name;
                }
                break;
            case Lane::depth:  text = juce::String ((int) cell.value);            break;
            case Lane::cutoff: text = juce::String ((int) cell.value);            break;
            case Lane::shape:
                text  = shapeAbbrev[juce::jlimit (0, 6, (int) cell.value)];
                faint = ((int) cell.value == 0);
                break;
        }

        g.setColour (juce::Colours::white.withAlpha ((inactive ? 0.25f : (faint ? 0.45f : 0.85f)) * dim));
        g.setFont (juce::FontOptions (fontSize, juce::Font::bold));
        g.drawText (text, r, juce::Justification::centred);
    }
}
