#include "core/analysis/TempoAnalyzer.h"

#include "dsp/ChannelConverter.h"
#include "dsp/TransientDetector.h"

#include <cmath>

namespace zarlino::source::analysis
{

TempoEstimate TempoAnalyzer::analyse (const juce::AudioBuffer<float>& buffer,
                                      double sampleRate,
                                      Settings settings)
{
    TempoEstimate estimate;

    if (buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0 || sampleRate <= 0.0)
        return estimate;

    if (settings.minBpm <= 0.0 || settings.maxBpm <= settings.minBpm)
        return estimate;

    // ---- onset-strength envelope ------------------------------------------------------
    const auto mono = dsp::ChannelConverter::toMono (buffer);
    const dsp::StftConfig stftConfig;

    dsp::Stft stft;
    stft.prepare (stftConfig, sampleRate);

    const auto strength = dsp::TransientDetector::onsetStrengthFunction (
        mono.getReadPointer (0), mono.getNumSamples(), stft);

    if (strength.size() < 8)
        return estimate;

    const double frameRate = sampleRate / (double) stftConfig.hopSize;

    // Mean-remove to make the autocorrelation meaningful.
    double mean = 0.0;

    for (const float v : strength)
        mean += v;

    mean /= (double) strength.size();

    std::vector<double> x (strength.size());

    for (size_t i = 0; i < strength.size(); ++i)
        x[i] = (double) strength[i] - mean;

    double r0 = 0.0;

    for (const double v : x)
        r0 += v * v;

    if (r0 <= 1.0e-12)
        return estimate;

    const int lagMin = juce::jmax (1, (int) std::floor (60.0 / settings.maxBpm * frameRate));
    const int lagMax = juce::jmin ((int) x.size() - 1, (int) std::ceil (60.0 / settings.minBpm * frameRate));

    if (lagMax <= lagMin)
        return estimate;

    double best = 0.0;
    int bestLag = lagMin;

    for (int lag = lagMin; lag <= lagMax; ++lag)
    {
        double sum = 0.0;

        for (size_t i = 0; i + (size_t) lag < x.size(); ++i)
            sum += x[i] * x[i + (size_t) lag];

        const double value = sum / r0;

        if (value > best)
        {
            best = value;
            bestLag = lag;
        }
    }

    estimate.confidence = juce::jlimit (0.0f, 1.0f, (float) best);

    if (best >= settings.minConfidence)
    {
        estimate.valid = true;
        estimate.bpm = 60.0 * frameRate / (double) bestLag;
    }

    return estimate;
}

} // namespace zarlino::source::analysis
