#pragma once

#include "core/extraction/SourceResult.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace zarlino::source::sampling
{

struct OneShot
{
    juce::AudioBuffer<float> audio;
    double sampleRate = 0.0;

    double startTime = 0.0;     // in the source result's timeline (seconds)
    double endTime = 0.0;

    bool success = false;
    juce::String error;
};

/** Turns an extracted source into a usable one-shot (spec section 30):

        strongest onset -> attack/body/decay/usable tail
        -> zero-crossing-snapped boundaries -> short fades

    The transient character is preserved (no long fades, no arbitrary
    normalisation). All boundaries are measured from the audio. */
class OneShotCreator
{
public:
    struct Settings
    {
        double maxLengthSeconds = 4.0;
        double minLengthSeconds = 0.05;
        double tailThresholdDb = -45.0;   // relative to the one-shot peak
        double minTailSeconds = 0.04;     // silence run that ends the one-shot
        double fadeSeconds = 0.003;
    };

    static OneShot create (const extraction::SourceResult& result,
                           Settings settings = {});
};

} // namespace zarlino::source::sampling
