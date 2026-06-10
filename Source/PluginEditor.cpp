#include "PluginEditor.h"
#include <utility>

namespace
{
    constexpr int editorWidth  = 760;
    constexpr int editorHeight = 568;

    // Division indices: 0=1/1 1=1/2 2=1/2T 3=1/4. 4=1/4 5=1/4T 6=1/8. 7=1/8
    //                    8=1/8T 9=1/16. 10=1/16 11=1/16T 12=1/32 13=Rest
    struct Preset
    {
        const char* name;
        int   rate, shape, filter;                       // choice indices
        float cutoff, res, depth, drive, width, mix;     // raw parameter values
        bool  pattern;
        int   steps;                                     // 1..8
        int   stepDivs[8];
    };

    constexpr Preset presets[] =
    {
        { "Custom",          4, 0, 0, 2500.0f, 40.0f, 100.0f,  6.0f,   0.0f, 100.0f, false, 8, { 4, 4, 4, 4, 4, 4, 4, 4 } },
        { "Init",            4, 0, 0, 2500.0f, 40.0f, 100.0f,  6.0f,   0.0f, 100.0f, false, 8, { 4, 4, 4, 4, 4, 4, 4, 4 } },
        { "Classic Wob",     4, 0, 0, 2000.0f, 55.0f, 100.0f,  9.0f,   0.0f, 100.0f, false, 8, { 4, 4, 4, 4, 4, 4, 4, 4 } },
        { "Wub Machine",     4, 0, 0, 2600.0f, 60.0f, 100.0f, 12.0f,   0.0f, 100.0f, true,  8, { 4, 4, 7, 7, 4, 10, 10, 1 } },
        { "Talking Bass",    4, 0, 4, 2500.0f, 50.0f,  90.0f,  9.0f,   0.0f, 100.0f, false, 8, { 4, 4, 4, 4, 4, 4, 4, 4 } },
        { "Half-Time Grind", 1, 2, 0, 1800.0f, 45.0f,  80.0f, 18.0f,   0.0f, 100.0f, false, 8, { 4, 4, 4, 4, 4, 4, 4, 4 } },
        { "Acid Squelch",    7, 0, 1, 1400.0f, 85.0f,  70.0f, 15.0f,   0.0f, 100.0f, false, 8, { 4, 4, 4, 4, 4, 4, 4, 4 } },
        { "Laser Chop",      7, 4, 0, 6000.0f, 30.0f, 100.0f,  6.0f,   0.0f, 100.0f, false, 8, { 4, 4, 4, 4, 4, 4, 4, 4 } },
        { "Robot Gibberish", 10, 5, 4, 2500.0f, 60.0f, 100.0f, 12.0f,  0.0f, 100.0f, false, 8, { 4, 4, 4, 4, 4, 4, 4, 4 } },
        { "Stereo Swirl",    1, 0, 0, 3500.0f, 35.0f,  60.0f,  3.0f, 180.0f, 100.0f, false, 8, { 4, 4, 4, 4, 4, 4, 4, 4 } },
        { "Notch Phaser",    0, 0, 3, 1200.0f, 65.0f,  80.0f,  0.0f,  90.0f, 100.0f, false, 8, { 4, 4, 4, 4, 4, 4, 4, 4 } },
    };
}

