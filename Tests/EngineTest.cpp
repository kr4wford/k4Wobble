// Console smoke test for WobbleEngine: runs every filter mode and LFO shape,
// checks the output stays finite, that the wobble actually modulates the
// signal, and that the LFO is locked to the timeline (same ppq = same output).

#include "../Source/WobbleEngine.h"
#include "../Source/PresetLibrary.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

static int failures = 0;

static void expect (bool condition, const char* what)
{
    if (! condition)
    {
        std::printf ("FAIL: %s\n", what);
        ++failures;
    }
}

// Renders 2 seconds of a 110 Hz saw through the engine; returns RMS of the
// output and min/max of the reported LFO position.
struct RenderResult { float rms, modMin, modMax; bool finite; };

static RenderResult render (WobbleEngine& engine, WobbleEngine::Parameters p,
                            double sampleRate, double startPpq)
{
    const int blockSize = 512;
    const int numBlocks = (int) (2.0 * sampleRate) / blockSize;

    juce::AudioBuffer<float> buffer (2, blockSize);
    std::vector<float> mod ((size_t) blockSize, 0.0f);

    RenderResult r { 0.0f, 1.0e9f, -1.0e9f, true };
    double accum = 0.0;
    long long count = 0;
    double phase = 0.0;

    for (int b = 0; b < numBlocks; ++b)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            const float saw = (float) (2.0 * phase - 1.0) * 0.5f;
            phase += 110.0 / sampleRate;
            if (phase >= 1.0) phase -= 1.0;
            buffer.setSample (0, i, saw);
            buffer.setSample (1, i, saw);
        }

        p.ppq = startPpq + (double) (b * blockSize) * (p.bpm / 60.0 / sampleRate);
        engine.setParameters (p);
        engine.process (buffer, mod.data());

        for (int i = 0; i < blockSize; ++i)
        {
            const float s = buffer.getSample (0, i);
            if (! std::isfinite (s))
                r.finite = false;
            accum += (double) s * (double) s;
            ++count;
            r.modMin = std::min (r.modMin, mod[(size_t) i]);
            r.modMax = std::max (r.modMax, mod[(size_t) i]);
        }
    }

    r.rms = (float) std::sqrt (accum / (double) count);
    return r;
}

