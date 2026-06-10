#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

namespace wobble
{
    inline constexpr float kPi = 3.14159265358979323846f;

    struct Division { const char* name; float quarters; };

    // Note divisions the LFO can sync to (lengths in quarter notes).
    inline constexpr Division divisions[] =
    {
        { "1/1",   4.0f },
        { "1/2",   2.0f },
        { "1/2T",  4.0f / 3.0f },
        { "1/4.",  1.5f },
        { "1/4",   1.0f },
        { "1/4T",  2.0f / 3.0f },
        { "1/8.",  0.75f },
        { "1/8",   0.5f },
        { "1/8T",  1.0f / 3.0f },
        { "1/16.", 0.375f },
        { "1/16",  0.25f },
        { "1/16T", 1.0f / 6.0f },
        { "1/32",  0.125f },
    };

    inline constexpr int numDivisions = (int) (sizeof (divisions) / sizeof (divisions[0]));
    inline constexpr int maxSteps     = 8;
    inline constexpr int restIndex    = numDivisions;   // pattern steps may also "Rest"

    inline juce::StringArray divisionNames (bool withRest)
    {
        juce::StringArray names;
        for (const auto& d : divisions)
            names.add (d.name);
        if (withRest)
            names.add ("Rest");
        return names;
    }
}

/**
    The whole wobble lives here so the processor stays thin.

    A tempo-synced LFO (phase derived from the host timeline, so it's
    sample-accurate and loops identically on every pass) sweeps a TPT
    state-variable filter, optionally re-choosing its rate every beat from an
    8-step pattern. A "Talk" mode morphs three vowel formant band-passes
    instead. Post-filter tanh drive adds the growl; dry/wet mix at the end.
*/
class WobbleEngine
{
public:
    enum class Shape  { Sine = 0, Triangle, SawDown, SawUp, Square, Random };
    enum class Filter { LowPass = 0, BandPass, HighPass, Notch, Talk };

    struct Parameters
    {
        Shape  shape      = Shape::Sine;
        Filter filter     = Filter::LowPass;
        int    rateDiv    = 4;        // index into wobble::divisions ("1/4")
        float  cutoffHz   = 2500.0f;  // top of the sweep
        float  resonance  = 0.4f;     // 0..1
        float  depth      = 1.0f;     // 0..1 (octaves swept / vowel morph)
        float  driveDb    = 6.0f;
        float  widthDeg   = 0.0f;     // right-channel LFO phase offset
        float  mix        = 1.0f;     // 0..1 wet
        bool   usePattern = false;
        int    patternLen = 8;        // 1..maxSteps, one beat per step
        int    stepDiv[wobble::maxSteps] = { 4, 4, 4, 4, 4, 4, 4, 4 };
        double bpm        = 140.0;
        double ppq        = 0.0;      // host quarter-note position at block start
        bool   isPlaying  = false;
    };

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    void setParameters (const Parameters& newParams);

    /** Processes in place. If modOut is non-null it receives one value per
        sample: the smoothed LFO position 0..1 (1 = filter open), for the GUI. */
    void process (juce::AudioBuffer<float>& buffer, float* modOut);

    /** Pattern step playing right now, or -1 when the pattern is off. */
    int getCurrentStep() const noexcept { return currentStep.load (std::memory_order_relaxed); }

private:
    struct Svf    { float ic1 = 0.0f, ic2 = 0.0f; };
    struct SvfOut { float lp, bp, hp; };

    SvfOut svfTick (Svf&, float input, float cutoffHz, float damping) const;

    static float lfoValue (Shape, float phase, long long cycle);
    static float hash01 (long long n);

    double sampleRate  = 44100.0;
    int    maxBlock    = 512;
    int    numChannels = 2;

    double freeQ = 0.0;          // free-running quarter-note clock while stopped

    float lfoCoeff = 0.01f;      // one-pole smoothing on the LFO (kills clicks)
    float lfoState[2] { 1.0f, 1.0f };

    Svf mainSvf[2];
    Svf talkSvf[2][3];

    juce::SmoothedValue<float> cutoffSm { 2500.0f };
    juce::SmoothedValue<float> resSm    { 0.4f };
    juce::SmoothedValue<float> depthSm  { 1.0f };
    juce::SmoothedValue<float> driveSm  { 6.0f };
    juce::SmoothedValue<float> mixSm    { 1.0f };

    juce::AudioBuffer<float> dryBuffer;

    std::atomic<int> currentStep { -1 };

    Parameters params;
};
