#pragma once

#include "core/audio/AudioDocument.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace zarlino::source::audio
{

/** Audio file loading and WAV writing, with validation and clear errors. */
class AudioFileIO
{
public:
    enum class BitDepth { pcm16, pcm24, float32 };

    struct LoadOutcome
    {
        AudioDocument document;
        bool success = false;
        juce::String error;
    };

    /** File extensions the loader accepts (lower case, without the dot). */
    static juce::StringArray getSupportedExtensions();

    static bool isSupportedFile (const juce::File& file);

    /** Loads and validates an audio file. Never throws; validation failures
        are reported through LoadOutcome::error. */
    static LoadOutcome load (const juce::File& file);

    /** Writes a WAV file at the requested bit depth. Returns false and sets
        errorOut on failure. */
    static bool writeWav (const juce::File& file,
                          const juce::AudioBuffer<float>& buffer,
                          double sampleRate,
                          BitDepth depth,
                          juce::String* errorOut = nullptr);

private:
    AudioFileIO() = delete;
};

} // namespace zarlino::source::audio