int main()
{
    constexpr double sampleRate = 48000.0;

    WobbleEngine::Parameters base;
    base.bpm = 140.0;
    base.isPlaying = true;
    base.depth = 1.0f;
    base.mix = 1.0f;

    // Every filter mode passes signal, stays finite, and the LFO sweeps.
    for (int f = 0; f <= 4; ++f)
    {
        WobbleEngine engine;
        engine.prepare (sampleRate, 512, 2);

        auto p = base;
        p.filter = (WobbleEngine::Filter) f;
        const auto r = render (engine, p, sampleRate, 0.0);

        std::printf ("filter %d: rms=%.4f mod=[%.3f..%.3f] finite=%d\n",
                     f, r.rms, r.modMin, r.modMax, (int) r.finite);
        expect (r.finite, "output is finite");
        expect (r.rms > 0.005f, "output passes signal");
        expect (r.modMax - r.modMin > 0.8f, "LFO sweeps most of its range");
    }

    // Every LFO shape modulates without blowing up.
    for (int s = 0; s <= 5; ++s)
    {
        WobbleEngine engine;
        engine.prepare (sampleRate, 512, 2);

        auto p = base;
        p.shape = (WobbleEngine::Shape) s;
        const auto r = render (engine, p, sampleRate, 0.0);
        expect (r.finite, "shape output is finite");
        expect (r.rms > 0.005f, "shape passes signal");
    }

    // The pattern engine: rests hold the filter open, steps stay finite.
    {
        WobbleEngine engine;
        engine.prepare (sampleRate, 512, 2);

        auto p = base;
        p.usePattern = true;
        p.patternLen = 4;
        p.stepDiv[0] = 4;                   // 1/4
        p.stepDiv[1] = wobble::restIndex;   // rest
        p.stepDiv[2] = 10;                  // 1/16
        p.stepDiv[3] = 7;                   // 1/8
        const auto r = render (engine, p, sampleRate, 0.0);
        expect (r.finite, "pattern output is finite");
        expect (r.rms > 0.005f, "pattern passes signal");
        expect (engine.getCurrentStep() >= 0, "current step is reported");
    }

    // Timeline lock: rendering twice from the same ppq gives identical output.
    {
        WobbleEngine a, b2;
        a.prepare (sampleRate, 512, 2);
        b2.prepare (sampleRate, 512, 2);

        auto p = base;
        p.shape = WobbleEngine::Shape::Random;
        const auto ra = render (a,  p, sampleRate, 16.0);
        const auto rb = render (b2, p, sampleRate, 16.0);
        expect (std::abs (ra.rms - rb.rms) < 1.0e-6f,
                "same timeline position renders identically");
    }

    // Full resonance + max drive torture: still finite and bounded.
    {
        WobbleEngine engine;
        engine.prepare (sampleRate, 512, 2);

        auto p = base;
        p.resonance = 1.0f;
        p.driveDb = 36.0f;
        p.rateDiv = 12; // 1/32
        const auto r = render (engine, p, sampleRate, 0.0);
        expect (r.finite, "torture output is finite");
        expect (r.rms < 2.0f, "torture output is bounded");
    }

    // Silence in → exact silence out. Anything the scope showed with no input
    // would have to be a DSP bug.
    {
        WobbleEngine engine;
        engine.prepare (sampleRate, 512, 2);

        auto p = base;
        juce::AudioBuffer<float> buffer (2, 512);
        std::vector<float> mod (512, 0.0f);
        bool clean = true;

        for (int b = 0; b < 200; ++b)
        {
            buffer.clear();
            p.ppq = (double) (b * 512) * (p.bpm / 60.0 / sampleRate);
            engine.setParameters (p);
            engine.process (buffer, mod.data());
            for (int i = 0; i < 512; ++i)
                if (buffer.getSample (0, i) != 0.0f || buffer.getSample (1, i) != 0.0f)
                    clean = false;
        }
        expect (clean, "silence in gives exact silence out");
    }

    // Mix at 0 must be bit-transparent.
    {
        WobbleEngine engine;
        auto p = base;
        p.mix = 0.0f;
        p.resonance = 0.9f;
        p.driveDb = 24.0f;
        engine.setParameters (p);          // before prepare, so the smoothers snap to it
        engine.prepare (sampleRate, 512, 2);

        juce::AudioBuffer<float> buffer (2, 512);
        std::vector<float> mod (512, 0.0f);
        std::vector<float> dry (512, 0.0f);
        bool transparent = true;
        double phase = 0.0;

        for (int b = 0; b < 50; ++b)
        {
            for (int i = 0; i < 512; ++i)
            {
                dry[(size_t) i] = (float) (2.0 * phase - 1.0) * 0.5f;
                phase += 110.0 / sampleRate;
                if (phase >= 1.0) phase -= 1.0;
                buffer.setSample (0, i, dry[(size_t) i]);
                buffer.setSample (1, i, dry[(size_t) i]);
            }
            p.ppq = (double) (b * 512) * (p.bpm / 60.0 / sampleRate);
            engine.setParameters (p);
            engine.process (buffer, mod.data());
            for (int i = 0; i < 512; ++i)
            {
                const float out = buffer.getSample (0, i);
                if (std::memcmp (&out, &dry[(size_t) i], sizeof (float)) != 0)
                    transparent = false;
            }
        }
        expect (transparent, "mix 0 is bit-transparent");
    }

    // Host block size must not change the sound (everything is timeline-driven).
    {
        const int total = 48000;
        std::vector<float> input ((size_t) total);
        double phase = 0.0;
        for (int i = 0; i < total; ++i)
        {
            input[(size_t) i] = (float) (2.0 * phase - 1.0) * 0.5f;
            phase += 110.0 / sampleRate;
            if (phase >= 1.0) phase -= 1.0;
        }

        const auto renderChunked = [&] (int chunk)
        {
            WobbleEngine engine;
            auto p = base;
            engine.setParameters (p);
            engine.prepare (sampleRate, chunk, 2);

            std::vector<float> out ((size_t) total, 0.0f);
            std::vector<float> mod ((size_t) chunk, 0.0f);
            juce::AudioBuffer<float> buf (2, chunk);

            for (int pos = 0; pos + chunk <= total; pos += chunk)
            {
                for (int i = 0; i < chunk; ++i)
                {
                    buf.setSample (0, i, input[(size_t) (pos + i)]);
                    buf.setSample (1, i, input[(size_t) (pos + i)]);
                }
                p.ppq = (double) pos * (p.bpm / 60.0 / sampleRate);
                engine.setParameters (p);
                engine.process (buf, mod.data());
                for (int i = 0; i < chunk; ++i)
                    out[(size_t) (pos + i)] = buf.getSample (0, i);
            }
            return out;
        };

        const auto a = renderChunked (480);
        const auto b3 = renderChunked (96);
        float maxDiff = 0.0f;
        for (int i = 0; i < 47520; ++i)   // common processed length
            maxDiff = std::max (maxDiff, std::abs (a[(size_t) i] - b3[(size_t) i]));
        std::printf ("chunk invariance: maxDiff=%.2e\n", (double) maxDiff);
        expect (maxDiff < 1.0e-6f, "block size does not change the sound");
    }

    // Mono buffers work.
    {
        WobbleEngine engine;
        engine.prepare (sampleRate, 512, 1);

        auto p = base;
        juce::AudioBuffer<float> buffer (1, 512);
        std::vector<float> mod (512, 0.0f);
        double phase = 0.0;
        bool finite = true;
        float rms = 0.0f;

        for (int b = 0; b < 100; ++b)
        {
            for (int i = 0; i < 512; ++i)
            {
                buffer.setSample (0, i, (float) (2.0 * phase - 1.0) * 0.5f);
                phase += 110.0 / sampleRate;
                if (phase >= 1.0) phase -= 1.0;
            }
            p.ppq = (double) (b * 512) * (p.bpm / 60.0 / sampleRate);
            engine.setParameters (p);
            engine.process (buffer, mod.data());
            for (int i = 0; i < 512; ++i)
            {
                const float s = buffer.getSample (0, i);
                if (! std::isfinite (s)) finite = false;
                rms += s * s;
            }
        }
        expect (finite, "mono output is finite");
        expect (std::sqrt (rms / (100.0f * 512.0f)) > 0.005f, "mono passes signal");
    }

    // ---- v1.1 features -----------------------------------------------------

    // Sub Guard: a tone far below the crossover passes nearly untouched even
    // while the wobble chokes everything above it.
    {
        WobbleEngine engine;
        auto p = base;
        p.splitHz = 150.0f;
        p.depth   = 1.0f;
        engine.setParameters (p);
        engine.prepare (sampleRate, 512, 2);

        juce::AudioBuffer<float> buffer (2, 512);
        std::vector<float> mod (512, 0.0f);
        double phase = 0.0;
        double inRms = 0.0, outRms = 0.0;

        for (int b = 0; b < 200; ++b)
        {
            for (int i = 0; i < 512; ++i)
            {
                const float sine = 0.5f * std::sin (2.0f * wobble::kPi * (float) phase);
                phase += 50.0 / sampleRate;   // 50 Hz sub, well below the split
                buffer.setSample (0, i, sine);
                buffer.setSample (1, i, sine);
                inRms += (double) sine * sine;
            }
            p.ppq = (double) (b * 512) * (p.bpm / 60.0 / sampleRate);
            engine.setParameters (p);
            engine.process (buffer, mod.data());
            for (int i = 0; i < 512; ++i)
                outRms += (double) buffer.getSample (0, i) * buffer.getSample (0, i);
        }
        const double ratioDb = 10.0 * std::log10 (outRms / inRms);
        std::printf ("sub guard: 50 Hz level change %.2f dB\n", ratioDb);
        expect (std::abs (ratioDb) < 3.0, "sub guard keeps the sub within 3 dB");
    }

    // Swing + 16-step pattern + lanes + step length: finite and audible.
    {
        WobbleEngine engine;
        engine.prepare (sampleRate, 512, 2);

        auto p = base;
        p.usePattern = true;
        p.patternLen = 16;
        p.swing      = 1.0f;
        p.stepLenQ   = 0.5f;
        p.lazy       = 0.5f;
        for (int i = 0; i < 16; ++i)
        {
            p.stepDiv[i]   = (i % 5 == 0) ? wobble::restIndex : (i % wobble::numDivisions);
            p.stepDepth[i] = (i % 2 == 0) ? 1.0f : 0.4f;
            p.stepCut[i]   = ((i % 4) - 2) * 0.5f;
            p.stepShape[i] = i % 7;
        }
        const auto r = render (engine, p, sampleRate, 0.0);
        expect (r.finite, "16-step swung lane pattern is finite");
        expect (r.rms > 0.005f, "16-step swung lane pattern passes signal");
    }

    // Drive modes and 24 dB slope: finite, bounded, audible.
    for (int mode = 0; mode <= 2; ++mode)
    {
        WobbleEngine engine;
        engine.prepare (sampleRate, 512, 2);

        auto p = base;
        p.driveMode = (WobbleEngine::Drive) mode;
        p.driveDb   = 30.0f;
        p.slope24   = true;
        p.resonance = 0.9f;
        const auto r = render (engine, p, sampleRate, 0.0);
        expect (r.finite, "drive mode output is finite");
        expect (r.rms > 0.003f && r.rms < 2.0f, "drive mode output is sane");
    }

    // The dice must be safe: over many rolls it never touches the level /
    // routing parameters and every value stays inside its musical range.
    {
        juce::Random rng (0x5EED);
        bool safe = true, bounded = true;

        for (int roll = 0; roll < 500; ++roll)
        {
            std::map<juce::String, float> patch;
            for (const auto& [id, value] : presets::randomPatch (rng))
                patch[id] = value;

            for (const auto* forbidden : { ids::mix, ids::split, ids::trim, ids::autogain })
                if (patch.count (forbidden) > 0)
                    safe = false;

            const auto within = [&patch, &bounded] (const juce::String& id, float lo, float hi)
            {
                const auto it = patch.find (id);
                if (it == patch.end() || it->second < lo || it->second > hi)
                    bounded = false;
            };
            within (ids::cutoff, 800.0f, 6000.0f);
            within (ids::res,    20.0f,  80.0f);
            within (ids::depth,  60.0f,  100.0f);
            within (ids::drive,  0.0f,   18.0f);
            within (ids::swing,  0.0f,   60.0f);
            within (ids::lazy,   0.0f,   40.0f);
            within (ids::steps,  0.0f,   15.0f);
            for (int i = 0; i < wobble::maxSteps; ++i)
            {
                within (ids::step (i), 0.0f, (float) wobble::restIndex);
                within (ids::dep (i),  50.0f, 100.0f);
                within (ids::cut (i), -25.0f, 25.0f);
                within (ids::shp (i),  0.0f,  6.0f);
            }
        }
        expect (safe,    "dice never touches Mix / Split / Trim / Auto-Gain");
        expect (bounded, "dice values always stay in their musical ranges");
    }

    // Every factory preset carries values a control can actually show.
    {
        bool ok = true;
        for (const auto& pr : presets::factory())
        {
            if (pr.name.isEmpty() || pr.category.isEmpty())
                ok = false;
            for (const auto& [id, value] : pr.settings())
            {
                juce::ignoreUnused (id);
                if (! std::isfinite (value))
                    ok = false;
            }
            if (pr.steps < 1 || pr.steps > wobble::maxSteps)
                ok = false;
        }
        std::printf ("factory presets: %d\n", (int) presets::factory().size());
        expect (ok, "factory presets are well-formed");
    }

    if (failures == 0)
        std::printf ("\nAll engine tests passed.\n");
    else
        std::printf ("\n%d FAILURE(S).\n", failures);

    return failures == 0 ? 0 : 1;
}
