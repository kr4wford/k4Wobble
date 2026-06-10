#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

/**
    A scrolling 2D mesh waveform.

    The dry signal is a faint silhouette behind a mesh "ribbon" that breathes
    with the wet (wobbled) output — rows squeeze together as the filter chokes
    the sound and spread apart as it opens. The mesh colour shifts with the
    filter position (violet = closed, cyan = open), and a thin trace plots the
    LFO itself — but only while there's actually signal, so a silent track
    shows a clean, still display (and costs nothing to draw).
    Click anywhere on the visible scope area to pause / resume.
*/
class WobbleScope : public juce::Component,
                    public juce::SettableTooltipClient,
                    private juce::Timer
{
public:
    explicit WobbleScope (K4WobbleProcessor&);
    ~WobbleScope() override;

    /** Height of the control panel overlapping our bottom edge: clicks there
        fall through to the controls, and the hints/LFO trace stay above it. */
    void setReservedBottom (int pixels) { reservedBottom = pixels; }

    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void pushColumn (float dryPeak, float wetPeak, float mod);

    static constexpr int   history      = 480;   // columns of data kept
    static constexpr int   drawStep     = 2;     // draw every Nth column
    static constexpr int   rows         = 7;     // horizontal mesh lines
    static constexpr float silenceLevel = 0.001f; // -60 dBFS

    K4WobbleProcessor& processor;

    std::array<float, history> dryPeaks {};
    std::array<float, history> wetPeaks {};
    std::array<float, history> modVals {};
    std::array<bool,  history> hasAudio {};
    int  head = 0;             // ring index where the next column is written
    int  audioColumns = 0;     // how many columns currently hold audible signal
    bool lastFrameBlank = false;
    bool paused = false;
    bool hovering = false;
    float lastMod = 1.0f;
    int  reservedBottom = 0;

    std::vector<float> scratchDry, scratchWet, scratchMod;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WobbleScope)
};
