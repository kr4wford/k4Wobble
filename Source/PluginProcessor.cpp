#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParamIDs.h"

K4WobbleProcessor::K4WobbleProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    visBufferDry.assign (visFifoSize, 0.0f);
    visBufferWet.assign (visFifoSize, 0.0f);
    visBufferMod.assign (visFifoSize, 1.0f);

    const auto raw = [this] (const juce::String& id)
    {
        auto* p = apvts.getRawParameterValue (id);
        jassert (p != nullptr);
        return p;
    };

    prm.rate      = raw (ids::rate);      prm.shape     = raw (ids::shape);
    prm.filter    = raw (ids::filter);    prm.cutoff    = raw (ids::cutoff);
    prm.res       = raw (ids::res);       prm.depth     = raw (ids::depth);
    prm.drive     = raw (ids::drive);     prm.width     = raw (ids::width);
    prm.mix       = raw (ids::mix);       prm.pattern   = raw (ids::pattern);
    prm.steps     = raw (ids::steps);     prm.split     = raw (ids::split);
    prm.swing     = raw (ids::swing);     prm.push      = raw (ids::push);
    prm.slope     = raw (ids::slope);     prm.drivemode = raw (ids::drivemode);
    prm.lazy      = raw (ids::lazy);      prm.steplen   = raw (ids::steplen);
    prm.trim      = raw (ids::trim);      prm.autogain  = raw (ids::autogain);

    for (int i = 0; i < wobble::maxSteps; ++i)
    {
        prm.stepDiv[i] = raw (ids::step (i));
        prm.stepDep[i] = raw (ids::dep (i));
        prm.stepCut[i] = raw (ids::cut (i));
        prm.stepShp[i] = raw (ids::shp (i));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout K4WobbleProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    // ----- v1.0 parameters (IDs and ranges frozen; choice lists may only grow) -----
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ids::rate, 1 }, "Rate", wobble::divisionNames (false), 4)); // 1/4

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ids::shape, 1 }, "Shape",
        StringArray { "Sine", "Triangle", "Saw Down", "Saw Up", "Square", "Random" }, 0));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ids::filter, 1 }, "Filter",
        StringArray { "Low-pass", "Band-pass", "High-pass", "Notch", "Talk" }, 0));

    NormalisableRange<float> cutoffRange (60.0f, 16000.0f, 1.0f);
    cutoffRange.setSkewForCentre (1500.0f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::cutoff, 1 }, "Cutoff", cutoffRange, 2500.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::res, 1 }, "Res",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 40.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::depth, 1 }, "Depth",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::drive, 1 }, "Drive",
        NormalisableRange<float> (0.0f, 36.0f, 0.1f), 6.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::width, 1 }, "Width",
        NormalisableRange<float> (0.0f, 180.0f, 1.0f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::mix, 1 }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { ids::pattern, 1 }, "Pattern", false));

    // v1.0 saved indices 0..7 still map to "1".."8" — appending is safe.
    StringArray stepCounts;
    for (int i = 1; i <= wobble::maxSteps; ++i)
        stepCounts.add (String (i));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ids::steps, 1 }, "Steps", stepCounts, 7));

    for (int i = 0; i < wobble::maxSteps; ++i)
        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { ids::step (i), 1 }, "Step " + String (i + 1),
            wobble::divisionNames (true), 4)); // 1/4

    // ----- v1.1 parameters (all defaults neutral = v1.0 behaviour) -----
    NormalisableRange<float> splitRange (0.0f, 500.0f, 1.0f);
    splitRange.setSkewForCentre (120.0f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::split, 1 }, "Split", splitRange, 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::swing, 1 }, "Swing",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::push, 1 }, "Push",
        NormalisableRange<float> (-50.0f, 50.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ids::slope, 1 }, "Slope",
        StringArray { "12 dB", "24 dB" }, 0));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ids::drivemode, 1 }, "Drive Mode",
        StringArray { "Soft", "Hard", "Fold" }, 0));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::lazy, 1 }, "Lazy",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ids::steplen, 1 }, "Step Length",
        StringArray { "1/2 beat", "1 beat", "2 beats" }, 1));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::trim, 1 }, "Trim",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { ids::autogain, 1 }, "Auto-Gain", false));

    for (int i = 0; i < wobble::maxSteps; ++i)
    {
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ids::dep (i), 1 }, "Step Depth " + String (i + 1),
            NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ids::cut (i), 1 }, "Step Cutoff " + String (i + 1),
            NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 0.0f));

        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { ids::shp (i), 1 }, "Step Shape " + String (i + 1),
            StringArray { "Global", "Sine", "Triangle", "Saw Down", "Saw Up", "Square", "Random" }, 0));
    }

    return layout;
}

bool K4WobbleProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();
    const auto mainIn  = layouts.getMainInputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono()
        && mainOut != juce::AudioChannelSet::stereo())
        return false;

    return mainOut == mainIn;
}

void K4WobbleProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, getMainBusNumInputChannels());

    const auto sz = static_cast<size_t> (juce::jmax (1, samplesPerBlock));
    monoDry.assign (sz, 0.0f);
    monoWet.assign (sz, 0.0f);
    modBuf.assign (sz, 1.0f);
}

