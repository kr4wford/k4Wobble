#include "WobbleEngine.h"
#include <cmath>

namespace
{
    // Three formants morphed between "oo" (boot) and "ah" (father) for Talk
    // mode — Peterson & Barney averages, amplitudes normalised to F1.
    constexpr float vowelLoF[3] = { 300.0f,  870.0f, 2240.0f };
    constexpr float vowelHiF[3] = { 730.0f, 1090.0f, 2440.0f };
    constexpr float vowelLoA[3] = { 1.0f, 0.50f, 0.15f };
    constexpr float vowelHiA[3] = { 1.0f, 0.70f, 0.25f };

    constexpr float butterworthDamping = 1.41421356f;   // Q = 0.707
}

void WobbleEngine::prepare (double newSampleRate, int maxBlockSize, int newNumChannels)
{
    sampleRate  = newSampleRate;
    maxBlock    = juce::jmax (1, maxBlockSize);
    numChannels = juce::jlimit (1, 2, newNumChannels);

    dryBuffer.setSize (numChannels, maxBlock);

    for (auto* sm : { &cutoffSm, &resSm, &depthSm, &driveSm, &mixSm, &splitSm, &trimSm })
        sm->reset (sampleRate, 0.02);

    reset();
}

void WobbleEngine::reset()
{
    for (int c = 0; c < 2; ++c)
    {
        mainSvf[c]  = {};
        mainSvf2[c] = {};
        for (auto& f : talkSvf[c]) f = {};
        for (auto& f : splitLp[c]) f = {};
        for (auto& f : splitHp[c]) f = {};
        lfoState[c] = 1.0f;
    }

    freeQ          = 0.0;
    laneDepthState = 1.0f;
    laneCutState   = 0.0f;

    cutoffSm.setCurrentAndTargetValue (params.cutoffHz);
    resSm.setCurrentAndTargetValue (params.resonance);
    depthSm.setCurrentAndTargetValue (params.depth);
    driveSm.setCurrentAndTargetValue (params.driveDb);
    mixSm.setCurrentAndTargetValue (params.mix);
    splitSm.setCurrentAndTargetValue (params.splitHz);
    trimSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.trimDb));
}

void WobbleEngine::setParameters (const Parameters& newParams)
{
    params = newParams;
    cutoffSm.setTargetValue (newParams.cutoffHz);
    resSm.setTargetValue (newParams.resonance);
    depthSm.setTargetValue (newParams.depth);
    driveSm.setTargetValue (newParams.driveDb);
    mixSm.setTargetValue (newParams.mix);
    splitSm.setTargetValue (newParams.splitHz);
    trimSm.setTargetValue (juce::Decibels::decibelsToGain (newParams.trimDb));

    // ~2.5 ms one-pole on the LFO normally; "lazy" stretches it up to ~80 ms
    // so step changes glide instead of jumping.
    const float tau = 0.0025f + 0.08f * newParams.lazy;
    lfoCoeff = 1.0f - std::exp (-1.0f / (tau * (float) sampleRate));
}

float WobbleEngine::hash01 (long long n)
{
    // Deterministic per-cycle random, keyed to the song position: the "random"
    // wobble is identical on every loop pass and in every render.
    auto x = (juce::uint64) n * 0x9E3779B97F4A7C15ULL;
    x ^= x >> 33;  x *= 0xFF51AFD7ED558CCDULL;  x ^= x >> 33;
    return (float) (x >> 40) / (float) (1 << 24);
}

float WobbleEngine::lfoValue (Shape shape, float phase, long long cycle)
{
    // Output is "filter openness" 0..1; every shape starts open (1) at phase 0
    // so the wobble lands open on the beat.
    switch (shape)
    {
        case Shape::Sine:     return 0.5f + 0.5f * std::cos (2.0f * wobble::kPi * phase);
        case Shape::Triangle: return phase < 0.5f ? 1.0f - 2.0f * phase : 2.0f * phase - 1.0f;
        case Shape::SawDown:  return 1.0f - phase;
        case Shape::SawUp:    return phase;
        case Shape::Square:   return phase < 0.5f ? 1.0f : 0.0f;
        case Shape::Random:   return hash01 (cycle);
    }
    return 1.0f;
}

WobbleEngine::SvfOut WobbleEngine::svfTick (Svf& s, float input, float cutoffHz, float damping) const
{
    // Zavalishin TPT state-variable filter (stable under fast modulation).
    const float g  = std::tan (wobble::kPi * cutoffHz / (float) sampleRate);
    const float a1 = 1.0f / (1.0f + g * (g + damping));
    const float v1 = a1 * s.ic1 + g * a1 * (input - s.ic2);
    const float v2 = s.ic2 + g * v1;

    s.ic1 = 2.0f * v1 - s.ic1;
    s.ic2 = 2.0f * v2 - s.ic2;

    return { v2, v1, input - damping * v1 - v2 };
}

