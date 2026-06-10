#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

/**
    The 8-step rate sequencer.

    Each cell is one beat and shows the LFO rate for that step ("—" = rest:
    the filter stays open). Click a cell to cycle to the next rate, right-click
    to cycle back, drag up/down or scroll to dial one in. The cell currently
    playing lights up. Steps beyond the pattern length are dimmed.
*/
class PatternStrip : public juce::Component,
                     public juce::SettableTooltipClient,
                     private juce::Timer
{
public:
    explicit PatternStrip (K4WobbleProcessor&);
    ~PatternStrip() override;

    /** Fired on any user edit (so the editor can drop the preset to Custom). */
    std::function<void()> onUserChange;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override;
    int  cellAt (juce::Point<int>) const;
    juce::Rectangle<float> cellBounds (int index) const;

    static int numChoices() { return wobble::numDivisions + 1; } // divisions + Rest

    K4WobbleProcessor& processor;

    struct Cell
    {
        juce::RangedAudioParameter* param = nullptr;
        std::unique_ptr<juce::ParameterAttachment> attachment;
        int value = 4;
    };

    std::array<Cell, wobble::maxSteps> cells;

    std::atomic<float>* patternParam = nullptr;
    std::atomic<float>* stepsParam   = nullptr;

    bool patternOn   = false;
    int  activeSteps = 8;
    int  playStep    = -1;

    int  dragCell = -1;
    int  dragStartValue = 0;
    bool dragging = false;
    juce::Point<int> dragStartPos;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatternStrip)
};
