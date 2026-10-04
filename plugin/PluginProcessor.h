#pragma once

#include "core/audio/CaptureBuffer.h"
#include "plugin/ExtractionController.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <memory>

namespace zarlino::source
{

/** Zarlino Source - VST3 + standalone.

    Real-time thread rules (enforced by design, verified by tests):
      - processBlock() never allocates, locks, loads models or runs inference.
      - DAW audio capture is a lock-free SPSC ring; a worker drains it.
      - All extraction work happens on background threads; results arrive on
        the message thread.
*/
class ZarlinoSourceAudioProcessor final : public juce::AudioProcessor
{
public:
    ZarlinoSourceAudioProcessor();
    ~ZarlinoSourceAudioProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Zarlino Source"; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    /** Arms/disarms DAW input capture. Called from the message thread. */
    void setCaptureArmed (bool shouldBeArmed) noexcept
    {
        captureArmed_.store (shouldBeArmed, std::memory_order_relaxed);
    }

    bool isCaptureArmed() const noexcept
    {
        return captureArmed_.load (std::memory_order_relaxed);
    }

    /** The capture ring - producer = audio thread, consumer = worker. */
    audio::CaptureBuffer& getCaptureBuffer() noexcept { return captureBuffer_; }

    /** Offline extraction worker + document. Message/worker threads only -
        never touched from processBlock. */
    ExtractionController& getExtractionController() noexcept { return *extractionController_; }

    /** Called from the audio thread once per block with the capture state; a
        worker uses this to know when the host is actually running. */
    std::atomic<bool> audioRunningFlag_ { false };

private:
    audio::CaptureBuffer captureBuffer_;
    std::atomic<bool> captureArmed_ { false };
    std::unique_ptr<ExtractionController> extractionController_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZarlinoSourceAudioProcessor)
};

} // namespace zarlino::source
