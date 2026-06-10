#include "WobbleScope.h"
#include <cmath>

namespace
{
    const juce::Colour closedColour { 0xff6b2fd6 };  // filter shut: deep violet
    const juce::Colour openColour   { 0xff4cc2ff };  // filter open: k4 cyan
    const juce::Colour traceColour  { 0xffff6ad5 };  // LFO trace
    const juce::Colour dryColour    { 0xff7b61ff };  // dry silhouette
}

WobbleScope::WobbleScope (K4WobbleProcessor& p) : processor (p)
{
    setOpaque (true);
    scratchDry.assign (4096, 0.0f);
    scratchWet.assign (4096, 0.0f);
    scratchMod.assign (4096, 1.0f);
    modVals.fill (1.0f);
    startTimerHz (60);
}

WobbleScope::~WobbleScope()
{
    stopTimer();
}

bool WobbleScope::hitTest (int, int y)
{
    return y < getHeight() - reservedBottom;
}

void WobbleScope::pushColumn (float dryPeak, float wetPeak, float mod)
{
    if (hasAudio[(size_t) head])
        --audioColumns;

    const bool audible = (dryPeak > silenceLevel || wetPeak > silenceLevel);
    hasAudio[(size_t) head] = audible;
    if (audible)
        ++audioColumns;

    dryPeaks[(size_t) head] = dryPeak;
    wetPeaks[(size_t) head] = wetPeak;
    modVals[(size_t) head]  = mod;
    head = (head + 1) % history;
}

void WobbleScope::timerCallback()
{
    if (paused)
        return;

    // Drain everything available, tracking this frame's peaks and the most
    // recent LFO position.
    float dPk = 0.0f, wPk = 0.0f;
    int got;
    do
    {
        got = processor.readVisualizerSamples (scratchDry.data(), scratchWet.data(),
                                               scratchMod.data(), (int) scratchDry.size());
        for (int i = 0; i < got; ++i)
        {
            dPk = juce::jmax (dPk, std::abs (scratchDry[(size_t) i]));
            wPk = juce::jmax (wPk, std::abs (scratchWet[(size_t) i]));
        }
        if (got > 0)
            lastMod = juce::jlimit (0.0f, 1.0f, scratchMod[(size_t) got - 1]);
    } while (got == (int) scratchDry.size());

    // One new column per frame keeps the scroll speed steady and the look clean.
    pushColumn (dPk, wPk, lastMod);

    // Once the last audible column has scrolled off, the display is static —
    // skip repainting entirely so an idle plugin costs nothing.
    const bool blank = (audioColumns == 0);
    if (! (blank && lastFrameBlank))
        repaint();
    lastFrameBlank = blank;
}

void WobbleScope::mouseDown (const juce::MouseEvent&)
{
    paused = ! paused;
    repaint();
}

void WobbleScope::mouseEnter (const juce::MouseEvent&)
{
    hovering = true;
    repaint();
}

void WobbleScope::mouseExit (const juce::MouseEvent&)
{
    hovering = false;
    repaint();
}

