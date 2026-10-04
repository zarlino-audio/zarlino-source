#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace zarlino::source::dsp
{

/** Channel-count utilities (offline). */
class ChannelConverter
{
public:
    enum class MonoMode
    {
        average,    // sum / numChannels
        left,
        right,
        mid         // (L + R) / 2
    };

    /** Up-mixes/copies to exactly two channels (mono is duplicated). */
    static juce::AudioBuffer<float> toStereo (const juce::AudioBuffer<float>& input);

    /** Down-mixes to a single channel. */
    static juce::AudioBuffer<float> toMono (const juce::AudioBuffer<float>& input,
                                            MonoMode mode = MonoMode::average);

    /** Mid/side decomposition. `mid` and `side` are resized as needed.
        With a mono source: mid = source, side = zero. */
    static void toMidSide (const juce::AudioBuffer<float>& input,
                           juce::AudioBuffer<float>& mid,
                           juce::AudioBuffer<float>& side);

    /** Recombines mid/side to stereo: L = M + S, R = M - S. */
    static juce::AudioBuffer<float> fromMidSide (const juce::AudioBuffer<float>& mid,
                                                 const juce::AudioBuffer<float>& side);
};

} // namespace zarlino::source::dsp
