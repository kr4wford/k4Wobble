#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>
#include "WobbleEngine.h"
#include "ParamIDs.h"

namespace presets
{
    /** A complete factory patch. Field defaults equal the Init sound, so each
        preset only overrides what makes it distinctive. Values are in raw
        parameter units (the same numbers a user would see on the controls). */
    struct Preset
    {
        juce::String name, category;

        int   rate = 4, shape = 0, filter = 0;                 // choice indices
        float cutoff = 2500.0f, res = 40.0f, depth = 100.0f;
        float drive = 6.0f, width = 0.0f, mix = 100.0f;

        bool  pattern = false;
        int   steps = 8;
        std::array<int,   wobble::maxSteps> stepDiv;
        std::array<float, wobble::maxSteps> stepDep;
        std::array<float, wobble::maxSteps> stepCut;
        std::array<int,   wobble::maxSteps> stepShp;

        float split = 0.0f, swing = 0.0f, push = 0.0f, lazy = 0.0f, trim = 0.0f;
        int   slope = 0, driveMode = 0, stepLen = 1;           // stepLen 1 = one beat
        bool  autoGain = false;

        Preset()
        {
            stepDiv.fill (4);      // 1/4
            stepDep.fill (100.0f);
            stepCut.fill (0.0f);
            stepShp.fill (0);      // Global
        }

        /** All (id, raw value) pairs, ready to apply through the APVTS. */
        std::vector<std::pair<juce::String, float>> settings() const
        {
            std::vector<std::pair<juce::String, float>> s;
            s.reserve (20 + 4 * wobble::maxSteps);
            s.push_back ({ ids::rate,      (float) rate });
            s.push_back ({ ids::shape,     (float) shape });
            s.push_back ({ ids::filter,    (float) filter });
            s.push_back ({ ids::cutoff,    cutoff });
            s.push_back ({ ids::res,       res });
            s.push_back ({ ids::depth,     depth });
            s.push_back ({ ids::drive,     drive });
            s.push_back ({ ids::width,     width });
            s.push_back ({ ids::mix,       mix });
            s.push_back ({ ids::pattern,   pattern ? 1.0f : 0.0f });
            s.push_back ({ ids::steps,     (float) (steps - 1) });
            s.push_back ({ ids::split,     split });
            s.push_back ({ ids::swing,     swing });
            s.push_back ({ ids::push,      push });
            s.push_back ({ ids::slope,     (float) slope });
            s.push_back ({ ids::drivemode, (float) driveMode });
            s.push_back ({ ids::lazy,      lazy });
            s.push_back ({ ids::steplen,   (float) stepLen });
            s.push_back ({ ids::trim,      trim });
            s.push_back ({ ids::autogain,  autoGain ? 1.0f : 0.0f });
            for (int i = 0; i < wobble::maxSteps; ++i)
            {
                s.push_back ({ ids::step (i), (float) stepDiv[(size_t) i] });
                s.push_back ({ ids::dep (i),  stepDep[(size_t) i] });
                s.push_back ({ ids::cut (i),  stepCut[(size_t) i] });
                s.push_back ({ ids::shp (i),  (float) stepShp[(size_t) i] });
            }
            return s;
        }
    };

