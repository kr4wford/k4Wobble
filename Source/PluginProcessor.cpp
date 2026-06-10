#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ids
{
    constexpr auto rate    = "rate";
    constexpr auto shape   = "shape";
    constexpr auto filter  = "filter";
    constexpr auto cutoff  = "cutoff";
    constexpr auto res     = "res";
    constexpr auto depth   = "depth";
    constexpr auto drive   = "drive";
    constexpr auto width   = "width";
    constexpr auto mix     = "mix";
    constexpr auto pattern = "pattern";
    constexpr auto steps   = "steps";

    inline juce::String step (int i) { return "step" + juce::String (i + 1); }
}

K4WobbleProcessor::K4WobbleProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    visBufferDry.assign (visFifoSize, 0.0f);
    visBufferWet.assign (visFifoSize, 0.0f);
    visBufferMod.assign (visFifoSize, 1.0f);
}

juce::AudioProcessorValueTreeState::ParameterLayout K4WobbleProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

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

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ids::steps, 1 }, "Steps",
        StringArray { "1", "2", "3", "4", "5", "6", "7", "8" }, 7));

    for (int i = 0; i < wobble::maxSteps; ++i)
        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { ids::step (i), 1 }, "Step " + String (i + 1),
            wobble::divisionNames (true), 4)); // 1/4

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
    p.shape      = static_cast<WobbleEngine::Shape>  ((int) apvts.getRawParameterValue (ids::shape)->load());
    p.filter     = static_cast<WobbleEngine::Filter> ((int) apvts.getRawParameterValue (ids::filter)->load());
    p.rateDiv    = (int) apvts.getRawParameterValue (ids::rate)->load();
    p.cutoffHz   = apvts.getRawParameterValue (ids::cutoff)->load();
    p.resonance  = apvts.getRawParameterValue (ids::res)->load()   * 0.01f;
    p.depth      = apvts.getRawParameterValue (ids::depth)->load() * 0.01f;
    p.driveDb    = apvts.getRawParameterValue (ids::drive)->load();
    p.widthDeg   = apvts.getRawParameterValue (ids::width)->load();
    p.mix        = apvts.getRawParameterValue (ids::mix)->load()   * 0.01f;
    p.usePattern = apvts.getRawParameterValue (ids::pattern)->load() > 0.5f;
    p.patternLen = 1 + (int) apvts.getRawParameterValue (ids::steps)->load();
    for (int i = 0; i < wobble::maxSteps; ++i)
        p.stepDiv[i] = (int) apvts.getRawParameterValue (ids::step (i))->load();

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
