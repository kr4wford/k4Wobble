#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "WobbleScope.h"
#include "PatternStrip.h"

/** A TooltipWindow whose tips can be switched off outright.

    Raising the appear-delay is not enough to disable tips: while a tip is
    visible (or was hidden less than half a second ago) JUCE shows the next
    tip immediately, bypassing the delay — so once one tip appears, hovering
    keeps them "always on". Returning no tip text is the reliable off switch.
*/
struct GatedTooltipWindow : public juce::TooltipWindow
{
    explicit GatedTooltipWindow (juce::Component* parent) : juce::TooltipWindow (parent) {}

    std::function<bool()> tipsEnabled;

    juce::String getTipFor (juce::Component& c) override
    {
        return (tipsEnabled == nullptr || tipsEnabled()) ? juce::TooltipWindow::getTipFor (c)
                                                         : juce::String();
    }
};

class K4WobbleEditor : public juce::AudioProcessorEditor
{
public:
    explicit K4WobbleEditor (K4WobbleProcessor&);
    ~K4WobbleEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BoxAttachment    = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct LabeledKnob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void setupKnob (LabeledKnob&, const juce::String& paramID, const juce::String& text,
                    const juce::String& suffix, const juce::String& help);
    void setupBox (juce::ComboBox&, juce::Label&, std::unique_ptr<BoxAttachment>&,
                   const juce::String& paramID, const juce::String& text,
                   const juce::StringArray& items, const juce::String& help);
    void applyPreset (int presetIndex);
    void markCustom();
    void applyTipsMode();
    void updateEnabledControls();

    K4WobbleProcessor& proc;

    GatedTooltipWindow tooltips { this };

    WobbleScope  visualizer;
    PatternStrip patternStrip;

    juce::Label titleLabel, subtitleLabel, versionLabel;

    juce::ComboBox presetBox;
    juce::Label    presetLabel;

    juce::ToggleButton tipsButton { "Tips" };

    juce::ToggleButton patternButton { "Pattern" };
    std::unique_ptr<ButtonAttachment> patternAttachment;

    juce::ComboBox stepsBox;
    juce::Label    stepsLabel;
    std::unique_ptr<BoxAttachment> stepsAttachment;

    juce::ComboBox rateBox, shapeBox, filterBox;
    juce::Label    rateLabel, shapeLabel, filterLabel;
    std::unique_ptr<BoxAttachment> rateAttachment, shapeAttachment, filterAttachment;

    LabeledKnob cutoff, res, depth, drive, width, mix;

    bool settingPreset = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (K4WobbleEditor)
};
