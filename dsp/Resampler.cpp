#include "dsp/Resampler.h"

#include <cmath>

namespace zarlino::source::dsp
{

int Resampler::outputLengthFor (int inputLength, double inputRate, double outputRate) noexcept
{
    if (inputLength <= 0)
        return 0;

    if (inputRate <= 0.0 || outputRate <= 0.0 || inputRate == outputRate)
        return inputLength;

    return (int) std::llround ((double) inputLength * outputRate / inputRate);
}

juce::AudioBuffer<float> Resampler::process (const juce::AudioBuffer<float>& input,
                                             double inputRate,
                                             double outputRate,
                                             Quality quality)
{
    const int numIn = input.getNumSamples();
    const int numCh = input.getNumChannels();

    if (numIn <= 0 || numCh <= 0 || inputRate <= 0.0 || outputRate <= 0.0)
        return {};

    if (inputRate == outputRate)
    {
        juce::AudioBuffer<float> copy;
        copy.makeCopyOf (input);
        return copy;
    }

    const int numOut = outputLengthFor (numIn, inputRate, outputRate);

    if (numOut <= 0)
        return {};

    const double speedRatio = inputRate / outputRate;   // input samples per output sample

    juce::AudioBuffer<float> out (numCh, numOut);

    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* in = input.getReadPointer (ch);
        float* dst = out.getWritePointer (ch);

        switch (quality)
        {
            case Quality::linear:
            {
                juce::LinearInterpolator interp;
                interp.reset();
                interp.process (speedRatio, in, dst, numOut, numIn, 0);
                break;
            }

            case Quality::catmullRom:
            {
                juce::CatmullRomInterpolator interp;
                interp.reset();
                interp.process (speedRatio, in, dst, numOut, numIn, 0);
                break;
            }

            case Quality::windowedSinc:
            default:
            {
                juce::WindowedSincInterpolator interp;
                interp.reset();
                interp.process (speedRatio, in, dst, numOut, numIn, 0);
                break;
            }
        }
    }

    return out;
}

} // namespace zarlino::source::dsp
