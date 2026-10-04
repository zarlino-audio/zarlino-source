#include "dsp/ChannelConverter.h"

namespace zarlino::source::dsp
{

juce::AudioBuffer<float> ChannelConverter::toStereo (const juce::AudioBuffer<float>& input)
{
    const int numCh = input.getNumChannels();
    const int numSamples = input.getNumSamples();

    if (numCh <= 0 || numSamples <= 0)
        return {};

    if (numCh == 2)
    {
        juce::AudioBuffer<float> copy;
        copy.makeCopyOf (input);
        return copy;
    }

    if (numCh == 1)
    {
        juce::AudioBuffer<float> out (2, numSamples);
        out.copyFrom (0, 0, input, 0, 0, numSamples);
        out.copyFrom (1, 0, input, 0, 0, numSamples);
        return out;
    }

    // More than two channels: first two are taken as L/R.
    juce::AudioBuffer<float> out (2, numSamples);
    out.copyFrom (0, 0, input, 0, 0, numSamples);
    out.copyFrom (1, 0, input, 1, 0, numSamples);
    return out;
}

juce::AudioBuffer<float> ChannelConverter::toMono (const juce::AudioBuffer<float>& input,
                                                   MonoMode mode)
{
    const int numCh = input.getNumChannels();
    const int numSamples = input.getNumSamples();

    if (numCh <= 0 || numSamples <= 0)
        return {};

    juce::AudioBuffer<float> out (1, numSamples);
    auto* dst = out.getWritePointer (0);

    switch (mode)
    {
        case MonoMode::left:
            out.copyFrom (0, 0, input, 0, 0, numSamples);
            break;

        case MonoMode::right:
            out.copyFrom (0, 0, input, juce::jmin (1, numCh - 1), 0, numSamples);
            break;

        case MonoMode::mid:
            if (numCh == 1)
            {
                out.copyFrom (0, 0, input, 0, 0, numSamples);
            }
            else
            {
                const auto* l = input.getReadPointer (0);
                const auto* r = input.getReadPointer (1);

                for (int i = 0; i < numSamples; ++i)
                    dst[i] = 0.5f * (l[i] + r[i]);
            }
            break;

        case MonoMode::average:
        default:
            if (numCh == 1)
            {
                out.copyFrom (0, 0, input, 0, 0, numSamples);
            }
            else
            {
                juce::FloatVectorOperations::clear (dst, numSamples);

                for (int ch = 0; ch < numCh; ++ch)
                    juce::FloatVectorOperations::add (dst, input.getReadPointer (ch), numSamples);

                const float scale = 1.0f / (float) numCh;
                juce::FloatVectorOperations::multiply (dst, scale, numSamples);
            }
            break;
    }

    return out;
}

void ChannelConverter::toMidSide (const juce::AudioBuffer<float>& input,
                                  juce::AudioBuffer<float>& mid,
                                  juce::AudioBuffer<float>& side)
{
    const int numCh = input.getNumChannels();
    const int numSamples = input.getNumSamples();

    if (numCh <= 0 || numSamples <= 0)
    {
        mid.setSize (0, 0);
        side.setSize (0, 0);
        return;
    }

    mid.setSize (1, numSamples, false, true, false);
    side.setSize (1, numSamples, false, true, false);

    auto* m = mid.getWritePointer (0);
    auto* s = side.getWritePointer (0);
    const auto* l = input.getReadPointer (0);
    const auto* r = (numCh >= 2) ? input.getReadPointer (1) : l;

    for (int i = 0; i < numSamples; ++i)
    {
        m[i] = 0.5f * (l[i] + r[i]);
        s[i] = 0.5f * (l[i] - r[i]);
    }
}

juce::AudioBuffer<float> ChannelConverter::fromMidSide (const juce::AudioBuffer<float>& mid,
                                                        const juce::AudioBuffer<float>& side)
{
    const int numSamples = mid.getNumSamples();

    if (numSamples <= 0)
        return {};

    juce::AudioBuffer<float> out (2, numSamples);
    auto* l = out.getWritePointer (0);
    auto* r = out.getWritePointer (1);

    const auto* m = mid.getReadPointer (0);
    const auto* s = (side.getNumSamples() >= numSamples && side.getNumChannels() > 0)
                        ? side.getReadPointer (0)
                        : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        const float sv = (s != nullptr) ? s[i] : 0.0f;
        l[i] = m[i] + sv;
        r[i] = m[i] - sv;
    }

    return out;
}

} // namespace zarlino::source::dsp
