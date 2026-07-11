#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

/**
    The 16-step sequencer strip.

    Four lanes, one visible at a time: Rate (which LFO division each step
    uses, or a rest), Depth (scales the sweep per step), Cutoff (shifts the
    filter per step) and Shape (overrides the LFO shape per step). In simple
    mode only the Rate lane exists — the tabs appear with the Advanced view.

    Choice lanes: click cycles forward, right-click back, drag/scroll to dial.
    Continuous lanes: drag up/down or scroll; right-click resets the step.
    The cell currently playing lights up; steps beyond the length are dimmed.
*/
class PatternStrip : public juce::Component,
                     public juce::SettableTooltipClient,
                     private juce::Timer
{
public:
    enum class Lane { rate = 0, depth, cutoff, shape };

    explicit PatternStrip (K4WobbleProcessor&);
    ~PatternStrip() override;

    /** Fired on any user edit (so the editor can drop the preset to Custom). */
    std::function<void()> onUserChange;

    /** Advanced mode shows the lane tabs; simple mode pins the Rate lane. */
    void setAdvanced (bool shouldShowLanes);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override;

    struct Cell
    {
        juce::RangedAudioParameter* param = nullptr;
        std::unique_ptr<juce::ParameterAttachment> attachment;
        float value = 0.0f;
    };

    Cell&       cellFor (Lane, int step);
    const Cell& cellFor (Lane, int step) const;
    void  buildLane (Lane, const juce::String& paramPrefixId);

    int   cellAt (juce::Point<int>) const;
    bool  inTabRow (juce::Point<int>) const;
    juce::Rectangle<float> cellBounds (int index) const;
    juce::Rectangle<float> tabBounds (int tab) const;
    float cellsTop() const;

    bool  laneIsChoice() const   { return lane == Lane::rate || lane == Lane::shape; }
    int   laneNumChoices() const;
    float laneMin() const        { return lane == Lane::cutoff ? -100.0f : 0.0f; }
    float laneMax() const        { return 100.0f; }
    float laneDefault() const    { return lane == Lane::depth ? 100.0f : 0.0f; }

    K4WobbleProcessor& processor;

    std::array<Cell, wobble::maxSteps> rateCells, depthCells, cutCells, shapeCells;

    std::atomic<float>* patternParam = nullptr;
    std::atomic<float>* stepsParam   = nullptr;

    Lane lane        = Lane::rate;
    bool advanced    = false;
    bool patternOn   = false;
    int  activeSteps = 8;
    int  playStep    = -1;

    int   dragCell = -1;
    float dragStartValue = 0.0f;
    bool  dragging = false;
    juce::Point<int> dragStartPos;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatternStrip)
};