    // Division indices: 0=1/1 1=1/2 2=1/2T 3=1/4. 4=1/4 5=1/4T 6=1/8. 7=1/8
    //                    8=1/8T 9=1/16. 10=1/16 11=1/16T 12=1/32 13=Rest
    inline const std::vector<Preset>& factory()
    {
        static const std::vector<Preset> list = []
        {
            std::vector<Preset> v;
            const auto add = [&v] (const char* cat, const char* name) -> Preset&
            {
                Preset p;
                p.category = cat;
                p.name     = name;
                v.push_back (std::move (p));
                return v.back();
            };

            // ---- Classic Wobs -------------------------------------------------
            add ("Classic Wobs", "Init");
            { auto& p = add ("Classic Wobs", "Classic Wob");
              p.cutoff = 2000; p.res = 55; p.drive = 9; }
            { auto& p = add ("Classic Wobs", "Deep Wob");
              p.cutoff = 1500; p.res = 50; p.drive = 10; }
            { auto& p = add ("Classic Wobs", "Clean Sub Wob");
              p.split = 95; p.cutoff = 2200; p.res = 60; p.drive = 12; }
            { auto& p = add ("Classic Wobs", "Double-Time");
              p.rate = 7; p.drive = 9; p.res = 50; }
            { auto& p = add ("Classic Wobs", "Halftime Heavy");
              p.rate = 1; p.shape = 2; p.cutoff = 1800; p.res = 45; p.depth = 85;
              p.drive = 15; p.slope = 1; }
            { auto& p = add ("Classic Wobs", "Slow Cinematic");
              p.rate = 0; p.depth = 70; p.lazy = 40; p.width = 90; p.drive = 3; }

            // ---- Patterns -----------------------------------------------------
            { auto& p = add ("Patterns", "Wub Machine");
              p.pattern = true; p.cutoff = 2600; p.res = 60; p.drive = 12;
              p.stepDiv = { 4, 4, 7, 7, 4, 10, 10, 1,  4, 4, 7, 7, 4, 10, 10, 1 }; }
            { auto& p = add ("Patterns", "Call & Response");
              p.pattern = true; p.res = 55; p.drive = 10;
              p.stepDiv = { 4, 4, 7, 7, 4, 13, 10, 10,  4, 4, 7, 7, 4, 13, 10, 10 }; }
            { auto& p = add ("Patterns", "Triplet Roller");
              p.pattern = true; p.res = 50; p.drive = 9;
              p.stepDiv = { 5, 5, 5, 5, 8, 8, 5, 5,  5, 5, 5, 5, 8, 8, 5, 5 }; }
            { auto& p = add ("Patterns", "Build-Up 16");
              p.pattern = true; p.steps = 16; p.res = 55; p.drive = 9;
              p.stepDiv = { 1, 1, 4, 4, 3, 7, 7, 7,  8, 10, 10, 10, 11, 12, 12, 12 }; }
            { auto& p = add ("Patterns", "Swung Groove");
              p.pattern = true; p.swing = 55; p.lazy = 15; p.res = 45; p.drive = 8;
              p.stepDiv = { 7, 7, 7, 7, 7, 7, 7, 7,  7, 7, 7, 7, 7, 7, 7, 7 }; }
            { auto& p = add ("Patterns", "Depth Pump");
              p.pattern = true; p.res = 50; p.drive = 10;
              p.stepDep = { 100, 45, 100, 45, 100, 45, 100, 70,
                            100, 45, 100, 45, 100, 45, 100, 70 }; }
            { auto& p = add ("Patterns", "Cutoff Stairs");
              p.pattern = true; p.res = 60; p.drive = 9;
              p.stepCut = { -60, -40, -20, 0, 20, 40, 60, 80,
                            -60, -40, -20, 0, 20, 40, 60, 80 }; }
            { auto& p = add ("Patterns", "Shape Shifter");
              p.pattern = true; p.res = 55; p.drive = 10;
              p.stepShp = { 1, 1, 5, 5, 3, 3, 6, 6,  1, 1, 5, 5, 3, 3, 6, 6 }; }

            // ---- Talk & Vox ---------------------------------------------------
            { auto& p = add ("Talk & Vox", "Talking Bass");
              p.filter = 4; p.res = 50; p.depth = 90; p.drive = 9; }
            { auto& p = add ("Talk & Vox", "Yoi Yoi");
              p.filter = 4; p.rate = 7; p.res = 70; p.drive = 12; }
            { auto& p = add ("Talk & Vox", "Robot Gibberish");
              p.filter = 4; p.shape = 5; p.rate = 10; p.res = 60; p.drive = 12; }
            { auto& p = add ("Talk & Vox", "Slow Vowel Pad");
              p.filter = 4; p.rate = 0; p.lazy = 60; p.width = 120; p.drive = 3;
              p.depth = 80; }

            // ---- FX & Weird ---------------------------------------------------
            { auto& p = add ("FX & Weird", "Acid Squelch");
              p.rate = 7; p.filter = 1; p.cutoff = 1400; p.res = 85; p.depth = 70;
              p.drive = 15; }
            { auto& p = add ("FX & Weird", "Laser Chop");
              p.rate = 7; p.shape = 4; p.cutoff = 6000; p.res = 30; }
            { auto& p = add ("FX & Weird", "Notch Phaser");
              p.rate = 0; p.filter = 3; p.cutoff = 1200; p.res = 65; p.depth = 80;
              p.drive = 0; p.width = 90; }
            { auto& p = add ("FX & Weird", "Fold Machine");
              p.driveMode = 2; p.drive = 20; p.res = 30; p.cutoff = 3000; }
            { auto& p = add ("FX & Weird", "Hard Gate");
              p.rate = 10; p.shape = 4; p.driveMode = 1; p.cutoff = 8000; p.drive = 10; }
            { auto& p = add ("FX & Weird", "Random Robot 16");
              p.pattern = true; p.steps = 16; p.shape = 5; p.res = 60; p.drive = 12;
              p.stepDiv = { 10, 10, 7, 10, 13, 10, 7, 7,  10, 10, 4, 10, 13, 12, 12, 7 }; }
            { auto& p = add ("FX & Weird", "Rubber Band");
              p.filter = 1; p.shape = 3; p.rate = 7; p.res = 75; p.driveMode = 2;
              p.drive = 8; p.cutoff = 1800; }

            // ---- Subtle & Groove ----------------------------------------------
            { auto& p = add ("Subtle & Groove", "Stereo Swirl");
              p.rate = 1; p.cutoff = 3500; p.res = 35; p.depth = 60; p.drive = 3;
              p.width = 180; }
            { auto& p = add ("Subtle & Groove", "Gentle Motion");
              p.rate = 1; p.depth = 35; p.mix = 60; p.drive = 0; p.res = 30; }
            { auto& p = add ("Subtle & Groove", "Filter Tremolo");
              p.rate = 7; p.cutoff = 9000; p.depth = 25; p.res = 25; p.drive = 0; }
            { auto& p = add ("Subtle & Groove", "Parallel Growl");
              p.mix = 45; p.drive = 24; p.slope = 1; p.res = 65; p.cutoff = 1600; }
            { auto& p = add ("Subtle & Groove", "Lazy River");
              p.rate = 1; p.lazy = 70; p.depth = 60; p.res = 45; p.width = 60;
              p.drive = 4; }

            return v;
        }();
        return list;
    }

