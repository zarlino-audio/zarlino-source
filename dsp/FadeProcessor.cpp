#include "dsp/FadeProcessor.h"

#include <cmath>

namespace zarlino::source::dsp
{

float FadeProcessor::gainIn (float t, Curve curve) noexcept
{
    t = juce::jlimit (0.0f, 1.0f, t);

    switch (curve)
    {
        case Curve::linear:
            return t;

        case Curve::equalPower:
            return (float) std::sin (0.5 * juce::MathConstants<double>::pi * (double) t);

        case Curve::cosine:
        default:
            return (float) (0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * (double) t));
    }
}

void FadeProcessor::applyFadeIn (juce::AudioBuffer<float>& buffer,
                                 int startSample,
                                 int fadeLength,
                                 Curve curve)
{
    const int numSamples = buffer.getNumSamples();

    if (numSamples <= 0 || fadeLength <= 0)
        return;

    if (startSample >= numSamples || startSample < 0)
        return;

    fadeLength = juce::jmin (fadeLength, numSamples - startSample);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);

        for (int i = 0; i < fadeLength; ++i)
        {
            const float t = (fadeLength <= 1) ? 1.0f : (float) i / (float) (fadeLength - 1);
            data[startSample + i] *= gainIn (t, curve);
        }
    }
}

void FadeProcessor::applyFadeOut (juce::AudioBuffer<float>& buffer,
                                  int endSample,
                                  int fadeLength,
                                  Curve curve)
{
    const int numSamples = buffer.getNumSamples();

    if (numSamples <= 0 || fadeLength <= 0)
        return;

    if (endSample <= 0 || endSample > numSamples)
        endSample = numSamples;

    fadeLength = juce::jmin (fadeLength, endSample);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);

        for (int i = 0; i < fadeLength; ++i)
        {
            const float t = (fadeLength <= 1) ? 0.0f : (float) i / (float) (fadeLength - 1);
            data[endSample - fadeLength + i] *= gainIn (1.0f - t, curve);
        }
    }
}

void FadeProcessor::declickEdges (juce::AudioBuffer<float>& buffer,
                                  double sampleRate,
                                  double fadeSeconds)
{
    if (sampleRate <= 0.0 || fadeSeconds <= 0.0)
        return;

    const int fadeLength = juce::jmax (1, (int) std::llround (fadeSeconds * sampleRate));

    applyFadeIn (buffer, 0, fadeLength, Curve::equalPower);
    applyFadeOut (buffer, buffer.getNumSamples(), fadeLength, Curve::equalPower);
}

} // namespace zarlino::source::dsp
