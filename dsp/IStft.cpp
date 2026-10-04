#include "dsp/IStft.h"

namespace zarlino::source::dsp
{

void IStft::prepare (const StftConfig& config, double sampleRate)
{
    if (! config.isValid())
    {
        jassertfalse;
        return;
    }

    config_ = config;
    sampleRate_ = sampleRate;
    fftSize_ = config.getFftSize();
    numBins_ = fftSize_ / 2 + 1;

    fft_ = std::make_unique<juce::dsp::FFT> (config.fftOrder);
    window_ = makeStftWindow (config.window, fftSize_);
    work_.assign ((size_t) fftSize_ * 2, 0.0f);
}

void IStft::begin (int outputLengthSamples, int numChannels)
{
    outputLength_ = juce::jmax (0, outputLengthSamples);
    numChannels_ = juce::jmax (0, numChannels);

    output_.setSize (numChannels_, outputLength_, false, true, false);
    output_.clear();

    weight_.assign ((size_t) outputLength_, 0.0f);
}

void IStft::addFrame (int channel, int frameIndex, const float* bins)
{
    if (fft_ == nullptr || bins == nullptr)
    {
        jassertfalse;
        return;
    }

    if (! juce::isPositiveAndBelow (channel, numChannels_))
        return;

    std::fill (work_.begin(), work_.end(), 0.0f);

    for (int k = 0; k < numBins_; ++k)
    {
        work_[(size_t) (2 * k)]     = bins[2 * k];
        work_[(size_t) (2 * k + 1)] = bins[2 * k + 1];
    }

    // DC and Nyquist must be real for a real-valued signal.
    work_[1] = 0.0f;
    work_[(size_t) (2 * (numBins_ - 1) + 1)] = 0.0f;

    // JUCE's real-only inverse is normalised by 1/N and mirrors the
    // conjugate half internally. Output reals are in work_[0 .. fftSize).
    fft_->performRealOnlyInverseTransform (work_.data());

    const int start = frameIndex * config_.hopSize;
    auto* out = output_.getWritePointer (channel);

    for (int i = 0; i < fftSize_; ++i)
    {
        const int idx = start + i;

        if (idx >= outputLength_)
            break;

        if (idx < 0)
            continue;

        const float w = window_[(size_t) i];
        out[idx] += w * work_[(size_t) i];

        if (channel == 0)
            weight_[(size_t) idx] += w * w;
    }
}

juce::AudioBuffer<float> IStft::finalise()
{
    for (int ch = 0; ch < output_.getNumChannels(); ++ch)
    {
        auto* out = output_.getWritePointer (ch);

        for (int i = 0; i < outputLength_; ++i)
        {
            const float wgt = weight_[(size_t) i];
            out[i] = (wgt > 1.0e-6f) ? (out[i] / wgt) : 0.0f;
        }
    }

    return std::move (output_);
}

} // namespace zarlino::source::dsp
