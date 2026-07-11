#include "PluginEditor.h"
#include "PresetLibrary.h"
#include "ParamIDs.h"
#include <utility>

namespace
{
    constexpr int editorWidth = 760;

    // Bottom control block heights; the window grows with the Advanced view.
    constexpr int simpleBottom   = 266;
    constexpr int advancedBottom = 446;
    constexpr int simpleHeight   = 568;
    constexpr int advancedHeight = simpleHeight + (advancedBottom - simpleBottom);

    constexpr int customId       = 1;     // preset combo ids
    constexpr int factoryBaseId  = 10;
    constexpr int userBaseId     = 1000;
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

    // --- Presets ----------------------------------------------------------------
    presetLabel.setText ("Preset", juce::dontSendNotification);
    presetLabel.setJustificationType (juce::Justification::centredRight);
    presetLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    presetLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    addAndMakeVisible (presetLabel);

    presetBox.setTooltip ("Starting points for classic dubstep sounds, plus your own saved "
                          "presets. Pick one, then tweak - any change makes it yours.");
    presetBox.onChange = [this]
    {
        if (settingPreset)
            return;
        const int id = presetBox.getSelectedId();
        if (id >= userBaseId && id - userBaseId < userPresetFiles.size())
            applyUserPresetFile (userPresetFiles[id - userBaseId]);
        else if (id >= factoryBaseId
                 && id - factoryBaseId < (int) presets::factory().size())
            applySettings (presets::factory()[(size_t) (id - factoryBaseId)].settings());
    };
    addAndMakeVisible (presetBox);
    rebuildPresetMenu (factoryBaseId);   // Init

    saveButton.setTooltip ("Save the current sound as a preset file. Saved presets appear "
                           "in the preset menu under User Presets.");
    saveButton.onClick = [this] { saveUserPreset(); };
    addAndMakeVisible (saveButton);

    diceButton.setTooltip ("Roll a random - but always musical - patch. It never touches "
                           "Mix, Split, Trim or Auto-Gain, so your levels and routing stay "
                           "exactly as you set them.");
    diceButton.onClick = [this] { rollDice(); };
    addAndMakeVisible (diceButton);

    // --- Tips checkbox ------------------------------------------------------------
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

    // --- Advanced toggle ----------------------------------------------------------
    advancedButton.setTooltip ("Show the advanced controls: groove (swing/push), filter slope, "
                               "drive character, step feel and output trim - plus the extra "
                               "sequencer lanes (Depth, Cutoff, Shape per step).");
    advancedButton.setToggleState ((bool) proc.apvts.state.getProperty ("advancedOn", false),
                                   juce::dontSendNotification);
    advancedButton.onClick = [this]
    {
        advanced = advancedButton.getToggleState();
        proc.apvts.state.setProperty ("advancedOn", advanced, nullptr);
        updateAdvancedView();
    };
    addAndMakeVisible (advancedButton);

    // --- Pattern row ----------------------------------------------------------------
    patternButton.setTooltip ("Sequence the wobble rate. Each step lasts one beat (see Step "
                              "Length in Advanced) and picks its own LFO speed - that's how the "
                              "classic 'wob... wob... wobwobwob' basslines move. While on, the "
                              "Rate box is ignored.");
    addAndMakeVisible (patternButton);
    patternAttachment = std::make_unique<ButtonAttachment> (proc.apvts, ids::pattern, patternButton);
    patternButton.onClick = [this]
    {
        updateEnabledControls();
        if (! settingPreset)
            markCustom();
    };

    patternStrip.setTooltip ("Each box is one step. Rate lane: click to cycle, right-click back, "
                             "drag or scroll to dial one in; a dash rests (the filter stays "
                             "open). Depth/Cutoff lanes: drag or scroll, right-click resets. "
                             "The lit box is the step playing now; lane tabs appear with "
                             "Advanced.");
    patternStrip.onUserChange = [this] { markCustom(); };
    addAndMakeVisible (patternStrip);

    {
        juce::StringArray counts;
        for (int i = 1; i <= wobble::maxSteps; ++i)
            counts.add (juce::String (i));
        setupBox (stepsBox, ids::steps, "Steps", counts,
                  "Pattern length in steps. With 1-beat steps, 8 = two bars in 4/4 and "
                  "16 = four bars.");
    }

    // --- Mode boxes -------------------------------------------------------------------
    setupBox (rateBox, ids::rate, "Rate", wobble::divisionNames (false),
              "How fast the wobble cycles, synced to the host tempo. 1/4 = one wob per "
              "beat. Ignored while Pattern is on.");