void WobbleScope::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // Background.
    g.setGradientFill (juce::ColourGradient (
        juce::Colour (0xff16101f), b.getCentreX(), b.getY(),
        juce::Colour (0xff0b0a10), b.getCentreX(), b.getBottom(), false));
    g.fillAll();

    const float mid   = b.getCentreY();
    const float halfH = b.getHeight() * 0.40f;
    const float w     = b.getWidth();

    // Centre line.
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawHorizontalLine ((int) mid, b.getX(), b.getRight());

    const auto valueAt = [this] (const std::array<float, history>& a, int col)
    {
        // col 0 = oldest (left), col history-1 = newest (right)
        const int idx = (head + col) % history;
        return juce::jlimit (0.0f, 1.0f, a[(size_t) idx]);
    };

    const auto audioAt = [this] (int col)
    {
        return hasAudio[(size_t) ((head + col) % history)];
    };

    const auto xAt = [&] (int col)
    {
        return b.getX() + (w * (float) col) / (float) (history - 1);
    };

    constexpr int drawnPoints = history / drawStep + 2;

    if (audioColumns > 0)
    {
        // Pre-scan so silent layers can be skipped wholesale.
        float maxWet = 0.0f, maxDry = 0.0f;
        for (int c = 0; c < history; c += drawStep)
        {
            maxWet = juce::jmax (maxWet, valueAt (wetPeaks, c));
            maxDry = juce::jmax (maxDry, valueAt (dryPeaks, c));
        }

        // Dry silhouette underneath (the un-wobbled input).
        if (maxDry > silenceLevel)
        {
            juce::Path dry;
            dry.preallocateSpace (drawnPoints * 6 + 8);
            dry.startNewSubPath (b.getX(), mid);
            for (int c = 0; c < history; c += drawStep)
                dry.lineTo (xAt (c), mid - valueAt (dryPeaks, c) * halfH);
            dry.lineTo (xAt (history - 1), mid - valueAt (dryPeaks, history - 1) * halfH);
            for (int c = history - 1; c >= 0; c -= drawStep)
                dry.lineTo (xAt (c), mid + valueAt (dryPeaks, c) * halfH);
            dry.closeSubPath();
            g.setColour (dryColour.withAlpha (0.13f));
            g.fillPath (dry);
        }

        const float currentMod = valueAt (modVals, history - 1);
        const auto  meshColour = closedColour.interpolatedWith (openColour, currentMod);

        // Vertical mesh lines, coloured by where the filter was at that moment.
        for (int c = 0; c < history; c += 8)
        {
            const float a = valueAt (wetPeaks, c);
            if (a < silenceLevel)
                continue;
            const float x = xAt (c);
            g.setColour (closedColour.interpolatedWith (openColour, valueAt (modVals, c))
                             .withAlpha (0.30f));
            g.drawLine (x, mid - a * halfH, x, mid + a * halfH, 1.0f);
        }

        // Horizontal mesh rows: the ribbon that breathes with the wet signal.
        // When the ribbon would collapse below ~a pixel, one line is enough.
        if (maxWet * halfH >= 1.5f)
        {
            for (int r = 0; r < rows; ++r)
            {
                const float frac = (float) r / (float) (rows - 1) * 2.0f - 1.0f; // -1..1
                juce::Path row;
                row.preallocateSpace (drawnPoints * 3 + 8);
                row.startNewSubPath (xAt (0), mid + frac * valueAt (wetPeaks, 0) * halfH);
                for (int c = drawStep; c < history; c += drawStep)
                    row.lineTo (xAt (c), mid + frac * valueAt (wetPeaks, c) * halfH);
                row.lineTo (xAt (history - 1), mid + frac * valueAt (wetPeaks, history - 1) * halfH);

                const bool edge = (r == 0 || r == rows - 1);
                g.setColour (meshColour.withAlpha (edge ? 0.85f : 0.20f + 0.40f * std::abs (frac)));
                g.strokePath (row, juce::PathStrokeType (edge ? 1.4f : 1.0f));
            }
        }
        else
        {
            g.setColour (meshColour.withAlpha (0.5f));
            g.drawHorizontalLine ((int) mid, b.getX(), b.getRight());
        }

        // The LFO trace (top = open), drawn only across stretches that carry
        // signal and kept above the control panel.
        {
            const float bandBottom = (float) (getHeight() - reservedBottom);
            const float bandHeight = bandBottom - 12.0f;

            juce::Path trace;
            trace.preallocateSpace (drawnPoints * 3 + 8);
            bool inRun = false;
            for (int c = 0; c < history; c += drawStep)
            {
                if (audioAt (c))
                {
                    const float y = bandBottom - 6.0f - valueAt (modVals, c) * bandHeight;
                    if (inRun)
                        trace.lineTo (xAt (c), y);
                    else
                        trace.startNewSubPath (xAt (c), y);
                    inRun = true;
                }
                else
                {
                    inRun = false;
                }
            }
            g.setColour (traceColour.withAlpha (0.85f));
            g.strokePath (trace, juce::PathStrokeType (1.5f));
        }
    }

    // Hover hint / pause indicator (above the control panel).
    const auto hintArea = getLocalBounds().withTrimmedBottom (reservedBottom)
                                          .reduced (10).removeFromBottom (22);
    if (paused)
    {
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (juce::String::fromUTF8 ("\xE2\x9D\x99\xE2\x9D\x99  paused"), // pause bars
                    hintArea, juce::Justification::centredRight);
    }
    else if (hovering)
    {
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.setFont (juce::FontOptions (12.0f));
        g.drawText ("click to pause", hintArea, juce::Justification::centredRight);
    }
}
