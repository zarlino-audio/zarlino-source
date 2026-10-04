#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace zarlino::source::dsp
{

/** Fade and de-click helpers (offline). */
class FadeProcessor
{
public:
    enum class Curve
    {
        linear,
        equalPower,     // sin/cos quarter-wave
        cosine          // raised cosine
    };

    /** Gain of a fade-in ramp at normalised position t in [0, 1]. */
    static float gainIn (float t, Curve curve) noexcept;

    /** Multiplies samples [startSample, startSample + fadeLength) by a ramp
        going 0 -> 1. The range is clamped to the buffer. */
    static void applyFadeIn (juce::AudioBuffer<float>& buffer,
                             int startSample,
                             int fadeLength,
                             Curve curve = Curve::equalPower);

    /** Multiplies samples [endSample - fadeLength, endSample) by a ramp
        going 1 -> 0. The range is clamped to the buffer. */
    static void applyFadeOut (juce::AudioBuffer<float>& buffer,
                              int endSample,
                              int fadeLength,
                              Curve curve = Curve::equalPower);

    /** Short equal-power fades at both edges of the buffer (2 ms or the given
        length). Call on assets that should never click when triggered. */
    static void declickEdges (juce::AudioBuffer<float>& buffer,
                              double sampleRate,
                              double fadeSeconds = 0.002);
};

} // namespace zarlino::source::dsp