    setupBox (shapeBox, ids::shape, "Shape",
              { "Sine", "Triangle", "Saw Down", "Saw Up", "Square", "Random" },
              "The motion of the sweep. Sine = smooth wob, Saw Down = snaps open then sweeps "
              "shut, Square = hard on/off chop, Random = a new filter position every cycle "
              "(locked to the song, so it repeats identically every loop).");

    setupBox (filterBox, ids::filter, "Filter",
              { "Low-pass", "Band-pass", "High-pass", "Notch", "Talk" },
              "What the LFO sweeps. Low-pass is the classic wobble; Band-pass is hollow and "
              "nasal; High-pass thins the sound; Notch is a phasery scoop; Talk morphs vowel "
              "formants for a talking bass.");

    // --- Advanced boxes -----------------------------------------------------------------
    setupBox (slopeBox, ids::slope, "Slope", { "12 dB", "24 dB" },
              "Filter steepness. 24 dB adds a second stage for a darker, more aggressive "
              "sweep without doubling the resonance.");

    setupBox (driveModeBox, ids::drivemode, "Drive", { "Soft", "Hard", "Fold" },
              "Saturation character. Soft = smooth tube-ish growl, Hard = aggressive clip, "
              "Fold = wavefolding for metallic, gnarly textures.");

    setupBox (stepLenBox, ids::steplen, "Step Len", { "1/2 beat", "1 beat", "2 beats" },
              "How long each pattern step lasts. Half-beat steps make twitchy patterns, "
              "2-beat steps make slow evolving ones.");

    autoGainButton.setTooltip ("Fully compensate the drive level so cranking Drive changes "
                               "the tone, not the loudness - honest A/B comparisons.");
    addAndMakeVisible (autoGainButton);
    autoGainAttachment = std::make_unique<ButtonAttachment> (proc.apvts, ids::autogain, autoGainButton);
    autoGainButton.onClick = [this] { if (! settingPreset) markCustom(); };

    // --- Main knobs -----------------------------------------------------------------------
    setupKnob (cutoff, ids::cutoff, "Cutoff", " Hz",
               "The top of the sweep - the filter opens up to here. In Talk mode this shifts "
               "the voice character instead.");
    setupKnob (res,    ids::res,    "Res",    " %",
               "Resonance - how much the filter squeals and honks around its sweep point. "
               "High values are the aggressive dubstep zone.");
    setupKnob (depth,  ids::depth,  "Depth",  " %",
               "How far down the wobble sweeps from the Cutoff - up to five octaves at 100%. "
               "In Talk mode, how far the vowels morph.");
    setupKnob (drive,  ids::drive,  "Drive",  " dB",
               "Saturation after the filter. Adds growl and grit; high values get nasty "
               "(in a good way).");
    setupKnob (split,  ids::split,  "Split",  " Hz",
               "Sub Guard: everything below this frequency stays clean and dry, so the "
               "wobble growls on top of a solid sub. 0 = off (wobble the whole signal). "
               "Try 90-120 Hz for bass music.");
    setupKnob (width,  ids::width,  "Width",  juce::String::fromUTF8 ("\xC2\xB0"), // degree sign
               "Offsets the right channel's LFO so the wobble swirls between the speakers. "
               "Zero keeps it mono-safe and centred.");
    setupKnob (mix,    ids::mix,    "Mix",    " %",
               "Dry/wet blend. 100% is the full effect; back it off for parallel wobble.");

    // --- Advanced knobs ---------------------------------------------------------------------
    setupKnob (swing, ids::swing, "Swing", " %",
               "Delays every second pattern step for a shuffled, head-nod groove. "
               "Zero is dead straight.");
    setupKnob (push,  ids::push,  "Push",  " %",
               "Shifts the LFO phase so the wob peaks ahead of or behind the beat. "
               "Small values move the groove's pocket.");
    setupKnob (lazy,  ids::lazy,  "Lazy",  " %",
               "Glides the filter between LFO values instead of snapping - rubbery, "
               "smeared wobbles at high settings.");
    setupKnob (trim,  ids::trim,  "Trim",  " dB",
               "Output level of the plugin, after everything else.");

    // Cache every control as an image: the scope underneath repaints at 60 fps,
    // and without this each frame would re-render every knob, label and combo
    // (vector + text work) instead of just compositing cached images.
    for (auto* child : getChildren())
        if (child != &visualizer && dynamic_cast<juce::TooltipWindow*> (child) == nullptr)
            child->setBufferedToImage (true);