void K4WobbleProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Gather current parameter values into the engine.
    WobbleEngine::Parameters p;
    p.shape      = static_cast<WobbleEngine::Shape>  ((int) prm.shape->load());
    p.filter     = static_cast<WobbleEngine::Filter> ((int) prm.filter->load());
    p.rateDiv    = (int) prm.rate->load();
    p.cutoffHz   = prm.cutoff->load();
    p.resonance  = prm.res->load()   * 0.01f;
    p.depth      = prm.depth->load() * 0.01f;
    p.driveDb    = prm.drive->load();
    p.widthDeg   = prm.width->load();
    p.mix        = prm.mix->load()   * 0.01f;
    p.usePattern = prm.pattern->load() > 0.5f;
    p.patternLen = 1 + (int) prm.steps->load();

    p.splitHz    = prm.split->load();
    p.swing      = prm.swing->load() * 0.01f;
    p.push       = prm.push->load()  * 0.01f;
    p.slope24    = prm.slope->load() > 0.5f;
    p.driveMode  = static_cast<WobbleEngine::Drive> ((int) prm.drivemode->load());
    p.lazy       = prm.lazy->load() * 0.01f;
    p.stepLenQ   = (float) std::pow (2.0, (int) prm.steplen->load() - 1); // 0.5 / 1 / 2
    p.trimDb     = prm.trim->load();
    p.autoGain   = prm.autogain->load() > 0.5f;

    for (int i = 0; i < wobble::maxSteps; ++i)
    {
        p.stepDiv[i]   = (int) prm.stepDiv[i]->load();
        p.stepDepth[i] = prm.stepDep[i]->load() * 0.01f;
        p.stepCut[i]   = prm.stepCut[i]->load() * 0.01f;
        p.stepShape[i] = (int) prm.stepShp[i]->load();
    }

    // Sync to the host timeline; free-run at the last known tempo when stopped.
    if (auto* playHead = getPlayHead())
    {
        if (const auto pos = playHead->getPosition())
        {
            p.isPlaying = pos->getIsPlaying();
            if (const auto bpm = pos->getBpm())
                p.bpm = *bpm;
            if (const auto ppq = pos->getPpqPosition())
                p.ppq = *ppq;
        }
    }
    engine.setParameters (p);

    const int  n       = buffer.getNumSamples();
    const int  numCh   = juce::jmin (buffer.getNumChannels(), getMainBusNumInputChannels());
    const bool feedViz = (n > 0 && numCh > 0 && n <= (int) monoDry.size());

    const auto mixToMono = [numCh, n, &buffer] (float* dst)
    {
        juce::FloatVectorOperations::copy (dst, buffer.getReadPointer (0), n);
        for (int ch = 1; ch < numCh; ++ch)
            juce::FloatVectorOperations::add (dst, buffer.getReadPointer (ch), n);
        if (numCh > 1)
            juce::FloatVectorOperations::multiply (dst, 1.0f / (float) numCh, n);
    };

    if (feedViz)
        mixToMono (monoDry.data());

    engine.process (buffer, feedViz ? modBuf.data() : nullptr);

    if (feedViz)
    {
        mixToMono (monoWet.data());

        // Keep the FIFO from overflowing if the GUI isn't draining it.
        if (visFifo.getFreeSpace() < n)
            visFifo.read (n - visFifo.getFreeSpace());

        pushVisualizerSamples (monoDry.data(), monoWet.data(), modBuf.data(), n);
    }
}

void K4WobbleProcessor::pushVisualizerSamples (const float* dry, const float* wet,
                                               const float* mod, int n)
{
    const auto scope = visFifo.write (n);

    const auto copyRegion = [this] (const float* dSrc, const float* wSrc, const float* mSrc,
                                    int start, int count)
    {
        juce::FloatVectorOperations::copy (visBufferDry.data() + start, dSrc, count);
        juce::FloatVectorOperations::copy (visBufferWet.data() + start, wSrc, count);
        juce::FloatVectorOperations::copy (visBufferMod.data() + start, mSrc, count);
    };

    if (scope.blockSize1 > 0)
        copyRegion (dry, wet, mod, scope.startIndex1, scope.blockSize1);
    if (scope.blockSize2 > 0)
        copyRegion (dry + scope.blockSize1, wet + scope.blockSize1, mod + scope.blockSize1,
                    scope.startIndex2, scope.blockSize2);
}

int K4WobbleProcessor::readVisualizerSamples (float* dryDest, float* wetDest,
                                              float* modDest, int maxSamples)
{
    const int ready = juce::jmin (maxSamples, visFifo.getNumReady());
    const auto scope = visFifo.read (ready);

    const auto copyRegion = [this] (float* dDst, float* wDst, float* mDst,
                                    int start, int count)
    {
        juce::FloatVectorOperations::copy (dDst, visBufferDry.data() + start, count);
        juce::FloatVectorOperations::copy (wDst, visBufferWet.data() + start, count);
        juce::FloatVectorOperations::copy (mDst, visBufferMod.data() + start, count);
    };

    if (scope.blockSize1 > 0)
        copyRegion (dryDest, wetDest, modDest, scope.startIndex1, scope.blockSize1);
    if (scope.blockSize2 > 0)
        copyRegion (dryDest + scope.blockSize1, wetDest + scope.blockSize1,
                    modDest + scope.blockSize1, scope.startIndex2, scope.blockSize2);
    return ready;
}

juce::AudioProcessorEditor* K4WobbleProcessor::createEditor()
{
    return new K4WobbleEditor (*this);
}

void K4WobbleProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void K4WobbleProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new K4WobbleProcessor();
}
