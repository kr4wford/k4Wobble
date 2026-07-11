#pragma once

#include <juce_core/juce_core.h>

// Parameter IDs shared by the processor, editor, pattern strip and presets.
// IDs are frozen once released: v1.0 IDs must never change, new parameters
// only ever append (so old sessions keep loading correctly).
namespace ids
{
    // v1.0
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

    // v1.1
    constexpr auto split     = "split";
    constexpr auto swing     = "swing";
    constexpr auto push      = "push";
    constexpr auto slope     = "slope";
    constexpr auto drivemode = "drivemode";
    constexpr auto lazy      = "lazy";
    constexpr auto steplen   = "steplen";
    constexpr auto trim      = "trim";
    constexpr auto autogain  = "autogain";

    // Per-step lanes (step = rate lane, dep/cut/shp added in v1.1)
    inline juce::String step (int i) { return "step" + juce::String (i + 1); }
    inline juce::String dep (int i)  { return "dep"  + juce::String (i + 1); }
    inline juce::String cut (int i)  { return "cut"  + juce::String (i + 1); }
    inline juce::String shp (int i)  { return "shp"  + juce::String (i + 1); }
}
