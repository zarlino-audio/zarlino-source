#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace zarlino::source::analysis
{

struct KeyEstimate
{
    bool valid = false;
    int tonic = 0;              // 0 = C, 1 = C#, ... 11 = B
    bool major = true;
    float confidence = 0.0f;    // margin between the best and runner-up key
    juce::String description;   // "A minor" ... or "Key uncertain"
};

/** Key estimation via chroma + Krumhansl-Schmuckler profiles (spec section 35).

    Chroma is accumulated from the STFT (bin power -> pitch class). The
    histogram is correlated with the published major/minor profiles; the
    confidence is the margin over the runner-up. Weak evidence reports
    "Key uncertain" instead of inventing a key. */
class KeyAnalyzer
{
public:
    struct Settings
    {
        float minCorrelation = 0.4f;   // best correlation needed to be valid
        float minMargin = 0.02f;       // required lead over the runner-up
    };

    static KeyEstimate analyse (const juce::AudioBuffer<float>& buffer,
                                double sampleRate,
                                Settings settings = {});
};

} // namespace zarlino::source::analysis
