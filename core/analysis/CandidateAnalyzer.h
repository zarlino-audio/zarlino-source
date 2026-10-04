#pragma once

#include "core/analysis/FrameFeatures.h"
#include "core/extraction/TargetProfile.h"

#include <vector>

namespace zarlino::source::analysis
{

struct CandidateConfig
{
    float minRegionAffinity = 0.45f;   // frames below this never start a region
    double minRegionSeconds = 0.25;    // regions shorter than this are dropped
    int maxRegions = 8;                // strongest N regions are kept
    int smoothingFrames = 5;           // moving average over frame affinities
};

struct CandidateRegion
{
    double startTime = 0.0;
    double endTime = 0.0;
    float affinity = 0.0f;             // mean affinity inside the region [0, 1]
};

/** Where the requested source most likely sits in the mixture (spec section
    18), scored purely from computed features against the TargetProfile. */
struct CandidateResult
{
    std::vector<float> frameAffinity;      // per-frame match (smoothed) [0, 1]
    std::vector<CandidateRegion> regions;  // ranked, strongest first
    float overallConfidence = 0.0f;        // best evidence * profile confidence
};

/** Scores a feature map against a TargetProfile.

    The per-frame score combines
      - band-proportion similarity (cosine of the low/mid/high distributions),
      - centroid proximity (only when the profile sets a centroid),
      - flatness vs. harmonicity (noise-like vs. tonal character).
    Regions are thresholded with hysteresis and ranked by mean affinity. */
CandidateResult analyse (const FeatureMap& features,
                         const extraction::TargetProfile& profile,
                         CandidateConfig config = {});

} // namespace zarlino::source::analysis
