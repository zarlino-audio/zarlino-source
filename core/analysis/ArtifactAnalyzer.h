#pragma once

#include "core/analysis/CandidateAnalyzer.h"
#include "core/analysis/FrameFeatures.h"
#include "core/extraction/SourceResult.h"

#include "dsp/TransientDetector.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <string>
#include <vector>

namespace zarlino::source::analysis
{

struct ArtifactReport
{
    extraction::QualityMetrics quality;

    /** Which optional metrics were actually computed. Unavailable metrics stay
        at their zero default and are flagged here instead of being invented
        (spec section 66). */
    bool phaseIntegrityComputed = false;
    bool transientIntegrityComputed = false;

    std::vector<std::string> notes;
};

/** Compares a separated target against the mixture region it came from and
    fills the quality metrics from measurable quantities only:

      - targetPreservation : mean energy ratio (target/mixture) over frames
                             where the target profile matched strongly
      - leakageEstimate    : share of target energy spent in frames where the
                             profile does not match
      - transientIntegrity : fraction of mixture onsets still present in the
                             target (matched within 25 ms)
      - spectralIntegrity  : 1 - short-lived-bin fraction of the target
                             (heuristic musical-noise proxy)
      - artifactScore      : combined leakage + musical-noise penalty
      - confidence         : profile confidence folded with the above

    Phase integrity and warble/chirp detection are not yet computed; the
    report says so explicitly. */
ArtifactReport analyseArtifacts (const juce::AudioBuffer<float>& target,
                                 const juce::AudioBuffer<float>& mixture,
                                 double sampleRate,
                                 const extraction::TargetProfile& profile,
                                 const CandidateResult& mixtureCandidates);

/** Fraction of active STFT bins that live in short temporal runs ("blobs"):
    bins whose consecutive active frames number <= maxRunFrames. With the
    default 2048/512 STFT an event smears over >= 4 frames, so single-frame
    isolation cannot occur - run length is the meaningful discriminator.
    Stationary content scores ~0; impulse-like bursts and musical-noise
    specks (which behave like short blobs) score high.

    Edge frames (the first and last `skipEdgeFrames`) are excluded: partial
    analysis windows at the file boundaries widen the spectrum and would
    otherwise register as spurious short runs. Heuristic, computed only.
    Returns 0 for empty/silent input. */
float burstyBinFraction (const float* mono, int numSamples, double sampleRate,
                         const dsp::StftConfig& config = {},
                         int maxRunFrames = 6,
                         int skipEdgeFrames = 2);

/** Fraction of reference onsets that have a candidate onset within the
    tolerance. Returns 1 when there is nothing to preserve. */
float onsetMatchRate (const std::vector<dsp::Onset>& reference,
                      const std::vector<dsp::Onset>& candidates,
                      double sampleRate,
                      double toleranceSeconds = 0.025);

} // namespace zarlino::source::analysis
