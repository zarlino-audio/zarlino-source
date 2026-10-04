#pragma once

#include "dsp/Stft.h"

#include <vector>

namespace zarlino::source::dsp
{

struct Onset
{
    int sampleIndex = 0;         // approximate onset position
    float strength = 0.0f;       // normalised flux peak strength [0, 1]
};

/** Spectral-flux onset detection with an adaptive median/MAD threshold. */
class TransientDetector
{
public:
    struct Settings
    {
        float thresholdMads = 4.0f;     // peak must exceed median + k * 1.4826 * MAD
        int medianWindowFrames = 12;    // local window for the adaptive threshold
        int minSpacingMs = 30;          // minimum distance between reported onsets
    };

    /** Detects onsets in a mono sample stream. */
    static std::vector<Onset> detect (const float* samples,
                                      int numSamples,
                                      double sampleRate,
                                      const StftConfig& config = {},
                                      Settings settings = {});

    /** Convenience overload: averages all channels to mono first. */
    static std::vector<Onset> detect (const juce::AudioBuffer<float>& buffer,
                                      double sampleRate,
                                      const StftConfig& config = {},
                                      Settings settings = {});

    /** Raw onset detection function (one value per STFT frame). */
    static std::vector<float> onsetStrengthFunction (const float* samples,
                                                     int numSamples,
                                                     const Stft& stft);
};

} // namespace zarlino::source::dsp
