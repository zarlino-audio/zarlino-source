#pragma once

#include "core/extraction/SourceQuery.h"
#include "core/extraction/SourceResult.h"

#include <juce_core/juce_core.h>

#include <string>
#include <vector>

namespace zarlino::source::project
{

/** .zsource project format (spec section 42).

    A project stores everything needed to reopen an extraction reproducibly:
    the source audio (referenced, or embedded in a sidecar folder), the query,
    the selection, processing settings, the model identity, the extraction
    diagnostics, sample metadata and the instrument mapping.

    Reproducibility rule: every referenced audio file carries its SHA256, and
    loading verifies it. A project whose audio changed on disk fails loudly
    instead of silently reopening the wrong sound. */

inline constexpr const char* kFormatId = "zarlino.zsource";
inline constexpr int kFormatVersion = 1;

/** A file the project depends on. `path` is stored relative to the project
    file when the two share a parent directory, so a project folder can move. */
struct AudioRef
{
    std::string path;
    std::string sha256;
    double sampleRate = 0.0;
    int numChannels = 0;
    juce::int64 lengthSamples = 0;

    bool isValid() const noexcept { return ! path.empty(); }
};

struct QuerySettings
{
    std::string text;
    std::string quality;              // "Preview" | "High Quality" | "Maximum"
    double startSeconds = -1.0;       // < 0 = the whole file
    double endSeconds = -1.0;
    bool hasSelection() const noexcept { return startSeconds >= 0.0 && endSeconds > startSeconds; }
};

struct ModelRef
{
    std::string name;
    std::string version;
    std::string sha256;
};

/** The diagnostics that make a result explainable months later. */
struct DiagnosticsRecord
{
    int refinementPasses = 0;
    bool cancelled = false;
    double separationSeconds = 0.0;
    float candidateConfidence = 0.0f;
    double tempoBpm = 0.0;
    float tempoConfidence = 0.0f;
    double meanPitchHz = 0.0;
    float pitchConfidence = 0.0f;
    std::string keyDescription = "Key uncertain";
    float keyConfidence = 0.0f;
    std::vector<std::string> notes;
};

struct SampleMeta
{
    std::string name;
    AudioRef audio;
    int rootNote = 60;                // MIDI note the sample plays back at
    double detectedPitchHz = 0.0;     // measured, 0 = unvoiced/unknown
    double startSeconds = 0.0;        // slice of the extracted source
    double lengthSeconds = 0.0;
};

struct InstrumentZone
{
    int sampleIndex = 0;              // index into samples
    int lowNote = 0;
    int highNote = 127;
    int rootNote = 60;
};

struct ProjectDocument
{
    std::string appVersion;
    std::string createdAt;

    AudioRef source;                  // the imported mixture
    QuerySettings query;
    ModelRef model;
    DiagnosticsRecord diagnostics;
    AudioRef extracted;               // the produced clean source
    std::vector<SampleMeta> samples;
    std::vector<InstrumentZone> instrumentMapping;

    /** True when the audio files live in a sidecar folder next to the project. */
    bool embedded = false;

    /** Path the document was loaded from / saved to (not serialised). */
    juce::File projectFile;

    /** Build a document from a completed extraction. */
    static ProjectDocument fromExtraction (const extraction::SourceResult& result,
                                           const extraction::SourceQuery& query,
                                           const AudioRef& sourceAudio);

    juce::var toVar() const;
    static ProjectDocument fromVar (const juce::var& value, juce::String& errorOut);

    /** Write the project, embedding referenced audio into
        "<name>.zsource.assets/" so the pair can be moved together. */
    juce::Result save (const juce::File& destination, bool embedAudio);

    /** Read a project. Does not touch the audio - call verifyAudio() for that. */
    static juce::Result load (const juce::File& source, ProjectDocument& out);

    /** Every referenced file exists and still hashes to the stored SHA256. */
    juce::Result verifyAudio() const;

    /** Absolute path for a stored (possibly relative) reference. */
    juce::File resolve (const std::string& storedPath) const;
};

/** SHA256 of a file as lowercase hex, or an empty string when unreadable. */
std::string hashFile (const juce::File& file);

/** Fill an AudioRef for a file on disk (hashes it and probes the audio). */
juce::Result describeAudioFile (const juce::File& file, AudioRef& out);

/** Store `path` relative to `baseDirectory` when possible. */
std::string relativise (const juce::File& path, const juce::File& baseDirectory);

} // namespace zarlino::source::project