    /** A random — but always usable — patch.

        Deliberately does NOT touch Mix, Split, Trim, Auto-Gain or anything
        else that would change levels or routing the user has set up: rolling
        the dice explores sounds, it never breaks the session (the classic
        "host randomised my output volume" trap). Every value is drawn from a
        curated musical range, weighted toward the workhorse settings.       */
    inline std::vector<std::pair<juce::String, float>> randomPatch (juce::Random& rng)
    {
        std::vector<std::pair<juce::String, float>> s;

        const auto pick = [&rng] (std::initializer_list<int> weights) // index by weight
        {
            int total = 0;
            for (int w : weights) total += w;
            int roll = rng.nextInt (total), idx = 0;
            for (int w : weights) { if (roll < w) return idx; roll -= w; ++idx; }
            return 0;
        };

        // Rate: the wobble workhorses, dotted/triplets as spice.
        static constexpr int rateChoices[] = { 4, 7, 10, 1, 5, 8, 3 };
        s.push_back ({ ids::rate,  (float) rateChoices[pick ({ 30, 25, 15, 10, 8, 7, 5 })] });

        // Shape: sine-heavy; Random shape is rare spice.
        s.push_back ({ ids::shape, (float) pick ({ 40, 15, 15, 10, 12, 8 }) });

        // Filter: LP is the genre default; Talk shows up sometimes.
        s.push_back ({ ids::filter, (float) pick ({ 45, 15, 8, 8, 24 }) });

        // Tone: bounded well away from useless extremes.
        s.push_back ({ ids::cutoff, 800.0f * std::pow (7.5f, rng.nextFloat()) }); // ~800..6000, log
        s.push_back ({ ids::res,    20.0f + rng.nextFloat() * 60.0f });           // 20..80
        s.push_back ({ ids::depth,  60.0f + rng.nextFloat() * 40.0f });           // 60..100
        s.push_back ({ ids::drive,  (float) rng.nextInt (19) });                  // 0..18 dB

        static constexpr float widthChoices[] = { 0.0f, 30.0f, 90.0f, 180.0f };
        s.push_back ({ ids::width, widthChoices[pick ({ 55, 20, 15, 10 })] });

        // Advanced: mostly neutral, occasionally flavoured.
        s.push_back ({ ids::slope,     (float) pick ({ 70, 30 }) });
        s.push_back ({ ids::drivemode, (float) pick ({ 70, 15, 15 }) });
        s.push_back ({ ids::swing,     rng.nextInt (100) < 25 ? 20.0f + rng.nextFloat() * 40.0f : 0.0f });
        s.push_back ({ ids::lazy,      rng.nextInt (100) < 30 ? rng.nextFloat() * 40.0f : 0.0f });
        s.push_back ({ ids::push,      0.0f });
        s.push_back ({ ids::steplen,   1.0f });

        // Pattern: half the rolls sequence, half stay simple.
        const bool usePattern = rng.nextBool();
        s.push_back ({ ids::pattern, usePattern ? 1.0f : 0.0f });
        static constexpr int lenChoices[] = { 4, 8, 16 };
        s.push_back ({ ids::steps, (float) (lenChoices[pick ({ 20, 55, 25 })] - 1) });

        // Steps: mostly a repeated backbone division with a few changes and
        // the occasional rest — the shape of real wobble basslines.
        const int backbone = rateChoices[pick ({ 35, 30, 15, 5, 8, 7, 0 })];
        for (int i = 0; i < wobble::maxSteps; ++i)
        {
            int div = backbone;
            const int roll = rng.nextInt (100);
            if (roll < 8)       div = wobble::restIndex;                    // rest
            else if (roll < 30) div = rateChoices[pick ({ 25, 30, 20, 5, 10, 10, 0 })];
            s.push_back ({ ids::step (i), (float) div });

            // Depth lane: mostly full, some accents.
            s.push_back ({ ids::dep (i), rng.nextInt (100) < 25
                                             ? 50.0f + rng.nextFloat() * 40.0f : 100.0f });
            // Cutoff lane: mostly centred, small pushes.
            s.push_back ({ ids::cut (i), rng.nextInt (100) < 20
                                             ? (rng.nextFloat() * 50.0f - 25.0f) : 0.0f });
            // Shape lane: nearly always Global.
            s.push_back ({ ids::shp (i), rng.nextInt (100) < 12
                                             ? (float) (1 + rng.nextInt (6)) : 0.0f });
        }

        return s;
    }
}
