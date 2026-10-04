#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace zarlino::source::audio
{

/** Lock-free SPSC capture ring for the plugin's real-time audio input.

    The audio thread calls pushBlock(); a background thread drains it with
    popFrames(). No allocation or locking happens on the audio thread after
    prepare(). If the consumer falls behind, incoming frames are dropped -
    the audio thread is never blocked.
*/
class CaptureBuffer
{
public:
    CaptureBuffer() = default;

    /** Allocates the ring. capacityFrames is rounded up to a power of two. */
    void prepare (int numChannels, int capacityFrames);

    void clear() noexcept;

    /** Audio-thread safe. Appends a block. Returns frames actually written
        (fewer than numSamples only if the ring is full). */
    int pushBlock (const juce::AudioBuffer<float>& block, int numSamples) noexcept;

    /** Number of frames currently available to the consumer. */
    int getNumReady() const noexcept;

    /** Consumer-side: copies up to maxFrames of the oldest pending frames into
        dest (must have this ring's channel count and at least maxFrames
        capacity). Returns the number of frames copied. */
    int popFrames (juce::AudioBuffer<float>& dest, int maxFrames) noexcept;

    int getNumChannels() const noexcept { return numChannels_; }
    int getCapacityFrames() const noexcept { return capacity_; }
    bool isPrepared() const noexcept { return numChannels_ > 0 && capacity_ > 0; }

private:
    juce::AbstractFifo fifo_ { 0 };
    juce::AudioBuffer<float> ring_;
    int numChannels_ = 0;
    int capacity_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CaptureBuffer)
};

} // namespace zarlino::source::audio