    advanced = advancedButton.getToggleState();
    updateEnabledControls();
    updateAdvancedView();
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

void K4WobbleEditor::setupBox (LabeledBox& b, const juce::String& paramID,
                               const juce::String& text, const juce::StringArray& items,
                               const juce::String& help)
{
    b.label.setText (text, juce::dontSendNotification);
    b.label.setJustificationType (juce::Justification::centredRight);
    b.label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    b.label.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
    b.label.setTooltip (help);
    addAndMakeVisible (b.label);

    b.box.addItemList (items, 1);
    b.box.setTooltip (help);
    addAndMakeVisible (b.box);
    b.attachment = std::make_unique<BoxAttachment> (proc.apvts, paramID, b.box);

    b.box.onChange = [this]
    {
        updateEnabledControls();
        if (! settingPreset)
            markCustom();
    };
}

// --------------------------------------------------------------------------- presets

juce::File K4WobbleEditor::userPresetDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("k4 Audio")
               .getChildFile ("k4 Wobble Presets");
}

void K4WobbleEditor::rebuildPresetMenu (int selectId)
{
    presetBox.clear (juce::dontSendNotification);
    presetBox.addItem ("Custom", customId);

    juce::String currentCategory;
    const auto& factory = presets::factory();
    for (int i = 0; i < (int) factory.size(); ++i)
    {
        const auto& pr = factory[(size_t) i];
        if (pr.category != currentCategory)
        {
            currentCategory = pr.category;
            presetBox.addSectionHeading (currentCategory);
        }
        presetBox.addItem (pr.name, factoryBaseId + i);
    }

    userPresetFiles = userPresetDirectory().findChildFiles (juce::File::findFiles, false,
                                                            "*.k4wpreset");
    userPresetFiles.sort();
    if (! userPresetFiles.isEmpty())
    {
        presetBox.addSectionHeading ("User Presets");
        for (int i = 0; i < userPresetFiles.size(); ++i)
            presetBox.addItem (userPresetFiles[i].getFileNameWithoutExtension(),
                               userBaseId + i);
    }

    presetBox.setSelectedId (selectId, juce::dontSendNotification);
}

void K4WobbleEditor::applySettings (const std::vector<std::pair<juce::String, float>>& settings)
{
    settingPreset = true;
    for (const auto& [id, value] : settings)
        if (auto* prm = proc.apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
    settingPreset = false;
    updateEnabledControls();
}

void K4WobbleEditor::applyUserPresetFile (const juce::File& file)
{
    const auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("K4WOBBLE_PRESET"))
        return;

    std::vector<std::pair<juce::String, float>> settings;
    for (auto* param : xml->getChildIterator())
        if (param->hasTagName ("PARAM"))
            settings.push_back ({ param->getStringAttribute ("id"),
                                  (float) param->getDoubleAttribute ("value") });
    applySettings (settings);
}

void K4WobbleEditor::saveUserPreset()
{
    auto dir = userPresetDirectory();
    dir.createDirectory();

    fileChooser = std::make_unique<juce::FileChooser> (
        "Save preset", dir.getChildFile ("My Wobble.k4wpreset"), "*.k4wpreset");

    fileChooser->launchAsync (
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File())
                return;

            juce::XmlElement xml ("K4WOBBLE_PRESET");
            xml.setAttribute ("version", JucePlugin_VersionString);
            for (auto* p : proc.getParameters())
            {
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                {
                    auto* param = xml.createNewChildElement ("PARAM");
                    param->setAttribute ("id", rp->paramID);
                    param->setAttribute ("value",
                                         (double) rp->convertFrom0to1 (rp->getValue()));
                }
            }

            if (xml.writeTo (file))
            {
                rebuildPresetMenu (customId);
                const int idx = userPresetFiles.indexOf (file);
                if (idx >= 0)
                    presetBox.setSelectedId (userBaseId + idx, juce::dontSendNotification);
            }
        });
}

void K4WobbleEditor::rollDice()
{
    auto& rng = juce::Random::getSystemRandom();
    applySettings (presets::randomPatch (rng));
    presetBox.setSelectedId (customId, juce::dontSendNotification);
}

// --------------------------------------------------------------------------- view state