K4WobbleEditor::K4WobbleEditor (K4WobbleProcessor& p)
    : AudioProcessorEditor (p), proc (p), visualizer (p), patternStrip (p)
{
    // Visualizer added first so it sits behind every control.
    addAndMakeVisible (visualizer);
    visualizer.setTooltip ("Live view of the wobble: the faint shape is your dry signal, "
                           "the mesh is the wobbled output (violet = filter shut, cyan = open), "
                           "and the thin line is the LFO sweeping the filter. Click to pause.");

    titleLabel.setText ("k4 Wobble", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("tempo-synced wobble filter", juce::dontSendNotification);
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);
    subtitleLabel.setFont (juce::FontOptions (11.0f));
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.45f));
    addAndMakeVisible (subtitleLabel);

    versionLabel.setText ("v" JucePlugin_VersionString, juce::dontSendNotification);
    versionLabel.setJustificationType (juce::Justification::centredRight);
    versionLabel.setFont (juce::FontOptions (12.0f));
    versionLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.55f));
    versionLabel.setTooltip ("Plugin version - check the GitHub Releases page for the latest");
    addAndMakeVisible (versionLabel);

    // --- Presets --------------------------------------------------------------
    presetLabel.setText ("Preset", juce::dontSendNotification);
    presetLabel.setJustificationType (juce::Justification::centredRight);
    presetLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    presetLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    addAndMakeVisible (presetLabel);

    for (int i = 0; i < juce::numElementsInArray (presets); ++i)
        presetBox.addItem (presets[i].name, i + 1);
    presetBox.setSelectedId (2, juce::dontSendNotification); // Init
    presetBox.setTooltip ("Starting points for classic dubstep sounds. Pick one, then tweak - "
                          "any change makes it yours.");
    presetBox.onChange = [this]
    {
        if (! settingPreset)
            applyPreset (presetBox.getSelectedId() - 1);
    };
    addAndMakeVisible (presetBox);

    // --- Tips checkbox ----------------------------------------------------------
    tipsButton.setTooltip ("Show these hover tips. Untick when you know your way around.");
    tipsButton.setToggleState ((bool) proc.apvts.state.getProperty ("tipsOn", true),
                               juce::dontSendNotification);
    tooltips.tipsEnabled = [this] { return tipsButton.getToggleState(); };
    tipsButton.onClick = [this]
    {
        proc.apvts.state.setProperty ("tipsOn", tipsButton.getToggleState(), nullptr);
        applyTipsMode();
    };
    addAndMakeVisible (tipsButton);

    // --- Pattern row ------------------------------------------------------------
    patternButton.setTooltip ("Sequence the wobble rate. Each step lasts one beat and picks its "
                              "own LFO speed - that's how the classic 'wob... wob... wobwobwob' "
                              "basslines move. While on, the Rate box is ignored.");
    addAndMakeVisible (patternButton);
    patternAttachment = std::make_unique<ButtonAttachment> (proc.apvts, "pattern", patternButton);
    patternButton.onClick = [this]
    {
        updateEnabledControls();
        if (! settingPreset)
            markCustom();
    };

    patternStrip.setTooltip ("Each box is one beat. Click to cycle the rate, right-click to cycle "
                             "back, drag up/down or scroll to dial one in. A dash rests (the "
                             "filter stays open). The lit box is the step playing now.");
    patternStrip.onUserChange = [this] { markCustom(); };
    addAndMakeVisible (patternStrip);

    stepsLabel.setText ("Steps", juce::dontSendNotification);
    stepsLabel.setJustificationType (juce::Justification::centredRight);
    stepsLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    stepsLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    addAndMakeVisible (stepsLabel);

    stepsBox.addItemList ({ "1", "2", "3", "4", "5", "6", "7", "8" }, 1);
    stepsBox.setTooltip ("Pattern length in beats. 8 steps = two bars in 4/4.");
    addAndMakeVisible (stepsBox);
    stepsAttachment = std::make_unique<BoxAttachment> (proc.apvts, "steps", stepsBox);
    stepsBox.onChange = [this] { if (! settingPreset) markCustom(); };

    // --- Mode boxes ---------------------------------------------------------------
    setupBox (rateBox, rateLabel, rateAttachment, "rate", "Rate",
              wobble::divisionNames (false),
              "How fast the wobble cycles, synced to the host tempo. 1/4 = one wob per "
              "beat. Ignored while Pattern is on.");

    setupBox (shapeBox, shapeLabel, shapeAttachment, "shape", "Shape",
              { "Sine", "Triangle", "Saw Down", "Saw Up", "Square", "Random" },
              "The motion of the sweep. Sine = smooth wob, Saw Down = snaps open then sweeps "
              "shut, Square = hard on/off chop, Random = a new filter position every cycle "
              "(locked to the song, so it repeats identically every loop).");

    setupBox (filterBox, filterLabel, filterAttachment, "filter", "Filter",
              { "Low-pass", "Band-pass", "High-pass", "Notch", "Talk" },
              "What the LFO sweeps. Low-pass is the classic wobble; Band-pass is hollow and "
              "nasal; High-pass thins the sound; Notch is a phasery scoop; Talk morphs vowel "
              "formants for a talking bass.");

    // --- Knobs ----------------------------------------------------------------
    setupKnob (cutoff, "cutoff", "Cutoff", " Hz",
               "The top of the sweep - the filter opens up to here. In Talk mode this shifts "
               "the voice character instead.");
    setupKnob (res,    "res",    "Res",    " %",
               "Resonance - how much the filter squeals and honks around its sweep point. "
               "High values are the aggressive dubstep zone.");
    setupKnob (depth,  "depth",  "Depth",  " %",
               "How far down the wobble sweeps from the Cutoff - up to five octaves at 100%. "
               "In Talk mode, how far the vowels morph.");
    setupKnob (drive,  "drive",  "Drive",  " dB",
               "Saturation after the filter. Adds growl and grit; high values get nasty "
               "(in a good way).");
    setupKnob (width,  "width",  "Width",  juce::String::fromUTF8 ("\xC2\xB0"), // degree sign
               "Offsets the right channel's LFO so the wobble swirls between the speakers. "
               "Zero keeps it mono-safe and centred.");
    setupKnob (mix,    "mix",    "Mix",    " %",
               "Dry/wet blend. 100% is the full effect; back it off for parallel wobble.");

    // Cache every control as an image: the scope underneath repaints at 60 fps,
    // and without this each frame would re-render every knob, label and combo
    // (vector + text work) instead of just compositing cached images.
    for (auto* child : getChildren())
        if (child != &visualizer && dynamic_cast<juce::TooltipWindow*> (child) == nullptr)
            child->setBufferedToImage (true);

    updateEnabledControls();
    setSize (editorWidth, editorHeight);
}

