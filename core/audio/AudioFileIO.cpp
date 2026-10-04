#include "core/audio/AudioFileIO.h"

#include <limits>

namespace zarlino::source::audio
{

juce::StringArray AudioFileIO::getSupportedExtensions()
{
    return { "wav", "aif", "aiff", "flac" };
}

bool AudioFileIO::isSupportedFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    const auto ext = file.getFileExtension().trimCharactersAtStart (".").toLowerCase();
    return getSupportedExtensions().contains (ext);
}

AudioFileIO::LoadOutcome AudioFileIO::load (const juce::File& file)
{
    LoadOutcome out;

    if (! file.existsAsFile())
    {
        out.error = "File not found: " + file.getFullPathName();
        return out;
    }

    if (! isSupportedFile (file))
    {
        out.error = "Unsupported format: '" + file.getFileExtension()
                  + "'. Supported: " + getSupportedExtensions().joinIntoString (", ");
        return out;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr)
    {
        out.error = "The file could not be decoded (corrupt or unsupported encoding).";
        return out;
    }

    if (reader->lengthInSamples <= 0 || reader->numChannels <= 0 || reader->sampleRate <= 0.0)
    {
        out.error = "The file contains no audio.";
        return out;
    }

    if (reader->lengthInSamples > (juce::int64) std::numeric_limits<int>::max())
    {
        out.error = "The file is too long to process.";
        return out;
    }

    const int numChannels = juce::jmin (2, (int) reader->numChannels);
    const int numSamples = (int) reader->lengthInSamples;

    out.document.buffer.setSize (numChannels, numSamples, false, true, false);
    out.document.buffer.clear();

    const bool useLeft = true;
    const bool useRight = (numChannels > 1) && reader->numChannels > 1;

    if (! reader->read (&out.document.buffer, 0, numSamples, 0, useLeft, useRight))
    {
        out.error = "Reading the audio data failed.";
        return out;
    }

    out.document.sampleRate = reader->sampleRate;
    out.document.name = file.getFileNameWithoutExtension();
    out.document.sourcePath = file.getFullPathName();
    out.success = true;

    return out;
}

bool AudioFileIO::writeWav (const juce::File& file,
                            const juce::AudioBuffer<float>& buffer,
                            double sampleRate,
                            BitDepth depth,
                            juce::String* errorOut)
{
    const auto fail = [errorOut] (const juce::String& message)
    {
        if (errorOut != nullptr)
            *errorOut = message;

        return false;
    };

    if (buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0)
        return fail ("Nothing to write: the buffer is empty.");

    if (sampleRate <= 0.0)
        return fail ("Invalid sample rate.");

    file.getParentDirectory().createDirectory();

    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);

    if (! static_cast<juce::FileOutputStream*> (stream.get())->openedOk())
        return fail ("Could not open the destination file: " + file.getFullPathName());

    juce::WavAudioFormat wav;

    juce::AudioFormatWriterOptions options;
    options = options.withSampleRate (sampleRate)
                     .withNumChannels (buffer.getNumChannels());

    switch (depth)
    {
        case BitDepth::pcm16:
            options = options.withBitsPerSample (16);
            break;

        case BitDepth::pcm24:
            options = options.withBitsPerSample (24);
            break;

        case BitDepth::float32:
            options = options.withBitsPerSample (32)
                             .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
            break;
    }

    auto writer = wav.createWriterFor (stream, options);

    if (writer == nullptr)
        return fail ("The WAV writer could not be created for the requested format.");

    const bool ok = writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());

    writer.reset();     // flush and close before checking the result

    if (! ok)
        return fail ("Writing the WAV data failed.");

    return true;
}

} // namespace zarlino::source::audio
