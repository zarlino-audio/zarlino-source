#pragma once

#include "core/audio/AudioDocument.h"
#include "core/extraction/ExtractionEngine.h"
#include "core/extraction/Progress.h"
#include "core/extraction/SourceQuery.h"
#include "core/extraction/SourceResult.h"
#include "inference/ISeparator.h"
#include "inference/ModelManager.h"

#include <juce_events/juce_events.h>

#include <atomic>
#include <memory>

namespace zarlino::source
{

/** Owns the offline extraction pipeline for one plugin instance.

    Threading contract (spec section 43):
      - The DAW audio thread never touches this class.
      - loadFile/startExtraction/cancelExtraction run on the message thread.
      - The worker thread runs the ExtractionEngine and publishes progress and
        results through a spin-locked snapshot; it then notifies listeners via
        sendChangeMessage() (message thread).
      - getResult()/getProgress() deep-copy under the lock; call them from the
        message thread only (UI). */
class ExtractionController final : public juce::ChangeBroadcaster,
                                   private juce::Thread
{
public:
    enum class State
    {
        idle,
        running,
        finished,
        failed,
        cancelled
    };

    enum class Screen
    {
        extract,
        processing,
        result
    };

    explicit ExtractionController (juce::File modelsDirectory);
    ~ExtractionController() override;

    //==========================================================================
    // Message thread API
    //==========================================================================

    /** Loads a mixed audio file. Refused while an extraction is running. */
    bool loadFile (const juce::File& file, juce::String& errorOut);

    const audio::AudioDocument& getDocument() const noexcept { return document_; }
    bool hasDocument() const noexcept { return document_.isValid(); }

    /** Starts an offline extraction on the worker thread. */
    void startExtraction (const extraction::SourceQuery& query,
                          inference::SeparationRequest::Quality quality);

    /** Requests cooperative cancellation; safe to call any time. */
    void cancelExtraction();

    bool isRunning() const noexcept { return state_.load() == State::running; }

    //==========================================================================
    // Snapshots (message thread reads)
    //==========================================================================

    State getState() const noexcept { return state_.load(); }
    Screen getScreen() const noexcept { return screen_.load(); }

    extraction::ProgressUpdate getProgress() const;
    extraction::SourceResult getResult() const;

    bool hasAvailableSeparator() const noexcept { return engine_.hasAvailableSeparator(); }

    /** Models scanned by this instance (for settings/diagnostics UI). */
    const inference::ModelManager& getModels() const noexcept { return models_; }

private:
    void run() override;

    inference::ModelManager models_;
    extraction::ExtractionEngine engine_;
    audio::AudioDocument document_;

    std::atomic<State> state_ { State::idle };
    std::atomic<Screen> screen_ { Screen::extract };

    extraction::CancellationToken token_;

    juce::SpinLock requestLock_;
    extraction::SourceQuery pendingQuery_;
    inference::SeparationRequest::Quality pendingQuality_ = inference::SeparationRequest::Quality::highQuality;

    mutable juce::SpinLock snapshotLock_;
    extraction::ProgressUpdate progress_;
    extraction::SourceResult result_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExtractionController)
};

} // namespace zarlino::source
