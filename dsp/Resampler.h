#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace zarlino::source::dsp
{

/** Offline sample-rate conversion. Not real-time; allocates freely.

    Quality levels:
      - linear:       cheap, lowest quality (interactive preview fallback)
      - catmullRom:   good transient behaviour, moderate quality
      - windowedSinc: default; used for model input and final output
*/
class Resampler
{
public:
    enum class Quality { linear, catmullRom, windowedSinc };

    /** Output length for a given input length and rate pair. */
    static int outputLengthFor (int inputLength, double inputRate, double outputRate) noexcept;

    /** Resamples a whole buffer offline. Returns an empty buffer if the input
        is empty or the rates are invalid. If the rates match, returns a copy. */
    static juce::AudioBuffer<float> process (const juce::AudioBuffer<float>& input,
                                             double inputRate,
                                             double outputRate,
                                             Quality quality = Quality::windowedSinc);
};

} // namespace zarlino::source::dsp
