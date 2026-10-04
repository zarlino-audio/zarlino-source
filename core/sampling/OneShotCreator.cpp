#include "core/sampling/OneShotCreator.h"

#include "dsp/ChannelConverter.h"
#include "dsp/FadeProcessor.h"
#include "dsp/TransientDetector.h"
#include "dsp/ZeroCrossing.h"

#include <cmath>

namespace zarlino::source::sampling
{

namespace
{
    double rmsAt (const juce::AudioBuffer<float>& buffer, int start, int length)
    {
        const int end = juce::jmin (buffer.getNumSamples(), start + length);
        double sum = 0.0;
        int count = 0;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* data = buffer.getReadPointer (ch);

            for (int i = start; i < end; ++i)
            {
                sum += (double) data[i] * (double) data[i];
                ++count;
            }
        }

        return count > 0 ? std::sqrt (sum / (double) count) : 0.0;
    }
}

OneShot OneShotCreator::create (const extraction::SourceResult& result, Settings settings)
{
    OneShot shot;

    if (! result.hasAudio() || result.metadata.sampleRate <= 0.0)
    {
        shot.error = "no audio to sample";
        return shot;
    }

    const auto& src = result.audio;
    const double sampleRate = result.metadata.sampleRate;
    const int numSamples = src.getNumSamples();

    // ---- strongest onset ------------------------------------------------------------
    const auto onsets = dsp::TransientDetector::detect (src, sampleRate);

    int startSample = 0;
    float bestStrength = 0.0f;

    for (const auto& onset : onsets)
        if (onset.strength > bestStrength)
        {
            bestStrength = onset.strength;
            startSample = onset.sampleIndex;
        }

    if (onsets.empty())
    {
        // No transient: start at the first frame with meaningful energy.
        constexpr int frame = 256;

        for (int i = 0; i + frame <= numSamples; i += frame)
            if (rmsAt (src, i, frame) > 1.0e-4)
            {
                startSample = i;
                break;
            }
    }

    // ---- usable tail ----------------------------------------------------------------
    const int maxSamples = (int) std::llround (settings.maxLengthSeconds * sampleRate);
    const int minSamples = (int) std::llround (settings.minLengthSeconds * sampleRate);

    const int searchLength = juce::jmin (maxSamples, numSamples - startSample);

    if (searchLength <= 0)
    {
        shot.error = "onset lies at the very end of the audio";
        return shot;
    }

    const float peak = src.getMagnitude (startSample, searchLength);

    if (peak <= 1.0e-6f)
    {
        shot.error = "one-shot region is silent";
        return shot;
    }

    const double threshold = (double) peak * std::pow (10.0, settings.tailThresholdDb / 20.0);
    const int maxEnd = juce::jmin (numSamples, startSample + maxSamples);
    const int minEnd = juce::jmin (numSamples, startSample + minSamples);
    constexpr int frame = 256;
    const int minSilenceFrames = juce::jmax (1, (int) std::ceil (settings.minTailSeconds * sampleRate / frame));

    int endSample = maxEnd;
    int silentRun = 0;

    for (int i = startSample + frame; i + frame <= maxEnd; i += frame)
    {
        if (i < minEnd)
            continue;

        if (rmsAt (src, i, frame) < threshold)
        {
            if (++silentRun >= minSilenceFrames)
            {
                endSample = i - (silentRun - 1) * frame;   // where the silence began
                break;
            }
        }
        else
        {
            silentRun = 0;
        }
    }

    endSample = juce::jlimit (minEnd, numSamples, endSample);

    // ---- zero-crossing snap ------------------------------------------------------------
    const auto mono = dsp::ChannelConverter::toMono (src);
    const auto* monoData = mono.getReadPointer (0);

    startSample = juce::jmax (0, dsp::ZeroCrossing::findNearestCrossing (monoData, numSamples,
                                                                         startSample, 256));
    endSample = juce::jlimit (startSample + 1, numSamples,
                              dsp::ZeroCrossing::findNearestCrossing (monoData, numSamples,
                                                                      endSample, 256));

    // ---- extract + short fades -----------------------------------------------------------
    shot.audio.setSize (src.getNumChannels(), endSample - startSample, false, true, false);

    for (int ch = 0; ch < src.getNumChannels(); ++ch)
        shot.audio.copyFrom (ch, 0, src, ch, startSample, endSample - startSample);

    const int fade = juce::jmax (1, (int) std::llround (settings.fadeSeconds * sampleRate));

    dsp::FadeProcessor::applyFadeIn (shot.audio, 0, fade);
    dsp::FadeProcessor::applyFadeOut (shot.audio, shot.audio.getNumSamples(), fade);

    shot.sampleRate = sampleRate;
    shot.startTime = (double) startSample / sampleRate;
    shot.endTime = (double) endSample / sampleRate;
    shot.success = true;
    return shot;
}

} // namespace zarlino::source::sampling
