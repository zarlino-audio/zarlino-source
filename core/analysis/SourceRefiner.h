#pragma once

#include "core/analysis/CandidateAnalyzer.h"
#include "core/extraction/SourceResult.h"
#include "core/extraction/TargetProfile.h"
#include "dsp/Stft.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <functional>
#include <string>
#include <vector>

namespace zarlino::source::analysis
{

/** Progress/cancel hook used by the refinement stages. Called on the worker
    thread; return false to cancel (the current pass stops and the state is
    returned unchanged). */
using ProgressFn = std::function<bool (float fraction, const juce::String& message)>;

/** The "acceptable?" decision of spec section 28. */
struct AcceptanceThresholds
{
    float maxArtifactScore = 0.5f;      // artifactScore must be at or below
    float minTargetPreservation = 0.5f; // targetPreservation at or above

    bool accepts (const extraction::QualityMetrics& q) const noexcept
    {
        return q.artifactScore <= maxArtifactScore
            && q.targetPreservation >= minTargetPreservation;
    }
};

/** What one gate pass did, measured. */
struct SpectralGateStats
{
    bool completed = false;             // false = cancelled mid-pass
    float attenuatedBinFraction = 0.0f; // share of time-frequency bins gated below 0.9
    float meanGain = 1.0f;              // mean applied gain across all bins
};

/** One profile-guided blob-gate pass over `audio` (in place).

    Uses the same definition of "bursty" as the artifact analyzer: per bin, a
    bin frame is active while its magnitude (mono, max-of-channels) exceeds
    -40 dB relative to that bin's 95th-percentile peak; active runs of at most
    six frames are short-lived blobs - musical noise, clicks, truncated
    leakage - and are attenuated towards a 0.15 floor. Sustained or evolving
    content forms long runs and passes through untouched. Bands the target
    profile emphasises above its own mean are protected: their suppression is
    blended back towards unity, so target-dominant bands keep their content
    while non-target bands receive the full gate. Edge frames are excluded,
    matching the analyzer. The gain is a non-negative real value applied
    identically to every channel, so phase and the stereo image are preserved
    exactly.

    Deterministic computed DSP; no model is involved. */
SpectralGateStats applySpectralGatePass (juce::AudioBuffer<float>& audio,
                                         double sampleRate,
                                         const extraction::TargetProfile& profile,
                                         const dsp::StftConfig& config = {},
                                         ProgressFn progress = {});

struct RefinementOutcome
{
    int passesRun = 0;                  // attempts made, <= maxPasses
    int passesAccepted = 0;             // attempts kept
    bool improved = false;              // at least one pass accepted
    std::vector<std::string> notes;     // human-readable, honest reporting
};

/** Artifact-driven refinement loop (spec section 28).

    Repeats, at most `maxPasses` times: apply the spectral gate to a copy,
    re-measure quality with ArtifactAnalyzer, and keep the result only if the
    composite score (targetPreservation - artifactScore) improves by a margin
    AND target preservation does not drop beyond the guard. Stops early when
    quality is within `thresholds`, when a pass is rejected (no improvement),
    on cancellation, or when the pass budget is exhausted. `audio` and
    `quality` are updated in place for accepted passes only. */
RefinementOutcome refine (juce::AudioBuffer<float>& audio,
                          const juce::AudioBuffer<float>& mixture,
                          double sampleRate,
                          const extraction::TargetProfile& profile,
                          const CandidateResult& mixtureCandidates,
                          extraction::QualityMetrics& quality,
                          AcceptanceThresholds thresholds = {},
                          int maxPasses = 3,
                          ProgressFn progress = {});

} // namespace zarlino::source::analysis
