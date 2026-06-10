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
}

void WobbleEngine::prepare (double newSampleRate, int maxBlockSize, int newNumChannels)
{
    sampleRate  = newSampleRate;
    maxBlock    = juce::jmax (1, maxBlockSize);
    numChannels = juce::jlimit (1, 2, newNumChannels);

    dryBuffer.setSize (numChannels, maxBlock);

    // ~2.5 ms one-pole on the LFO so square / random / step changes don't click.
    lfoCoeff = 1.0f - std::exp (-1.0f / (0.0025f * (float) sampleRate));

    for (auto* sm : { &cutoffSm, &resSm, &depthSm, &driveSm, &mixSm })
        sm->reset (sampleRate, 0.02);

    reset();
}

void WobbleEngine::reset()
{
    for (int c = 0; c < 2; ++c)
    {
        mainSvf[c] = {};
        for (auto& f : talkSvf[c])
            f = {};
        lfoState[c] = 1.0f;
    }

    freeQ = 0.0;

    cutoffSm.setCurrentAndTargetValue (params.cutoffHz);
    resSm.setCurrentAndTargetValue (params.resonance);
    depthSm.setCurrentAndTargetValue (params.depth);
    driveSm.setCurrentAndTargetValue (params.driveDb);
    mixSm.setCurrentAndTargetValue (params.mix);
}

void WobbleEngine::setParameters (const Parameters& newParams)
{
    params = newParams;
    cutoffSm.setTargetValue (newParams.cutoffHz);
    resSm.setTargetValue (newParams.resonance);
    depthSm.setTargetValue (newParams.depth);
    driveSm.setTargetValue (newParams.driveDb);
    mixSm.setTargetValue (newParams.mix);
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

    int firstStep = -1;

    // Derived values that only change while a knob is actually moving: cache
    // them so the steady state costs no pow()/dB conversions per sample.
    float cachedDriveDb = -1.0e9f, driveGain = 1.0f, driveComp = 1.0f;
    float cachedRes     = -1.0f,   damping   = 1.0f, talkDamping = 0.1f;

    for (int i = 0; i < n; ++i)
    {
        const double q = q0 + qps * (double) i;

        // Which division applies right now?
        int    divIdx    = params.rateDiv;
        double stepStart = 0.0;
        bool   rest      = false;

        if (params.usePattern)
        {
            const double beat = std::floor (q);                 // one beat per step
            const auto   k    = (long long) beat;
            const int    step = (int) (((k % len) + len) % len);
            if (i == 0)
                firstStep = step;
            divIdx    = params.stepDiv[step];
            rest      = (divIdx < 0 || divIdx >= wobble::numDivisions);
            stepStart = beat;
        }

        const double P = rest ? 1.0 : (double) wobble::divisions[divIdx].quarters;

        const float cutoff  = cutoffSm.getNextValue();
        const float res     = resSm.getNextValue();
        const float depth   = depthSm.getNextValue();
        const float driveDb = driveSm.getNextValue();
        const float mix     = mixSm.getNextValue();

        if (! juce::exactlyEqual (driveDb, cachedDriveDb))
        {
            cachedDriveDb = driveDb;
            driveGain = juce::Decibels::decibelsToGain (driveDb);
            driveComp = juce::Decibels::decibelsToGain (-0.5f * driveDb);
        }
        if (! juce::exactlyEqual (res, cachedRes))
        {
            cachedRes   = res;
            damping     = 1.0f / (0.55f * std::pow (18.0f, res));
            talkDamping = 1.0f / (5.0f + 15.0f * res);
        }

        for (int c = 0; c < ch; ++c)
        {
            float lfo = 1.0f;
            if (! rest)
            {
                // The pattern restarts the LFO at each step so every beat lands
                // predictably; channel 1 runs offset for stereo width.
                const double local = (params.usePattern ? q - stepStart : q)
                                   + (c == 1 ? (double) offset01 * P : 0.0);
                const double cyc   = std::floor (local / P);
                const double phase = local / P - cyc;
                lfo = lfoValue (params.shape, (float) phase, (long long) cyc);
            }

            lfoState[c] += lfoCoeff * (lfo - lfoState[c]);
            const float l = lfoState[c];

            const float x = dryBuffer.getReadPointer (c)[i];
            float y;

            if (talk)
            {
                const float t     = juce::jlimit (0.0f, 1.0f, 0.5f + (l - 0.5f) * depth);
                const float scale = juce::jlimit (0.5f, 2.0f, cutoff / 2500.0f);

                y = 0.0f;
                for (int f = 0; f < 3; ++f)
                {
                    const float fc  = juce::jlimit (20.0f, maxFc,
                        (vowelLoF[f] + (vowelHiF[f] - vowelLoF[f]) * t) * scale);
                    const float amp = vowelLoA[f] + (vowelHiA[f] - vowelLoA[f]) * t;

                    // talkDamping * bp = unity-gain band-pass, so level holds as Q moves
                    y += amp * talkDamping * svfTick (talkSvf[c][f], x, fc, talkDamping).bp;
                }
                y *= 2.0f;
            }
            else
            {
                const float fc = juce::jlimit (20.0f, maxFc,
                    cutoff * std::exp2 (-depth * 5.0f * (1.0f - l)));
                const auto  o = svfTick (mainSvf[c], x, fc, damping);

                switch (params.filter)
                {
                    case Filter::BandPass: y = o.bp;                  break;
                    case Filter::HighPass: y = o.hp;                  break;
                    case Filter::Notch:    y = x - damping * o.bp;    break;
                    case Filter::LowPass:
                    case Filter::Talk:     // handled above; keeps -Wswitch-enum quiet
                    default:               y = o.lp;                  break;
                }
            }

            y = std::tanh (y * driveGain) * driveComp;

            buffer.getWritePointer (c)[i] = y * mix + x * (1.0f - mix);

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

        if (bad (mainSvf[c]))
            mainSvf[c] = {};
        for (auto& f : talkSvf[c])
            if (bad (f))
                f = {};
        if (! std::isfinite (lfoState[c]))
            lfoState[c] = 1.0f;
    }
}
