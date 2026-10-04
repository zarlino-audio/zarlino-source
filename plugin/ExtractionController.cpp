#include "plugin/ExtractionController.h"

#include "core/audio/AudioFileIO.h"

namespace zarlino::source
{

ExtractionController::ExtractionController (juce::File modelsDirectory)
    : juce::Thread ("zarlino-extraction"),
      models_ (modelsDirectory),
      engine_ (models_)
{
    models_.refresh();
}

ExtractionController::~ExtractionController()
{
    token_.cancel();
    stopThread (5000);
    removeAllChangeListeners();
}

bool ExtractionController::loadFile (const juce::File& file, juce::String& errorOut)
{
    if (isRunning())
    {
        errorOut = "an extraction is running - wait for it to finish or cancel it first";
        return false;
    }

    auto outcome = audio::AudioFileIO::load (file);

    if (! outcome.success)
    {
        errorOut = outcome.error.isNotEmpty() ? outcome.error : juce::String ("could not load audio");
        return false;
    }

    document_ = std::move (outcome.document);

    state_.store (State::idle);
    screen_.store (Screen::extract);

    {
        const juce::SpinLock::ScopedLockType lock (snapshotLock_);
        result_ = {};
        progress_ = {};
    }

    sendChangeMessage();
    return true;
}

void ExtractionController::startExtraction (const extraction::SourceQuery& query,
                                            inference::SeparationRequest::Quality quality)
{
    if (isRunning() || ! document_.isValid())
        return;

    {
        const juce::SpinLock::ScopedLockType lock (requestLock_);
        pendingQuery_ = query;
        pendingQuality_ = quality;
    }

    token_.reset();
    state_.store (State::running);
    screen_.store (Screen::processing);
    sendChangeMessage();

    startThread();
}

void ExtractionController::cancelExtraction()
{
    if (isRunning())
        token_.cancel();
}

extraction::ProgressUpdate ExtractionController::getProgress() const
{
    const juce::SpinLock::ScopedLockType lock (snapshotLock_);
    return progress_;
}

extraction::SourceResult ExtractionController::getResult() const
{
    const juce::SpinLock::ScopedLockType lock (snapshotLock_);
    return result_;
}

void ExtractionController::run()
{
    extraction::SourceQuery query;
    inference::SeparationRequest::Quality quality;

    {
        const juce::SpinLock::ScopedLockType lock (requestLock_);
        query = pendingQuery_;
        quality = pendingQuality_;
    }

    extraction::ExtractionJob job;
    job.source = document_;      // message thread cannot mutate while running
    job.query = query;
    job.quality = quality;

    auto progressCallback = [this] (const extraction::ProgressUpdate& update)
    {
        {
            const juce::SpinLock::ScopedLockType lock (snapshotLock_);
            progress_ = update;
        }

        sendChangeMessage();
    };

    auto result = engine_.run (job, progressCallback, token_);

    const bool wasCancelled = result.diagnostics.cancelled;
    const bool wasSuccessful = result.successful;

    {
        const juce::SpinLock::ScopedLockType lock (snapshotLock_);
        result_ = std::move (result);
    }

    if (wasCancelled)
    {
        state_.store (State::cancelled);
        screen_.store (Screen::extract);
    }
    else if (wasSuccessful)
    {
        state_.store (State::finished);
        screen_.store (Screen::result);
    }
    else
    {
        state_.store (State::failed);
        screen_.store (Screen::extract);   // the error is shown on the extract screen
    }

    sendChangeMessage();
}

} // namespace zarlino::source
