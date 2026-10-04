#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace zarlino::source::analysis
{

struct TempoEstimate
{
    bool valid = false;
    double bpm = 0.0;
    float confidence = 0.0f;    // normalised autocorrelation peak

    /** Mirrors the product rule: unclear tempo is reported as uncertain,
        never invented (spec section 34). */
    bool uncertain() const noexcept { return ! valid; }
};

/** Tempo estimation from the onset-strength envelope (spec section 34).

    Autocorrelation of the spectral-flux onset function over the plausible
    BPM range; confidence is the normalised peak height. If the evidence is
    weak, `valid` stays false - "Tempo uncertain". */
class TempoAnalyzer
{
public:
    struct Settings
    {
        double minBpm = 60.0;
        double maxBpm = 200.0;
        float minConfidence = 0.2f;
    };

    static TempoEstimate analyse (const juce::AudioBuffer<float>& buffer,
                                  double sampleRate,
                                  Settings settings = {});
};

} // namespace zarlino::source::analysis
