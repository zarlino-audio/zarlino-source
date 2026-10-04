#include "plugin/PluginProcessor.h"

#include "plugin/PluginEditor.h"

namespace zarlino::source
{

namespace
{
    /** Local model-package root: the ZARLINO_SOURCE_MODELS environment variable
        when set, otherwise a "models" folder beside the executable (see
        models/README.md for the package format). */
    juce::File defaultModelsDirectory()
    {
        const auto env = juce::SystemStats::getEnvironmentVariable ("ZARLINO_SOURCE_MODELS", {});

        if (env.isNotEmpty())
        {
            const juce::File candidate { env };

            if (candidate.isDirectory())
                return candidate;
        }

        return juce::File::getSpecialLocation (juce::File::currentExecutableFile)
            .getParentDirectory()
            .getChildFile ("models");
    }
}

ZarlinoSourceAudioProcessor::ZarlinoSourceAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      extractionController_ (std::make_unique<ExtractionController> (defaultModelsDirectory()))
{
}

ZarlinoSourceAudioProcessor::~ZarlinoSourceAudioProcessor() = default;

void ZarlinoSourceAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    // A few seconds of capture ring; the worker drains continuously while the
    // capture is armed, so this does not need to hold a whole song.
    const int capacityFrames = (int) juce::jlimit ((juce::int64) (1 << 15),
                                                   (juce::int64) (1 << 21),
                                                   (juce::int64) (sampleRate * 8.0));

    captureBuffer_.prepare (2, capacityFrames);
    captureArmed_.store (false, std::memory_order_relaxed);
}

void ZarlinoSourceAudioProcessor::releaseResources()
{
    captureArmed_.store (false, std::memory_order_relaxed);
}

void ZarlinoSourceAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    audioRunningFlag_.store (true, std::memory_order_relaxed);

    if (captureArmed_.load (std::memory_order_relaxed))
        captureBuffer_.pushBlock (buffer, buffer.getNumSamples());

    // Audio passes through untouched. Extraction is an offline operation; no
    // model inference, allocation or locking ever happens on this thread.
}

juce::AudioProcessorEditor* ZarlinoSourceAudioProcessor::createEditor()
{
    return new ZarlinoSourceAudioProcessorEditor (*this);
}

void ZarlinoSourceAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Placeholder state: version stamp only. The .zsource project format
    // (Phase 9) supersedes this for full project persistence.
    juce::MemoryOutputStream stream (destData, false);
    stream.writeInt (1);
    stream.writeString (ZARLINO_SOURCE_VERSION);
}

void ZarlinoSourceAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream (data, (size_t) sizeInBytes, false);

    if (stream.getNumBytesRemaining() >= (juce::int64) sizeof (int))
    {
        const int version = stream.readInt();
        juce::ignoreUnused (version);

        if (stream.getNumBytesRemaining() > 0)
        {
            const auto storedVersion = stream.readString();
            juce::ignoreUnused (storedVersion);
        }
    }
}

} // namespace zarlino::source

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new zarlino::source::ZarlinoSourceAudioProcessor();
}
