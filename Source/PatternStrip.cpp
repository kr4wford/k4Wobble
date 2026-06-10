#include "PatternStrip.h"
#include <cmath>

PatternStrip::PatternStrip (K4WobbleProcessor& p) : processor (p)
{
    for (int i = 0; i < wobble::maxSteps; ++i)
    {
        auto& cell = cells[(size_t) i];
        cell.param = processor.apvts.getParameter ("step" + juce::String (i + 1));
        jassert (cell.param != nullptr);

        cell.attachment = std::make_unique<juce::ParameterAttachment> (
            *cell.param,
            [this, i] (float newValue)
            {
                cells[(size_t) i].value = (int) std::lround (newValue);
                repaint();
            });
        cell.attachment->sendInitialUpdate();
    }

    patternParam = processor.apvts.getRawParameterValue ("pattern");
    stepsParam   = processor.apvts.getRawParameterValue ("steps");

    startTimerHz (20);
}

PatternStrip::~PatternStrip()
{
    stopTimer();
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

juce::Rectangle<float> PatternStrip::cellBounds (int index) const
{
    const float w = (float) getWidth() / (float) wobble::maxSteps;
    return { w * (float) index, 0.0f, w, (float) getHeight() };
}

int PatternStrip::cellAt (juce::Point<int> pos) const
{
    if (! getLocalBounds().contains (pos) || getWidth() == 0)
        return -1;
    return juce::jlimit (0, wobble::maxSteps - 1,
                         pos.x * wobble::maxSteps / getWidth());
}

void PatternStrip::mouseDown (const juce::MouseEvent& e)
{
    dragCell = cellAt (e.getPosition());
    dragging = false;
    if (dragCell >= 0)
    {
        dragStartValue = cells[(size_t) dragCell].value;
        dragStartPos   = e.getPosition();
    }
}

void PatternStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (dragCell < 0)
        return;

    const int dy = dragStartPos.y - e.getPosition().y; // up = faster
    if (! dragging && std::abs (dy) > 4)
    {
        dragging = true;
        cells[(size_t) dragCell].attachment->beginGesture();
    }

    if (dragging)
    {
        const int value = juce::jlimit (0, numChoices() - 1, dragStartValue + dy / 12);
        cells[(size_t) dragCell].attachment->setValueAsPartOfGesture ((float) value);
        if (onUserChange)
            onUserChange();
    }
}

void PatternStrip::mouseUp (const juce::MouseEvent& e)
{
    if (dragCell < 0)
        return;

    auto& cell = cells[(size_t) dragCell];

    if (dragging)
    {
        cell.attachment->endGesture();
    }
    else
    {
        // Plain click cycles forward through the rates; right-click cycles back.
        const int dir   = e.mods.isPopupMenu() ? -1 : 1;
        const int value = (cell.value + dir + numChoices()) % numChoices();
        cell.attachment->setValueAsCompleteGesture ((float) value);
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

    auto& cell = cells[(size_t) idx];
    const int value = juce::jlimit (0, numChoices() - 1,
                                    cell.value + (wheel.deltaY > 0.0f ? 1 : -1));
    if (value != cell.value)
    {
        cell.attachment->setValueAsCompleteGesture ((float) value);
        if (onUserChange)
            onUserChange();
    }
}

void PatternStrip::paint (juce::Graphics& g)
{
    const float dim = patternOn ? 1.0f : 0.4f;

    for (int i = 0; i < wobble::maxSteps; ++i)
    {
        auto r = cellBounds (i).reduced (2.5f, 2.0f);
        const bool inactive = (i >= activeSteps);
        const bool playing  = patternOn && (i == playStep) && ! inactive;
        const int  value    = cells[(size_t) i].value;
        const bool rest     = (value >= wobble::numDivisions);

        auto fill = playing ? juce::Colour (0xff35517a) : juce::Colour (0xff1d1830);
        g.setColour (fill.withAlpha ((inactive ? 0.35f : 0.85f) * dim));
        g.fillRoundedRectangle (r, 4.0f);

        g.setColour ((playing ? juce::Colour (0xff4cc2ff) : juce::Colours::white.withAlpha (0.12f))
                         .withAlpha ((playing ? 0.9f : 0.12f) * dim));
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, playing ? 1.5f : 1.0f);

        const auto text = rest ? juce::String::fromUTF8 ("\xE2\x80\x94") // em dash
                               : juce::String (wobble::divisions[value].name);
        g.setColour (juce::Colours::white.withAlpha ((inactive ? 0.25f : (rest ? 0.45f : 0.85f)) * dim));
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (text, r, juce::Justification::centred);
    }
}
