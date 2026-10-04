#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>

namespace zarlino::source::analysis
{

struct PitchPoint
{
    double time = 0.0;
    double frequencyHz = 0.0;   // 0 = unvoiced frame (reported, not guessed)
    float confidence = 0.0f;    // 0..1, normalised autocorrelation peak
};

struct PitchTrack
{
    std::vector<PitchPoint> points;
    float overallConfidence = 0.0f;   // mean per-frame confidence
    bool anyVoiced = false;
};

/** Normalised-autocorrelation pitch tracking (spec section 33).

    Monophonic material gets a fundamental per frame plus a confidence;
    unvoiced frames are reported as frequency 0 instead of forcing a pitch.
    Polyphonic material degrades to low confidence rather than a fabricated
    single value. The signal is decimated x4 with a box prefilter before the
    autocorrelation to keep the search cheap. */
class PitchAnalyzer
{
public:
    struct Settings
    {
        int frameSize = 2048;        // at the original sample rate
        int hopSize = 512;
        double minHz = 40.0;
        double maxHz = 1200.0;
        float voicingThreshold = 0.35f;   // normalised ACF peak to count as voiced
    };

    static PitchTrack analyse (const juce::AudioBuffer<float>& buffer,
                               double sampleRate,
                               Settings settings = {});
};

} // namespace zarlino::source::analysis
