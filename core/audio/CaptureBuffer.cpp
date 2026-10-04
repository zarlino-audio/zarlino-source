#include "core/audio/CaptureBuffer.h"

namespace zarlino::source::audio
{

void CaptureBuffer::prepare (int numChannels, int capacityFrames)
{
    numChannels_ = juce::jmax (1, numChannels);
    capacity_ = juce::nextPowerOfTwo (juce::jmax (256, capacityFrames));

    fifo_.setTotalSize (capacity_);
    ring_.setSize (numChannels_, capacity_, false, true, false);
    ring_.clear();

    fifo_.reset();
}

void CaptureBuffer::clear() noexcept
{
    // Single-consumer reset: only safe when the producer is not writing
    // concurrently (the engine clears before arming a new capture).
    fifo_.reset();
    ring_.clear();
}

int CaptureBuffer::pushBlock (const juce::AudioBuffer<float>& block, int numSamples) noexcept
{
    if (! isPrepared() || numSamples <= 0)
        return 0;

    const int toWrite = juce::jmin (numSamples, block.getNumSamples());

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo_.prepareToWrite (toWrite, start1, size1, start2, size2);

    const int channelsToCopy = juce::jmin (numChannels_, block.getNumChannels());

    for (int ch = 0; ch < channelsToCopy; ++ch)
    {
        if (size1 > 0)
            ring_.copyFrom (ch, start1, block, ch, 0, size1);

        if (size2 > 0)
            ring_.copyFrom (ch, start2, block, ch, size1, size2);
    }

    // Channels that weren't supplied stay silent.
    for (int ch = channelsToCopy; ch < numChannels_; ++ch)
    {
        if (size1 > 0)
            ring_.clear (ch, start1, size1);

        if (size2 > 0)
            ring_.clear (ch, start2, size2);
    }

    fifo_.finishedWrite (size1 + size2);
    return size1 + size2;
}

int CaptureBuffer::getNumReady() const noexcept
{
    return fifo_.getNumReady();
}

int CaptureBuffer::popFrames (juce::AudioBuffer<float>& dest, int maxFrames) noexcept
{
    if (! isPrepared() || maxFrames <= 0)
        return 0;

    const int toRead = juce::jmin (maxFrames, dest.getNumSamples());

    if (toRead <= 0 || dest.getNumChannels() < numChannels_)
        return 0;

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo_.prepareToRead (toRead, start1, size1, start2, size2);

    for (int ch = 0; ch < numChannels_; ++ch)
    {
        if (size1 > 0)
            dest.copyFrom (ch, 0, ring_, ch, start1, size1);

        if (size2 > 0)
            dest.copyFrom (ch, size1, ring_, ch, start2, size2);
    }

    fifo_.finishedRead (size1 + size2);
    return size1 + size2;
}

} // namespace zarlino::source::audio
