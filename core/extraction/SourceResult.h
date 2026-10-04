#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "core/extraction/Progress.h"

#include <string>
#include <vector>

namespace zarlino::source::extraction
{

/** Computed quality metrics. Every value is measured on the produced audio by
    ArtifactAnalyzer - none are invented or assumed. All values are 0..1. */
struct QualityMetrics
{
    float targetPreservation = 0.0f;    // how much of the target's energy survived
    float leakageEstimate = 0.0f;       // residual content from other sources (lower better)
    float transientIntegrity = 0.0f;    // transients preserved vs the mixture
    float spectralIntegrity = 0.0f;     // absence of musical-noise style holes
    float phaseIntegrity = 0.0f;        // phase coherence between frames
    float artifactScore = 0.0f;         // aggregate artifact severity (lower better)
    float confidence = 0.0f;            // overall confidence in the result
};

/** Metadata describing what was produced and how. */
struct SourceMetadata
{
    std::string requestedDescription;
    std::string modelName;              // model package that performed extraction
    std::string modelVersion;
    double sampleRate = 0.0;
    int numChannels = 0;
    double startTime = 0.0;             // region actually processed (seconds)
    double endTime = 0.0;
};

/** Diagnostics: what the pipeline observed. Useful for QA, the UI and the
    .zsource project file. */
struct ExtractionDiagnostics
{
    std::vector<std::string> notes;
    int refinementPasses = 0;
    bool cancelled = false;
    double separationSeconds = 0.0;     // wall-clock time spent in inference
    float candidateConfidence = 0.0f;   // from CandidateAnalyzer

    // Musical analysis of the extracted source (spec sections 31-35). Every
    // value is computed; uncertainty is explicit (0 BPM / "Key uncertain")
    // instead of invented.
    double tempoBpm = 0.0;              // 0 = tempo uncertain
    float tempoConfidence = 0.0f;
    double meanPitchHz = 0.0;           // mean over voiced frames; 0 = none
    float pitchConfidence = 0.0f;
    std::string keyDescription = "Key uncertain";
    float keyConfidence = 0.0f;
};

/** The fundamental pipeline object.

    Everything downstream (sampling assets, instruments, projects, export)
    consumes a SourceResult. */
struct SourceResult
{
    juce::AudioBuffer<float> audio;

    std::string requestedDescription;

    SourceMetadata metadata;
    QualityMetrics quality;
    ExtractionDiagnostics diagnostics;

    bool successful = false;
    std::string errorMessage;

    bool hasAudio() const noexcept
    {
        return audio.getNumSamples() > 0 && audio.getNumChannels() > 0;
    }
};

} // namespace zarlino::source::extraction