void WobbleEngine::process (juce::AudioBuffer<float>& buffer, float* modOut)
{
    const int n  = buffer.getNumSamples();
    const int ch = juce::jmin (buffer.getNumChannels(), numChannels);
    if (n == 0 || ch == 0 || n > maxBlock)
        return;

    for (int c = 0; c < ch; ++c)
        dryBuffer.copyFrom (c, 0, buffer, c, 0, n);

    const double qps = params.bpm / 60.0 / sampleRate;          // quarters per sample
    const double q0  = params.isPlaying ? params.ppq : freeQ;

    const float  offset01 = (ch > 1 ? params.widthDeg / 360.0f : 0.0f);
    const bool   talk     = (params.filter == Filter::Talk);
    const float  maxFc    = (float) juce::jmin (18000.0, 0.45 * sampleRate);
    const int    len      = juce::jlimit (1, wobble::maxSteps, params.patternLen);
    const double stepLen  = (double) params.stepLenQ;
    const bool   swung    = params.usePattern && params.swing > 0.001f;
    const double swingB   = 1.0 + (double) params.swing / 3.0;  // pair midpoint 1..1.33
    const float  pushAmt  = params.push;

    int firstStep = -1;

    // Derived values that only change while a knob is actually moving: cache
    // them so the steady state costs no pow()/dB conversions per sample.
    float cachedDriveDb = -1.0e9f, driveGain = 1.0f, driveComp = 1.0f;
    float cachedRes     = -1.0f,   damping   = 1.0f, talkDamping = 0.1f;
    float cachedSplit   = -1.0f;
    bool  splitOn       = false;

    for (int i = 0; i < n; ++i)
    {
        const double q = q0 + qps * (double) i;

        // Which division / lane values apply right now?
        int    divIdx     = params.rateDiv;
        double intoStep   = 0.0;          // quarters into the (possibly swung) step
        bool   rest       = false;
        bool   inPattern  = false;
        float  laneDepth  = 1.0f;
        float  laneCut    = 0.0f;
        int    shapeIdx   = (int) params.shape;

        if (params.usePattern)
        {
            inPattern = true;
            double u = q / stepLen;                        // position in step units

            if (swung)
            {
                // MPC-style swing: the midpoint of each step pair slides late.
                const double pairBase = std::floor (u * 0.5) * 2.0;
                const double t = u - pairBase;             // 0..2 within the pair
                u = pairBase + (t < swingB ? t / swingB
                                           : 1.0 + (t - swingB) / (2.0 - swingB));
            }

            const double stepF = std::floor (u);
            const auto   k     = (long long) stepF;
            const int    step  = (int) (((k % len) + len) % len);
            if (i == 0)
                firstStep = step;

            divIdx    = params.stepDiv[step];
            rest      = (divIdx < 0 || divIdx >= wobble::numDivisions);
            intoStep  = (u - stepF) * stepLen;
            laneDepth = params.stepDepth[step];
            laneCut   = params.stepCut[step];
            if (params.stepShape[step] > 0)
                shapeIdx = params.stepShape[step] - 1;
        }

        const double P = rest ? 1.0 : (double) wobble::divisions[divIdx].quarters;

        const float cutoff  = cutoffSm.getNextValue();
        const float res     = resSm.getNextValue();
        const float depth   = depthSm.getNextValue();
        const float driveDb = driveSm.getNextValue();
        const float mix     = mixSm.getNextValue();
        const float split   = splitSm.getNextValue();
        const float trim    = trimSm.getNextValue();

        if (! juce::exactlyEqual (driveDb, cachedDriveDb))
        {
            cachedDriveDb = driveDb;
            driveGain = juce::Decibels::decibelsToGain (driveDb);
            driveComp = juce::Decibels::decibelsToGain (-driveDb * (params.autoGain ? 1.0f : 0.5f));
        }
        if (! juce::exactlyEqual (res, cachedRes))
        {
            cachedRes   = res;
            damping     = 1.0f / (0.55f * std::pow (18.0f, res));
            talkDamping = 1.0f / (5.0f + 15.0f * res);
        }
        if (! juce::exactlyEqual (split, cachedSplit))
        {
            cachedSplit = split;
            splitOn     = (split >= 20.0f);
        }

        // Smooth the lane values so step boundaries step, not zipper.
        laneDepthState += lfoCoeff * (laneDepth - laneDepthState);
        laneCutState   += lfoCoeff * (laneCut   - laneCutState);
        const float effDepth = depth * laneDepthState;
        const float cutOct   = laneCutState * 2.0f;        // +/- 2 octaves

        for (int c = 0; c < ch; ++c)
        {
            float lfo = 1.0f;
            if (! rest)
            {
                // The pattern restarts the LFO at each step so every beat lands
                // predictably; channel 1 runs offset for stereo width; push
                // shifts where the sweep peaks against the beat.
                const double off   = (c == 1 ? (double) offset01 * P : 0.0);
                const double local = (inPattern ? intoStep : q) + off;
                double phase = local / P - std::floor (local / P);
                phase += (double) pushAmt;
                phase -= std::floor (phase);

                // Random values hold per global period so patterns don't
                // repeat the same value every step, yet stay timeline-locked.
                const auto cyc = (long long) std::floor ((q + off) / P);
                lfo = lfoValue ((Shape) shapeIdx, (float) phase, cyc);
            }

            lfoState[c] += lfoCoeff * (lfo - lfoState[c]);
            const float l = lfoState[c];

            const float x = dryBuffer.getReadPointer (c)[i];

            // Sub Guard: below the crossover stays clean and dry.
            float low = 0.0f, high = x;
            if (splitOn)
            {
                low  = svfTick (splitLp[c][1],
                                svfTick (splitLp[c][0], x, split, butterworthDamping).lp,
                                split, butterworthDamping).lp;
                high = svfTick (splitHp[c][1],
                                svfTick (splitHp[c][0], x, split, butterworthDamping).hp,
                                split, butterworthDamping).hp;
            }

            float y;
            if (talk)
            {
                const float t     = juce::jlimit (0.0f, 1.0f, 0.5f + (l - 0.5f) * effDepth);
                const float scale = juce::jlimit (0.5f, 2.0f,
                                                  cutoff * std::exp2 (cutOct) / 2500.0f);

                y = 0.0f;
                for (int f = 0; f < 3; ++f)
                {
                    const float fc  = juce::jlimit (20.0f, maxFc,
                        (vowelLoF[f] + (vowelHiF[f] - vowelLoF[f]) * t) * scale);
                    const float amp = vowelLoA[f] + (vowelHiA[f] - vowelLoA[f]) * t;

                    // talkDamping * bp = unity-gain band-pass, so level holds as Q moves
                    y += amp * talkDamping * svfTick (talkSvf[c][f], high, fc, talkDamping).bp;
                }
                y *= 2.0f;
            }
            else
            {
                const float fc = juce::jlimit (20.0f, maxFc,
                    cutoff * std::exp2 (cutOct - effDepth * 5.0f * (1.0f - l)));
                const auto  o = svfTick (mainSvf[c], high, fc, damping);

                switch (params.filter)
                {
                    case Filter::BandPass: y = o.bp;                     break;
                    case Filter::HighPass: y = o.hp;                     break;
                    case Filter::Notch:    y = high - damping * o.bp;    break;
                    case Filter::LowPass:
                    case Filter::Talk:     // handled above; keeps -Wswitch-enum quiet
                    default:               y = o.lp;                     break;
                }

                if (params.slope24)
                {
                    // Second Butterworth stage: steeper without doubling the
                    // resonance peak.
                    const auto o2 = svfTick (mainSvf2[c], y, fc, butterworthDamping);
                    switch (params.filter)
                    {
                        case Filter::BandPass: y = o2.bp;                              break;
                        case Filter::HighPass: y = o2.hp;                              break;
                        case Filter::Notch:    y = y - butterworthDamping * o2.bp;     break;
                        case Filter::LowPass:
                        case Filter::Talk:
                        default:               y = o2.lp;                              break;
                    }
                }
            }

            switch (params.driveMode)
            {
                case Drive::Hard: y = juce::jlimit (-1.0f, 1.0f, y * driveGain) * driveComp; break;
                case Drive::Fold: y = std::sin (juce::jlimit (-6.0f, 6.0f, y * driveGain)) * driveComp; break;
                case Drive::Soft:
                default:          y = std::tanh (y * driveGain) * driveComp; break;
            }

            buffer.getWritePointer (c)[i] = (low + y * mix + high * (1.0f - mix)) * trim;

            if (c == 0 && modOut != nullptr)
                modOut[i] = l;
        }
    }

    currentStep.store (params.usePattern ? firstStep : -1, std::memory_order_relaxed);

    // Keep the clock moving while the transport is stopped, continuing from
    // wherever the host left off.
    freeQ = q0 + qps * (double) n;

    // Self-heal: a NaN/Inf from a bad host buffer must reset the filters, not
    // become permanent static (jlimit does NOT stop NaN).
    for (int c = 0; c < 2; ++c)
    {
        const auto bad = [] (const Svf& s)
        {
            return ! (std::isfinite (s.ic1) && std::isfinite (s.ic2));
        };

        if (bad (mainSvf[c]))  mainSvf[c]  = {};
        if (bad (mainSvf2[c])) mainSvf2[c] = {};
        for (auto& f : talkSvf[c]) if (bad (f)) f = {};
        for (auto& f : splitLp[c]) if (bad (f)) f = {};
        for (auto& f : splitHp[c]) if (bad (f)) f = {};
        if (! std::isfinite (lfoState[c]))
            lfoState[c] = 1.0f;
    }
    if (! std::isfinite (laneDepthState)) laneDepthState = 1.0f;
    if (! std::isfinite (laneCutState))   laneCutState   = 0.0f;
}
