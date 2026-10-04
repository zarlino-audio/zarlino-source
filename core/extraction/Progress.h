#pragma once

#include <juce_core/juce_core.h>

#include <atomic>
#include <functional>

namespace zarlino::source::extraction
{

/** Processing stages reported to the UI. These map to the progress lines
    shown during extraction ("Analyzing source...", "Finding target...", ...). */
enum class Stage
{
    idle,
    analyzingSource,     // format, level and spectral overview of the input
    findingTarget,       // query -> target profile, candidate regions
    separating,          // model inference
    refining,            // artifact-driven cleanup passes
    finalizing           // reconstruction, boundary treatment, metrics
};

inline const char* toString (Stage stage) noexcept
{
    switch (stage)
    {
        case Stage::analyzingSource: return "Analyzing source...";
        case Stage::findingTarget:   return "Finding target...";
        case Stage::separating:      return "Separating...";
        case Stage::refining:        return "Refining...";
        case Stage::finalizing:      return "Finalizing...";
        case Stage::idle:
        default:                     return "Idle";
    }
}

struct ProgressUpdate
{
    Stage stage = Stage::idle;
    float stageFraction = 0.0f;     // 0..1 within the current stage
    float overallFraction = 0.0f;   // 0..1 across the whole extraction
    juce::String detail;            // optional extra detail (never fabricated)
};

/** Progress callback. Called from worker threads only - never from an audio
    thread. Implementations must be cheap and may be called frequently. */
using ProgressCallback = std::function<void (const ProgressUpdate&)>;

/** Cooperative cancellation token shared between the UI and workers.
    Long-running operations poll this between chunks, stages and passes. */
class CancellationToken
{
public:
    void cancel() noexcept { cancelled_.store (true, std::memory_order_relaxed); }
    void reset() noexcept { cancelled_.store (false, std::memory_order_relaxed); }
    bool isCancelled() const noexcept { return cancelled_.load (std::memory_order_relaxed); }

private:
    std::atomic<bool> cancelled_ { false };
};

} // namespace zarlino::source::extraction
