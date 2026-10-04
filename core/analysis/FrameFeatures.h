#pragma once

#include "dsp/Stft.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>

namespace zarlino::source::analysis
{

/** Configuration for feature extraction. */
struct FeatureConfig
{
    dsp::StftConfig stft {};          // default 2048 / 512 / Hann
    double lowSplitHz = 250.0;        // low band: below this
    double highSplitHz = 2000.0;      // mid band: lowSplit..highSplit, high: above
};

/** Per-frame spectral features (one STFT column). Every value is computed from
    the audio - none are guessed. */
struct FrameFeature
{
    float lowEnergy = 0.0f;       // sum of bin powers below lowSplitHz (DC excluded)
    float midEnergy = 0.0f;       // ... between the splits
    float highEnergy = 0.0f;      // ... above highSplitHz
    float totalEnergy = 0.0f;     // total power in the frame
    float centroidHz = 0.0f;      // power-weighted spectral centroid
    float flatness = 0.0f;        // geometric/arithmetic mean of bin powers:
                                  // ~1 = noise-like, ~0 = tonal-peaky

    float bandTotal() const noexcept { return lowEnergy + midEnergy + highEnergy; }

    float lowRatio() const noexcept  { const float t = bandTotal(); return t > 0.0f ? lowEnergy / t : 0.0f; }
    float midRatio() const noexcept  { const float t = bandTotal(); return t > 0.0f ? midEnergy / t : 0.0f; }
    float highRatio() const noexcept { const float t = bandTotal(); return t > 0.0f ? highEnergy / t : 0.0f; }
};

struct FeatureMap
{
    double sampleRate = 0.0;
    int fftSize = 0;
    int hopSize = 0;
    int numFrames = 0;

    std::vector<FrameFeature> frames;

    double frameSeconds() const noexcept
    {
        return sampleRate > 0.0 ? (double) hopSize / sampleRate : 0.0;
    }

    double frameTime (int index) const noexcept { return (double) index * frameSeconds(); }
};

/** Computes the feature map of a mono signal. */
FeatureMap computeFeatures (const float* mono, int numSamples, double sampleRate,
                            const FeatureConfig& config = {});

/** Convenience: mixes the buffer to mono internally. */
FeatureMap computeFeatures (const juce::AudioBuffer<float>& buffer, double sampleRate,
                            const FeatureConfig& config = {});

} // namespace zarlino::source::analysis
