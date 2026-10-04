#pragma once

#include "dsp/Stft.h"

namespace zarlino::source::dsp
{

/** Weighted overlap-add inverse STFT.

    Typical use:

        IStft istft;
        istft.prepare (config, sampleRate);
        istft.begin (numSamples, numChannels);

        for (int f = 0; f < numFrames; ++f)
            for (int ch = 0; ch < numChannels; ++ch)
                istft.addFrame (ch, f, modifiedBins (ch, f));   // packed layout

        auto audio = istft.finalise();

    The synthesis window is identical to the analysis window used by Stft and
    the output is normalised by the accumulated squared window, giving exact
    reconstruction for unmodified spectra (including the buffer edges).

    addFrame expects the same packed layout as Stft: 2*numBins values
    (bins 0..fftSize/2). Channel 0 drives the normalisation accumulator, so
    channel 0 must receive every frame that should appear in the output.
*/
class IStft
{
public:
    IStft() = default;

    /** Allocates. May be called repeatedly. */
    void prepare (const StftConfig& config, double sampleRate);

    /** Starts a new pass. All previous accumulation is discarded. */
    void begin (int outputLengthSamples, int numChannels);

    /** Adds one frame for one channel. */
    void addFrame (int channel, int frameIndex, const float* bins);

    /** Normalises and returns the result. */
    juce::AudioBuffer<float> finalise();

private:
    StftConfig config_ {};
    double sampleRate_ = 0.0;
    int fftSize_ = 0;
    int numBins_ = 0;
    int outputLength_ = 0;
    int numChannels_ = 0;

    std::unique_ptr<juce::dsp::FFT> fft_;
    std::vector<float> window_;

    juce::AudioBuffer<float> output_;
    std::vector<float> weight_;              // accumulated window^2 per sample
    mutable std::vector<float> work_;        // 2 * fftSize scratch

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IStft)
};

} // namespace zarlino::source::dsp
