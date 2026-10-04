#include "core/analysis/FrameFeatures.h"

#include "dsp/ChannelConverter.h"

#include <cmath>

namespace zarlino::source::analysis
{

namespace
{
    constexpr float kPowerFloor = 1.0e-20f;
}

FeatureMap computeFeatures (const float* mono, int numSamples, double sampleRate,
                            const FeatureConfig& config)
{
    FeatureMap map;
    map.sampleRate = sampleRate;
    map.fftSize = config.stft.getFftSize();
    map.hopSize = config.stft.hopSize;

    if (mono == nullptr || numSamples <= 0 || sampleRate <= 0.0 || ! config.stft.isValid())
        return map;

    dsp::Stft stft;
    stft.prepare (config.stft, sampleRate);

    const int numBins = stft.getNumBins();
    const double binHz = sampleRate / (double) config.stft.getFftSize();

    // Split bins: [1, lowBinCount] = low band, (lowBinCount, highBinCount] = mid,
    // above = high. DC (bin 0) counts towards total energy but no band.
    const int lowBinCount = juce::jlimit (1, numBins - 2,
                                          (int) std::floor (config.lowSplitHz / binHz));
    const int highBinCount = juce::jlimit (lowBinCount + 1, numBins - 1,
                                           (int) std::ceil (config.highSplitHz / binHz));

    map.numFrames = stft.getNumFrames (numSamples);
    map.frames.reserve ((size_t) map.numFrames);

    stft.forEachFrame (mono, numSamples, [&] (int, const float* bins)
    {
        FrameFeature f;

        double sumAll = 0.0;
        double sumLog = 0.0;
        double weightedHz = 0.0;

        for (int b = 0; b < numBins; ++b)
        {
            const float p = dsp::Stft::powerOf (bins, b);

            sumAll += (double) p;
            sumLog += std::log ((double) p + kPowerFloor);
            weightedHz += (double) p * ((double) b * binHz);

            if (b == 0)
                continue;

            if (b <= lowBinCount)
                f.lowEnergy += p;
            else if (b <= highBinCount)
                f.midEnergy += p;
            else
                f.highEnergy += p;
        }

        f.totalEnergy = (float) sumAll;
        f.centroidHz = sumAll > 0.0 ? (float) (weightedHz / sumAll) : 0.0f;

        const double arith = sumAll / (double) numBins;
        const double geo = std::exp (sumLog / (double) numBins);
        f.flatness = arith > 0.0 ? (float) juce::jlimit (0.0, 1.0, geo / arith) : 0.0f;

        map.frames.push_back (f);
    });

    return map;
}

FeatureMap computeFeatures (const juce::AudioBuffer<float>& buffer, double sampleRate,
                            const FeatureConfig& config)
{
    if (buffer.getNumChannels() <= 0 || buffer.getNumSamples() <= 0)
        return {};

    const auto mono = dsp::ChannelConverter::toMono (buffer);

    return computeFeatures (mono.getReadPointer (0), mono.getNumSamples(), sampleRate, config);
}

} // namespace zarlino::source::analysis