K4WobbleEditor::~K4WobbleEditor() = default;

void K4WobbleEditor::setupKnob (LabeledKnob& k, const juce::String& paramID,
                                const juce::String& text, const juce::String& suffix,
                                const juce::String& help)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    if (suffix.isNotEmpty())
        k.slider.setTextValueSuffix (suffix);
    k.slider.setTooltip (help);
    addAndMakeVisible (k.slider);

    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setColour (juce::Label::textColourId, juce::Colours::white);
    k.label.setTooltip (help);
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<SliderAttachment> (proc.apvts, paramID, k.slider);

    k.slider.onValueChange = [this]
    {
        if (! settingPreset)
            markCustom();
    };
}

void K4WobbleEditor::setupBox (juce::ComboBox& box, juce::Label& label,
                               std::unique_ptr<BoxAttachment>& attachment,
                               const juce::String& paramID, const juce::String& text,
                               const juce::StringArray& items, const juce::String& help)
{
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredRight);
    label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
    label.setTooltip (help);
    addAndMakeVisible (label);

    box.addItemList (items, 1);
    box.setTooltip (help);
    addAndMakeVisible (box);
    attachment = std::make_unique<BoxAttachment> (proc.apvts, paramID, box);

    box.onChange = [this]
    {
        updateEnabledControls();
        if (! settingPreset)
            markCustom();
    };
}

void K4WobbleEditor::markCustom()
{
    if (presetBox.getSelectedId() != 1)
        presetBox.setSelectedId (1, juce::dontSendNotification);
}

