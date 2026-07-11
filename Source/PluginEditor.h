#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
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

    struct LabeledBox
    {
        juce::ComboBox box;
        juce::Label    label;
        std::unique_ptr<BoxAttachment> attachment;
    };

    void setupKnob (LabeledKnob&, const juce::String& paramID, const juce::String& text,
                    const juce::String& suffix, const juce::String& help);
    void setupBox (LabeledBox&, const juce::String& paramID, const juce::String& text,
                   const juce::StringArray& items, const juce::String& help);

    void applySettings (const std::vector<std::pair<juce::String, float>>&);
    void applyUserPresetFile (const juce::File&);
    void saveUserPreset();
    void rebuildPresetMenu (int selectId);
    void rollDice();
    void markCustom();
    void applyTipsMode();
    void updateEnabledControls();
    void updateAdvancedView();

    static juce::File userPresetDirectory();

    K4WobbleProcessor& proc;

    GatedTooltipWindow tooltips { this };

    WobbleScope  visualizer;
    PatternStrip patternStrip;

    juce::Label titleLabel, subtitleLabel, versionLabel;

    juce::ComboBox presetBox;
    juce::Label    presetLabel;
    juce::TextButton saveButton { "Save" };
    juce::TextButton diceButton { "Dice" };
    juce::Array<juce::File> userPresetFiles;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::ToggleButton tipsButton     { "Tips" };
    juce::ToggleButton advancedButton { "Advanced" };

    juce::ToggleButton patternButton { "Pattern" };
    std::unique_ptr<ButtonAttachment> patternAttachment;

    LabeledBox stepsBox;

    LabeledBox rateBox, shapeBox, filterBox;             // simple combos
    LabeledBox slopeBox, driveModeBox, stepLenBox;       // advanced combos
    juce::ToggleButton autoGainButton { "Auto-Gain" };
    std::unique_ptr<ButtonAttachment> autoGainAttachment;

    LabeledKnob cutoff, res, depth, drive, split, width, mix;   // main row
    LabeledKnob swing, push, lazy, trim;                        // advanced row

    bool settingPreset = false;
    bool advanced      = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (K4WobbleEditor)
};
