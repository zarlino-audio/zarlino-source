#include "dsp/TransientDetector.h"
#include "dsp/ChannelConverter.h"

#include <algorithm>
#include <cmath>

namespace zarlino::source::dsp
{

namespace
{
    float medianOf (std::vector<float>& values)
    {
        if (values.empty())
            return 0.0f;

        const size_t mid = values.size() / 2;
        std::nth_element (values.begin(), values.begin() + (long) mid, values.end());
        float med = values[mid];

        if ((values.size() % 2) == 0)
        {
            std::nth_element (values.begin(), values.begin() + (long) (mid - 1), values.end());
            med = 0.5f * (med + values[mid - 1]);
        }

        return med;
    }
}

std::vector<float> TransientDetector::onsetStrengthFunction (const float* samples,
                                                             int numSamples,
                                                             const Stft& stft)
{
    const int numFrames = stft.getNumFrames (numSamples);
    const int numBins = stft.getNumBins();

    std::vector<float> flux ((size_t) numFrames, 0.0f);
    std::vector<float> prevMag ((size_t) numBins, 0.0f);
    std::vector<float> curMag ((size_t) numBins, 0.0f);

    for (int f = 0; f < numFrames; ++f)
    {
        const float* bins = stft.computeFrame (samples, numSamples, f);

        if (bins == nullptr)
            break;

        for (int k = 0; k < numBins; ++k)
        {
            // sqrt compression: gentler than raw magnitude, keeps quiet
            // details visible without the noise sensitivity of log.
            curMag[(size_t) k] = std::sqrt (Stft::magnitudeOf (bins, k));
        }

        if (f > 0)
        {
            float sum = 0.0f;

            for (int k = 0; k < numBins; ++k)
            {
                const float diff = curMag[(size_t) k] - prevMag[(size_t) k];

                if (diff > 0.0f)
                    sum += diff;
            }

            flux[(size_t) f] = sum;
        }

        std::swap (prevMag, curMag);
    }

    return flux;
}

std::vector<Onset> TransientDetector::detect (const float* samples,
                                              int numSamples,
                                              double sampleRate,
                                              const StftConfig& config,
                                              Settings settings)
{
    std::vector<Onset> onsets;

    if (samples == nullptr || numSamples <= 0 || sampleRate <= 0.0)
        return onsets;

    Stft stft;
    stft.prepare (config, sampleRate);

    auto flux = onsetStrengthFunction (samples, numSamples, stft);
    const int numFrames = (int) flux.size();

    if (numFrames < 3)
        return onsets;

    float maxFlux = 0.0f;

    for (float v : flux)
        maxFlux = juce::jmax (maxFlux, v);

    if (maxFlux <= 0.0f)
        return onsets;

    // Adaptive threshold: median + k * 1.4826 * MAD inside a local window.
    const int halfWindow = juce::jmax (2, settings.medianWindowFrames / 2);
    std::vector<float> scratch;
    scratch.reserve ((size_t) (2 * halfWindow + 1));

    std::vector<int> candidates;

    for (int f = 1; f < numFrames - 1; ++f)
    {
        const int lo = juce::jmax (0, f - halfWindow);
        const int hi = juce::jmin (numFrames - 1, f + halfWindow);

        scratch.clear();

        for (int i = lo; i <= hi; ++i)
            scratch.push_back (flux[(size_t) i]);

        const float med = medianOf (scratch);

        for (auto& v : scratch)
            v = std::abs (v - med);

        const float mad = medianOf (scratch);
        const float threshold = med + settings.thresholdMads * 1.4826f * mad;

        const float value = flux[(size_t) f];

        const bool isLocalMax = value >= flux[(size_t) (f - 1)]
                             && value >= flux[(size_t) (f + 1)];

        if (isLocalMax && value > threshold && value > 0.02f * maxFlux)
            candidates.push_back (f);
    }

    // Enforce minimum spacing, keeping the strongest peak in each cluster.
    const int minSpacingSamples = juce::jmax (1, (int) ((double) settings.minSpacingMs * sampleRate / 1000.0));
    const int minSpacingFrames = juce::jmax (1, minSpacingSamples / juce::jmax (1, stft.getHopSize()));

    std::vector<int> kept;
    kept.reserve (candidates.size());

    // Walk left to right; when two candidates are too close, swap in the stronger one.
    for (int f : candidates)
    {
        if (! kept.empty() && (f - kept.back()) < minSpacingFrames)
        {
            if (flux[(size_t) f] > flux[(size_t) kept.back()])
                kept.back() = f;
        }
        else
        {
            kept.push_back (f);
        }
    }

    const int centreOffset = stft.getFftSize() / 2;

    for (int f : kept)
    {
        Onset onset;
        onset.sampleIndex = juce::jlimit (0, numSamples - 1, f * stft.getHopSize() + centreOffset);
        onset.strength = flux[(size_t) f] / maxFlux;
        onsets.push_back (onset);
    }

    return onsets;
}

std::vector<Onset> TransientDetector::detect (const juce::AudioBuffer<float>& buffer,
                                              double sampleRate,
                                              const StftConfig& config,
                                              Settings settings)
{
    auto mono = ChannelConverter::toMono (buffer);

    if (mono.getNumSamples() <= 0)
        return {};

    return detect (mono.getReadPointer (0), mono.getNumSamples(), sampleRate, config, settings);
}

} // namespace zarlino::source::dsp
