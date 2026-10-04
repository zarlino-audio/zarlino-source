#include "core/extraction/ExtractionEngine.h"

#include "core/analysis/ArtifactAnalyzer.h"
#include "core/analysis/SourceRefiner.h"
#include "core/analysis/CandidateAnalyzer.h"
#include "core/analysis/FrameFeatures.h"
#include "core/analysis/KeyAnalyzer.h"
#include "core/analysis/PitchAnalyzer.h"
#include "core/analysis/TempoAnalyzer.h"
#include "core/extraction/QueryInterpreter.h"
#include "dsp/FadeProcessor.h"

#include <cmath>

namespace zarlino::source::extraction
{

namespace
{

// Overall-progress mapping per stage (real stages, real fractions).
constexpr float kAnalyzeEnd = 0.15f;
constexpr float kTargetEnd = 0.30f;
constexpr float kSeparateEnd = 0.85f;
constexpr float kRefineEnd = 0.95f;

constexpr double kMinRegionSeconds = 0.1;
constexpr double kPreviewMaxSeconds = 30.0;

constexpr float kRefineThresholdArtifact = 0.5f;
constexpr float kRefineThresholdPreservation = 0.5f;

struct ProgressReporter
{
    const ProgressCallback& callback;

    void operator() (Stage stage, float stageFraction, float overall,
                     const juce::String& detail = {}) const
    {
        if (callback)
            callback ({ stage, juce::jlimit (0.0f, 1.0f, stageFraction),
                        juce::jlimit (0.0f, 1.0f, overall), detail });
    }
};

void addNote (SourceResult& result, const juce::String& note)
{
    result.diagnostics.notes.push_back (note.toStdString());
}

} // namespace

ExtractionEngine::ExtractionEngine (inference::ModelManager& models)
    : models_ (models)
{
}

void ExtractionEngine::addSeparator (inference::ISeparator* separator)
{
    if (separator != nullptr)
        separators_.push_back (separator);
}

bool ExtractionEngine::hasAvailableSeparator() const
{
    for (const auto* s : separators_)
        if (s != nullptr && s->isAvailable())
            return true;

    return false;
}

SourceResult ExtractionEngine::run (const ExtractionJob& job,
                                    const ProgressCallback& progress,
                                    const CancellationToken& cancellation) const
{
    SourceResult result;
    result.requestedDescription = job.query.description;

    const ProgressReporter report { progress };

    auto fail = [&result, &report] (const juce::String& message) -> SourceResult
    {
        result.successful = false;
        result.errorMessage = message.toStdString();
        report (Stage::idle, 0.0f, 0.0f, message);
        return std::move (result);
    };

    auto checkCancelled = [&result, &report, &cancellation] () -> bool
    {
        if (! cancellation.isCancelled())
            return false;

        result.successful = false;
        result.diagnostics.cancelled = true;
        result.errorMessage = "cancelled";
        report (Stage::idle, 0.0f, 0.0f, "cancelled");
        return true;
    };

    if (! job.source.isValid())
        return fail ("invalid source audio (empty buffer or unknown sample rate)");

    if (job.query.description.empty())
        return fail ("no source description given");

    const auto& source = job.source.buffer;
    const double sampleRate = job.source.sampleRate;
    const double sourceLength = job.source.getLengthSeconds();

    // ---- region -------------------------------------------------------------
    double startSeconds = job.query.startTime.value_or (0.0);
    double endSeconds = job.query.endTime.value_or (sourceLength);

    startSeconds = juce::jlimit (0.0, sourceLength, startSeconds);
    endSeconds = juce::jlimit (startSeconds, sourceLength, endSeconds);

    const int startSample = (int) std::llround (startSeconds * sampleRate);
    const int endSample = (int) std::llround (endSeconds * sampleRate);
    const int numSamples = endSample - startSample;

    if ((double) numSamples / sampleRate < kMinRegionSeconds)
        return fail ("region too short (minimum 0.1 s)");

    juce::AudioBuffer<float> mixture (source.getNumChannels(), numSamples);

    for (int ch = 0; ch < source.getNumChannels(); ++ch)
        mixture.copyFrom (ch, 0, source, ch, startSample, numSamples);

    result.metadata.sampleRate = sampleRate;
    result.metadata.numChannels = mixture.getNumChannels();
    result.metadata.startTime = startSeconds;
    result.metadata.endTime = endSeconds;

    // ---- memory precheck (spec section 46) -----------------------------------
    {
        const double inputBytes = (double) numSamples * mixture.getNumChannels() * sizeof (float);
        const double estimatedBytes = inputBytes * 4.0 + inputBytes;   // working set + output
        const double availableBytes = (double) juce::SystemStats::getMemorySizeInMegabytes() * 1024.0 * 1024.0;

        if (availableBytes > 0.0 && estimatedBytes > availableBytes * 0.5)
        {
            const auto requiredGb = juce::String (estimatedBytes / (1024.0 * 1024.0 * 1024.0), 2);
            const auto availableGb = juce::String (availableBytes / (1024.0 * 1024.0 * 1024.0), 2);

            auto message = "insufficient memory: Required: " + requiredGb + " GB / Available: " + availableGb + " GB";

            if (job.quality == inference::SeparationRequest::Quality::maximum)
                message << " - try High Quality mode";

            return fail (message);
        }

        addNote (result, "memory estimate: " + juce::String (estimatedBytes / (1024.0 * 1024.0), 1) + " MB working set");
    }

    // ---- stage: analyzing source ----------------------------------------------
    report (Stage::analyzingSource, 0.0f, 0.0f, "computing spectral features");

    if (checkCancelled())
        return std::move (result);

    const auto features = analysis::computeFeatures (mixture, sampleRate);

    addNote (result, "analysed " + juce::String (features.numFrames) + " STFT frames ("
                         + juce::String (features.fftSize) + "/" + juce::String (features.hopSize) + ")");

    report (Stage::analyzingSource, 1.0f, kAnalyzeEnd, "features ready");

    // ---- stage: finding target --------------------------------------------------
    report (Stage::findingTarget, 0.0f, kAnalyzeEnd, "interpreting description");

    if (checkCancelled())
        return std::move (result);

    const QueryInterpreter interpreter;
    const auto interpretation = interpreter.interpret (juce::String (job.query.description));

    result.diagnostics.candidateConfidence = 0.0f;
    addNote (result, "interpretation confidence: " + juce::String (interpretation.profile.interpretationConfidence, 2)
                         + " (" + interpretation.profile.summary + ")");

    const auto candidates = analysis::analyse (features, interpretation.profile);

    result.diagnostics.candidateConfidence = candidates.overallConfidence;

    if (! candidates.regions.empty())
    {
        const auto& best = candidates.regions.front();
        addNote (result, "target evidence in " + juce::String (best.startTime, 2) + " s .. "
                             + juce::String (best.endTime, 2) + " s (affinity "
                             + juce::String (best.affinity, 2) + ")");
    }
    else
    {
        addNote (result, "no strong candidate region found - broad search only");
    }

    report (Stage::findingTarget, 1.0f, kTargetEnd, "target located");

    // ---- stage: separating --------------------------------------------------------
    inference::ISeparator* separator = nullptr;

    for (auto* s : separators_)
        if (s != nullptr && s->isAvailable() && s->getCapabilities().queryConditioned)
        {
            separator = s;
            break;
        }

    if (separator == nullptr)
        for (auto* s : separators_)
            if (s != nullptr && s->isAvailable())
            {
                separator = s;
                break;
            }

    if (separator == nullptr)
    {
        // Spec section 57/66: real inference or an honest failure - nothing else.
        juce::String message;
        message << "No extraction model available. Zarlino Source performs real neural inference only; "
                   "place a validated model package under " << models_.getRoot().getFullPathName()
                << " (manifest + checkpoint, see MODEL_PROVENANCE.md). No placeholder output is produced.";

        return fail (message);
    }

    report (Stage::separating, 0.0f, kTargetEnd,
            "running " + separator->getName() + " " + separator->getVersion());

    if (checkCancelled())
        return std::move (result);

    inference::SeparationRequest request;
    request.sampleRate = sampleRate;
    request.region = { 0.0, (double) numSamples / sampleRate };
    request.profile = interpretation.profile;
    request.quality = job.quality;

    if (job.quality == inference::SeparationRequest::Quality::preview)
        request.maxSeconds = juce::jmin (kPreviewMaxSeconds, request.region.length());

    auto separationProgress = [&report] (const ProgressUpdate& update)
    {
        const float fraction = juce::jlimit (0.0f, 1.0f, update.stageFraction);

        report (Stage::separating, fraction,
                kTargetEnd + (kSeparateEnd - kTargetEnd) * fraction,
                update.detail);
    };

    const auto separationStart = juce::Time::getMillisecondCounterHiRes();
    auto separation = separator->separate (mixture, request, separationProgress, cancellation);
    result.diagnostics.separationSeconds = (juce::Time::getMillisecondCounterHiRes() - separationStart) / 1000.0;

    if (checkCancelled())
        return std::move (result);

    if (! separation.success)
    {
        addNote (result, "separator reported: " + separation.error);
        return fail ("separation failed: " + separation.error);
    }

    result.metadata.modelName = separation.modelName.toStdString();
    result.metadata.modelVersion = separation.modelVersion.toStdString();
    result.audio = std::move (separation.audio);

    report (Stage::separating, 1.0f, kSeparateEnd, "separation complete");

    // ---- stage: artifact analysis ---------------------------------------------------
    auto report2 = analysis::analyseArtifacts (result.audio, mixture, sampleRate,
                                               interpretation.profile, candidates);

    result.quality = report2.quality;

    for (const auto& note : report2.notes)
        addNote (result, juce::String (note));

    // ---- stage: refinement (max 3 passes, spec section 28) ----------------------------
    report (Stage::refining, 0.0f, kSeparateEnd, "checking quality criteria");

    analysis::AcceptanceThresholds thresholds;
    thresholds.maxArtifactScore = kRefineThresholdArtifact;
    thresholds.minTargetPreservation = kRefineThresholdPreservation;

    const bool refinementNeeded = ! thresholds.accepts (result.quality);

    if (! refinementNeeded)
    {
        addNote (result, "quality within thresholds - no refinement needed");
        report (Stage::refining, 1.0f, kRefineEnd, "no refinement needed");
    }
    else if (job.quality == inference::SeparationRequest::Quality::preview)
    {
        addNote (result, "refinement skipped in Preview mode");
        report (Stage::refining, 1.0f, kRefineEnd, "preview: refinement skipped");
    }
    else
    {
        addNote (result, "quality below thresholds - running profile-guided refinement");

        constexpr int kMaxRefinementPasses = 3;

        auto outcome = analysis::refine (
            result.audio, mixture, sampleRate,
            interpretation.profile, candidates, result.quality,
            thresholds, kMaxRefinementPasses,
            [&] (float fraction, const juce::String& message)
            {
                if (checkCancelled())
                    return false;

                const float clamped = juce::jlimit (0.0f, 1.0f, fraction);

                report (Stage::refining, clamped,
                        kSeparateEnd + (kRefineEnd - kSeparateEnd) * clamped,
                        message);
                return true;
            });

        result.diagnostics.refinementPasses = outcome.passesRun;

        for (const auto& note : outcome.notes)
            addNote (result, juce::String (note));

        if (outcome.passesAccepted > 0)
            report (Stage::refining, 1.0f, kRefineEnd, "refined");
        else
            report (Stage::refining, 1.0f, kRefineEnd, "refinement produced no improvement");
    }

    // ---- musical analysis of the extracted source (spec sections 31-35) ----------
    // Runs after refinement so the reported tempo/key/pitch describe the audio
    // the user actually receives.
    {
        const auto pitch = analysis::PitchAnalyzer::analyse (result.audio, sampleRate);

        double pitchSum = 0.0;
        int voicedFrames = 0;

        for (const auto& p : pitch.points)
            if (p.frequencyHz > 0.0)
            {
                pitchSum += p.frequencyHz;
                ++voicedFrames;
            }

        result.diagnostics.pitchConfidence = pitch.overallConfidence;

        if (voicedFrames > 0)
        {
            result.diagnostics.meanPitchHz = pitchSum / (double) voicedFrames;
            addNote (result, "pitch: " + juce::String (result.diagnostics.meanPitchHz, 1)
                                 + " Hz (confidence "
                                 + juce::String (pitch.overallConfidence, 2) + ")");
        }
        else
        {
            addNote (result, "pitch: no clear fundamental (unvoiced or polyphonic)");
        }

        const auto tempo = analysis::TempoAnalyzer::analyse (result.audio, sampleRate);

        result.diagnostics.tempoConfidence = tempo.confidence;

        if (tempo.valid)
        {
            result.diagnostics.tempoBpm = tempo.bpm;
            addNote (result, "tempo: " + juce::String (tempo.bpm, 1) + " BPM (confidence "
                                 + juce::String (tempo.confidence, 2) + ")");
        }
        else
        {
            addNote (result, "tempo: uncertain - not invented");
        }

        const auto key = analysis::KeyAnalyzer::analyse (result.audio, sampleRate);

        result.diagnostics.keyConfidence = key.confidence;
        result.diagnostics.keyDescription = key.valid ? key.description.toStdString()
                                                      : std::string ("Key uncertain");

        addNote (result, "key: " + key.description + " (confidence "
                             + juce::String (key.confidence, 2) + ")");
    }

    if (checkCancelled())
        return std::move (result);

    // ---- stage: finalizing -------------------------------------------------------------
    report (Stage::finalizing, 0.0f, kRefineEnd, "edge treatment");

    dsp::FadeProcessor::declickEdges (result.audio, sampleRate, 0.005);

    result.successful = true;
    report (Stage::finalizing, 1.0f, 1.0f, "done");

    return result;
}

} // namespace zarlino::source::extraction
