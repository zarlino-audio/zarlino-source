#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace zarlino::source::dsp
{

/** Zero-crossing helpers. */
class ZeroCrossing
{
public:
    /** Counts sign changes between adjacent samples; stretches of exact zeros
        are treated as belonging to the previous sign. */
    static int countCrossings (const float* samples, int numSamples) noexcept;

    /** Returns the sample index of the sign change nearest to `position`
        (searched outward within `searchRadius`): the index of the first sample
        after the crossing. Returns the clamped position if none is found. */
    static int findNearestCrossing (const float* samples, int numSamples,
                                    int position, int searchRadius) noexcept;

    /** Rough fundamental estimate from the crossing rate - a cheap sanity
        check, not a pitch detector. */
    static double estimateFrequencyFromRate (const float* samples, int numSamples,
                                             double sampleRate) noexcept;
};

} // namespace zarlino::source::dsp
