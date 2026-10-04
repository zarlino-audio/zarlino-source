#include "core/analysis/PitchAnalyzer.h"

#include "dsp/ChannelConverter.h"

#include <cmath>

namespace zarlino::source::analysis
{

namespace
{
    constexpr int kDecimation = 4;
    constexpr int kDsFrame = 512;    // frame size at the decimated rate
    constexpr int kDsHop = 128;      // 512 / 4

    float hann (int n, int size)
    {
        return (float) (0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * (double) n
                                              / (double) (size - 1)));
    }
}

PitchTrack PitchAnalyzer::analyse (const juce::AudioBuffer<float>& buffer,
                                   double sampleRate,
                                   Settings settings)
{
    PitchTrack track;

    if (buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0 || sampleRate <= 0.0)
        return track;

    if (settings.frameSize < 256 || settings.hopSize <= 0 || settings.minHz <= 0.0
        || settings.maxHz <= settings.minHz)
        return track;

    // ---- mix down + decimate --------------------------------------------------------
    const auto mono = dsp::ChannelConverter::toMono (buffer);
    const int numSamples = mono.getNumSamples();
    const auto* src = mono.getReadPointer (0);

    const int dsLength = numSamples / kDecimation;
    std::vector<float> ds ((size_t) juce::jmax (1, dsLength), 0.0f);

    for (int i = 0; i < dsLength; ++i)
    {
        const int base = i * kDecimation;
        ds[(size_t) i] = 0.25f * (src[base] + src[base + 1] + src[base + 2] + src[base + 3]);
    }

    const double dsRate = sampleRate / (double) kDecimation;

    const int lagMin = juce::jmax (2, (int) std::floor (dsRate / settings.maxHz));
    const int lagMax = juce::jmin (kDsFrame - 2, (int) std::ceil (dsRate / settings.minHz));

    if (lagMax <= lagMin)
        return track;

    const int hopDs = juce::jmax (1, settings.hopSize / kDecimation);
    const int numFrames = juce::jmax (1, (dsLength - kDsFrame) / hopDs + 1);

    std::vector<float> frame ((size_t) kDsFrame, 0.0f);
    track.points.reserve ((size_t) numFrames);

    double confidenceSum = 0.0;

    for (int f = 0; f < numFrames; ++f)
    {
        const int start = f * hopDs;

        double energy = 0.0;

        for (int i = 0; i < kDsFrame; ++i)
        {
            const int idx = start + i;
            frame[(size_t) i] = idx < dsLength ? ds[(size_t) idx] : 0.0f;
            energy += (double) frame[(size_t) i] * frame[(size_t) i];
        }

        PitchPoint point;
        point.time = (double) (start * kDecimation) / sampleRate;

        if (energy <= 1.0e-9)
        {
            track.points.push_back (point);   // unvoiced, confidence 0
            continue;
        }

        double mean = 0.0;

        for (int i = 0; i < kDsFrame; ++i)
            mean += frame[(size_t) i];

        mean /= kDsFrame;

        std::vector<float> windowed ((size_t) kDsFrame);

        for (int i = 0; i < kDsFrame; ++i)
            windowed[(size_t) i] = (float) ((frame[(size_t) i] - mean) * hann (i, kDsFrame));

        // Normalised autocorrelation.
        double r0 = 0.0;

        for (int i = 0; i < kDsFrame; ++i)
            r0 += (double) windowed[(size_t) i] * windowed[(size_t) i];

        if (r0 <= 1.0e-12)
        {
            track.points.push_back (point);
            continue;
        }

        float bestValue = -1.0f;
        int bestLag = lagMin;

        for (int lag = lagMin; lag <= lagMax; ++lag)
        {
            double sum = 0.0;

            for (int i = 0; i < kDsFrame - lag; ++i)
                sum += (double) windowed[(size_t) i] * windowed[(size_t) (i + lag)];

            const float value = (float) (sum / r0);

            if (value > bestValue)
            {
                bestValue = value;
                bestLag = lag;
            }
        }

        // Parabolic refinement around the peak.
        double refinedLag = bestLag;

        if (bestLag > lagMin && bestLag < lagMax)
        {
            auto acf = [&] (int lag)
            {
                double sum = 0.0;

                for (int i = 0; i < kDsFrame - lag; ++i)
                    sum += (double) windowed[(size_t) i] * windowed[(size_t) (i + lag)];

                return sum / r0;
            };

            const double a = acf (bestLag - 1);
            const double b = acf (bestLag);
            const double c = acf (bestLag + 1);
            const double denom = a - 2.0 * b + c;

            if (std::abs (denom) > 1.0e-12)
                refinedLag = bestLag + juce::jlimit (-0.5, 0.5, 0.5 * (a - c) / denom);
        }

        point.confidence = juce::jlimit (0.0f, 1.0f, bestValue);

        if (bestValue >= settings.voicingThreshold && refinedLag > 0.0)
            point.frequencyHz = dsRate / refinedLag;

        confidenceSum += point.confidence;
        track.points.push_back (point);
    }

    track.overallConfidence = numFrames > 0 ? (float) (confidenceSum / (double) numFrames) : 0.0f;

    for (const auto& p : track.points)
        if (p.frequencyHz > 0.0)
        {
            track.anyVoiced = true;
            break;
        }

    return track;
}

} // namespace zarlino::source::analysis