void K4WobbleEditor::applyTipsMode()
{
    // The gate (getTipFor) blocks new tips; this clears one already showing.
    if (! tipsButton.getToggleState())
        tooltips.hideTip();
}

void K4WobbleEditor::updateEnabledControls()
{
    const bool patternOn = patternButton.getToggleState();
    rateBox.setEnabled (! patternOn);
    rateBox.setAlpha (patternOn ? 0.45f : 1.0f);
    rateLabel.setAlpha (patternOn ? 0.45f : 1.0f);
    stepsBox.setEnabled (patternOn);
    stepsBox.setAlpha (patternOn ? 1.0f : 0.45f);
    stepsLabel.setAlpha (patternOn ? 1.0f : 0.45f);
}

void K4WobbleEditor::applyPreset (int presetIndex)
{
    if (presetIndex <= 0 || presetIndex >= juce::numElementsInArray (presets))
        return; // Custom

    const auto& pr = presets[presetIndex];
    settingPreset = true;

    const auto set = [this] (const juce::String& id, float value)
    {
        if (auto* prm = proc.apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
    };

    set ("rate",    (float) pr.rate);
    set ("shape",   (float) pr.shape);
    set ("filter",  (float) pr.filter);
    set ("cutoff",  pr.cutoff);
    set ("res",     pr.res);
    set ("depth",   pr.depth);
    set ("drive",   pr.drive);
    set ("width",   pr.width);
    set ("mix",     pr.mix);
    set ("pattern", pr.pattern ? 1.0f : 0.0f);
    set ("steps",   (float) (pr.steps - 1));
    for (int i = 0; i < 8; ++i)
        set ("step" + juce::String (i + 1), (float) pr.stepDivs[i]);

    settingPreset = false;
    updateEnabledControls();
}

void K4WobbleEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff121218));
}

void K4WobbleEditor::resized()
{
    // Visualizer is a full-bleed background behind everything; clicks over the
    // control panel must fall through to the controls, not toggle pause.
    visualizer.setBounds (getLocalBounds());
    visualizer.setReservedBottom (272);

    // Header strip.
    auto header = getLocalBounds().removeFromTop (44).reduced (12, 8);
    versionLabel.setBounds (header.removeFromRight (56));
    tipsButton.setBounds (header.removeFromRight (62));
    presetBox.setBounds (header.removeFromRight (150).reduced (0, 2));
    presetLabel.setBounds (header.removeFromRight (54));
    titleLabel.setBounds (header.removeFromLeft (120));
    subtitleLabel.setBounds (header);

    // Controls sit over the lower portion of the background.
    auto area = getLocalBounds().removeFromBottom (266).reduced (12, 6);

    auto patternRow = area.removeFromTop (54);
    patternButton.setBounds (patternRow.removeFromLeft (88));
    stepsBox.setBounds (patternRow.removeFromRight (58).reduced (0, 13));
    stepsLabel.setBounds (patternRow.removeFromRight (44));
    patternStrip.setBounds (patternRow.reduced (6, 4));

    area.removeFromTop (6);

    auto comboRow = area.removeFromTop (40);
    const int third = comboRow.getWidth() / 3;
    for (const auto& [label, box] : { std::pair { &rateLabel, &rateBox },
                                      std::pair { &shapeLabel, &shapeBox },
                                      std::pair { &filterLabel, &filterBox } })
    {
        auto cell = comboRow.removeFromLeft (third);
        label->setBounds (cell.removeFromLeft (52));
        box->setBounds (cell.reduced (6, 7));
    }

    area.removeFromTop (4);

    auto knobRow = area;
    const int w = knobRow.getWidth() / 6;
    for (auto* k : { &cutoff, &res, &depth, &drive, &width, &mix })
    {
        auto cell = knobRow.removeFromLeft (w);
        k->label.setBounds (cell.removeFromTop (18));
        k->slider.setBounds (cell.reduced (4));
    }
}
