#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace zarlino::source::audio
{

/** A loaded (or captured) mixture plus provenance. */
struct AudioDocument
{
    juce::AudioBuffer<float> buffer;
    double sampleRate = 0.0;
    juce::String name;          // display name (file name or capture label)
    juce::String sourcePath;    // empty for captures

    bool isValid() const noexcept
    {
        return buffer.getNumSamples() > 0 && buffer.getNumChannels() > 0 && sampleRate > 0.0;
    }

    double getLengthSeconds() const noexcept
    {
        return (sampleRate > 0.0) ? (double) buffer.getNumSamples() / sampleRate : 0.0;
    }
};

} // namespace zarlino::source::audio
