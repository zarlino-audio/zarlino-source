#pragma once

#include "core/extraction/Progress.h"
#include "core/extraction/SourceQuery.h"
#include "core/extraction/TargetProfile.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <utility>
#include <vector>

namespace zarlino::source::inference
{

/** What a separator can do, so the extraction engine can plan around it
    (spec sections 7/16/17).

    Class-based models (fixed stem sets, e.g. HTDemucs) set `stems` and leave
    `queryConditioned` false; the engine then selects / combines stems through
    the TargetProfile. Query-conditioned models set `queryConditioned` true and
    receive the profile directly. */
struct SeparatorCapabilities
{
    bool queryConditioned = false;
    bool multiStem = false;
    std::vector<juce::String> stems;
};

struct SeparationRequest
{
    double sampleRate = 0.0;

    /** Region of the mixture to process, in seconds. A zero-length range
        means "the whole buffer". */
    extraction::TimeRange region { 0.0, 0.0 };

    /** The interpreted target description (never a raw stem label). */
    extraction::TargetProfile profile;

    /** Quality mode, mapped to compute budgets (spec sections 21/22). */
    enum class Quality
    {
        preview,        // fastest model / reduced computation, for auditioning
        highQuality,    // normal final extraction
        maximum         // most expensive available offline processing
    };

    Quality quality = Quality::highQuality;

    /** Optional cap: process at most this many seconds (0 = no cap). Used by
        preview to bound latency on long inputs. */
    double maxSeconds = 0.0;
};

struct SeparationResult
{
    /** The separated target, at request.sampleRate. */
    juce::AudioBuffer<float> audio;
    double sampleRate = 0.0;

    bool success = false;

    /** Failure reason when success is false (e.g. "inference runtime
        unavailable", "model not validated"). Never produce fake audio. */
    juce::String error;

    juce::String modelName;
    juce::String modelVersion;

    /** Extra stems from multi-stem models (name -> audio), for hierarchical
        chains and refinement. The requested target is always `audio`. */
    std::vector<std::pair<juce::String, juce::AudioBuffer<float>>> otherStems;
};

/** Interchangeable separator interface (spec section 7).

    Implementations wrap real inference runtimes (ONNX Runtime, ...) around
    real, licence-verified model packages. If a separator cannot run it must
    fail via SeparationResult::error - silent fallbacks, dummy masks, copies
    of the input or placeholder audio are forbidden (spec sections 57/66).

    All calls happen on worker threads; the DAW audio thread never reaches
    this interface. */
class ISeparator
{
public:
    virtual ~ISeparator() = default;

    virtual juce::String getName() const = 0;
    virtual juce::String getVersion() const = 0;

    virtual SeparatorCapabilities getCapabilities() const { return {}; }

    /** True when the separator can actually run right now (runtime present,
        model present and validated). */
    virtual bool isAvailable() const = 0;

    virtual SeparationResult separate (const juce::AudioBuffer<float>& mixture,
                                       const SeparationRequest& request,
                                       const extraction::ProgressCallback& progress,
                                       const extraction::CancellationToken& cancellation) = 0;
};

} // namespace zarlino::source::inference
