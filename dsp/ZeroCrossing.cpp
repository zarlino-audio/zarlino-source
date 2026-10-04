#include "dsp/ZeroCrossing.h"

namespace zarlino::source::dsp
{

int ZeroCrossing::countCrossings (const float* samples, int numSamples) noexcept
{
    if (samples == nullptr || numSamples < 2)
        return 0;

    int count = 0;
    int lastSign = 0;   // sign of the most recent non-zero sample

    for (int i = 0; i < numSamples; ++i)
    {
        const float v = samples[i];

        if (v > 0.0f)
        {
            if (lastSign < 0)
                ++count;

            lastSign = 1;
        }
        else if (v < 0.0f)
        {
            if (lastSign > 0)
                ++count;

            lastSign = -1;
        }
    }

    return count;
}

int ZeroCrossing::findNearestCrossing (const float* samples, int numSamples,
                                       int position, int searchRadius) noexcept
{
    if (samples == nullptr || numSamples < 2)
        return juce::jmax (0, position);

    position = juce::jlimit (1, numSamples - 1, position);
    searchRadius = juce::jmax (0, searchRadius);

    const auto isCrossing = [samples] (int j) noexcept
    {
        const float a = samples[j - 1];
        const float b = samples[j];
        return (a <= 0.0f && b > 0.0f) || (a >= 0.0f && b < 0.0f);
    };

    for (int r = 0; r <= searchRadius; ++r)
    {
        const int lo = position - r;
        const int hi = position + r;

        if (lo >= 1 && isCrossing (lo))
            return lo;

        if (hi <= numSamples - 1 && hi != lo && isCrossing (hi))
            return hi;
    }

    return position;
}

double ZeroCrossing::estimateFrequencyFromRate (const float* samples, int numSamples,
                                                double sampleRate) noexcept
{
    if (samples == nullptr || numSamples < 2 || sampleRate <= 0.0)
        return 0.0;

    const int crossings = countCrossings (samples, numSamples);
    const double durationSeconds = (double) numSamples / sampleRate;

    if (durationSeconds <= 0.0)
        return 0.0;

    return (double) crossings / (2.0 * durationSeconds);
}

} // namespace zarlino::source::dsp