void K4WobbleEditor::markCustom()
{
    if (presetBox.getSelectedId() != customId)
        presetBox.setSelectedId (customId, juce::dontSendNotification);
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
    rateBox.box.setEnabled (! patternOn);
    rateBox.box.setAlpha (patternOn ? 0.45f : 1.0f);
    rateBox.label.setAlpha (patternOn ? 0.45f : 1.0f);
    stepsBox.box.setEnabled (patternOn);
    stepsBox.box.setAlpha (patternOn ? 1.0f : 0.45f);
    stepsBox.label.setAlpha (patternOn ? 1.0f : 0.45f);
}

void K4WobbleEditor::updateAdvancedView()
{
    patternStrip.setAdvanced (advanced);

    for (auto* b : { &slopeBox, &driveModeBox, &stepLenBox })
    {
        b->box.setVisible (advanced);
        b->label.setVisible (advanced);
    }
    autoGainButton.setVisible (advanced);
    for (auto* k : { &swing, &push, &lazy, &trim })
    {
        k->slider.setVisible (advanced);
        k->label.setVisible (advanced);
    }

    setSize (editorWidth, advanced ? advancedHeight : simpleHeight);
}

void K4WobbleEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff121218));
}

void K4WobbleEditor::resized()
{
    const int bottomH = advanced ? advancedBottom : simpleBottom;

    // Visualizer is a full-bleed background behind everything; clicks over the
    // control panel must fall through to the controls, not toggle pause.
    visualizer.setBounds (getLocalBounds());
    visualizer.setReservedBottom (bottomH + 6);

    // Header strip.
    auto header = getLocalBounds().removeFromTop (44).reduced (12, 8);
    versionLabel.setBounds (header.removeFromRight (44));
    tipsButton.setBounds (header.removeFromRight (56));
    advancedButton.setBounds (header.removeFromRight (82));
    diceButton.setBounds (header.removeFromRight (46).reduced (0, 2));
    header.removeFromRight (4);
    saveButton.setBounds (header.removeFromRight (48).reduced (0, 2));
    presetBox.setBounds (header.removeFromRight (140).reduced (0, 2));
    presetLabel.setBounds (header.removeFromRight (48));
    titleLabel.setBounds (header.removeFromLeft (110));
    subtitleLabel.setBounds (header);

    // Controls sit over the lower portion of the background.
    auto area = getLocalBounds().removeFromBottom (bottomH).reduced (12, 6);

    auto patternRow = area.removeFromTop (advanced ? 72 : 54);
    patternButton.setBounds (patternRow.removeFromLeft (88));
    stepsBox.box.setBounds (patternRow.removeFromRight (58)
                                .withSizeKeepingCentre (58, 26));
    stepsBox.label.setBounds (patternRow.removeFromRight (44));
    patternStrip.setBounds (patternRow.reduced (6, 2));

    area.removeFromTop (6);

    const auto layoutBoxRow = [] (juce::Rectangle<int> row,
                                  std::initializer_list<LabeledBox*> boxes)
    {
        const int cellW = row.getWidth() / (int) boxes.size();
        for (auto* b : boxes)
        {
            auto cell = row.removeFromLeft (cellW);
            b->label.setBounds (cell.removeFromLeft (52));
            b->box.setBounds (cell.reduced (6, 7));
        }
    };

    layoutBoxRow (area.removeFromTop (40), { &rateBox, &shapeBox, &filterBox });

    if (advanced)
    {
        area.removeFromTop (4);
        auto rowB = area.removeFromTop (38);
        auto agCell = rowB.removeFromRight (rowB.getWidth() / 4);
        autoGainButton.setBounds (agCell.reduced (10, 5));
        layoutBoxRow (rowB, { &slopeBox, &driveModeBox, &stepLenBox });
    }

    area.removeFromTop (4);

    const auto layoutKnobRow = [] (juce::Rectangle<int> row,
                                   std::initializer_list<LabeledKnob*> knobs)
    {
        const int w = row.getWidth() / (int) knobs.size();
        for (auto* k : knobs)
        {
            auto cell = row.removeFromLeft (w);
            k->label.setBounds (cell.removeFromTop (18));
            k->slider.setBounds (cell.reduced (4));
        }
    };

    layoutKnobRow (area.removeFromTop (advanced ? 150 : area.getHeight()),
                   { &cutoff, &res, &depth, &drive, &split, &width, &mix });

    if (advanced)
    {
        area.removeFromTop (2);
        auto rowB = area;
        rowB.reduce (rowB.getWidth() / 6, 0);   // centre the four smaller knobs
        layoutKnobRow (rowB, { &swing, &push, &lazy, &trim });
    }
}
