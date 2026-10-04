#include "dsp/Stft.h"

namespace zarlino::source::dsp
{

std::vector<float> makeStftWindow (StftConfig::WindowType type, int size)
{
    std::vector<float> w ((size_t) size, 1.0f);

    if (type == StftConfig::WindowType::rectangular)
        return w;

    const double twoPi = juce::MathConstants<double>::twoPi;

    for (int i = 0; i < size; ++i)
    {
        // Periodic Hann (denominator N, matching the STFT convention).
        const double hann = 0.5 * (1.0 - std::cos (twoPi * (double) i / (double) size));
        w[(size_t) i] = (type == StftConfig::WindowType::hannSqrt)
                            ? (float) std::sqrt (hann)
                            : (float) hann;
    }

    return w;
}

void Stft::prepare (const StftConfig& config, double sampleRate)
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

int Stft::getNumFrames (int numSamples) const noexcept
{
    if (numSamples <= 0 || config_.hopSize <= 0)
        return 0;

    return (numSamples + config_.hopSize - 1) / config_.hopSize;
}

const float* Stft::computeFrame (const float* samples, int numSamples, int frameIndex) const
{
    if (fft_ == nullptr)
    {
        jassertfalse;
        return nullptr;
    }

    const int start = getFrameStartSample (frameIndex);

    std::fill (work_.begin(), work_.end(), 0.0f);

    const int available = numSamples - start;

    if (available > 0)
    {
        const int n = juce::jmin (fftSize_, available);

        for (int i = 0; i < n; ++i)
            work_[(size_t) i] = samples[start + i] * window_[(size_t) i];
    }

    // Real-only forward transform. Only non-negative frequencies are computed;
    // the packed layout { re, im } for bins 0..N/2 remains valid.
    fft_->performRealOnlyForwardTransform (work_.data(), true);

    return work_.data();
}

void Stft::forEachFrame (const float* samples, int numSamples,
                         const std::function<void (int, const float*)>& fn) const
{
    const int numFrames = getNumFrames (numSamples);

    for (int f = 0; f < numFrames; ++f)
    {
        const float* bins = computeFrame (samples, numSamples, f);

        if (bins != nullptr)
            fn (f, bins);
    }
}

} // namespace zarlino::source::dsp
