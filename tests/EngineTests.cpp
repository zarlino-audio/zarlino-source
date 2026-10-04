#include "TestRunner.h"

#include "core/extraction/ExtractionEngine.h"

#include <cmath>

namespace
{
    constexpr double kSr = 48000.0;

    juce::AudioBuffer<float> makeTone (double seconds, double frequency = 440.0, float amplitude = 0.5f)
    {
        juce::AudioBuffer<float> buffer (2, (int) std::llround (seconds * kSr));

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.getWritePointer (ch)[i] = amplitude
                    * (float) std::sin (juce::MathConstants<double>::twoPi * frequency * (double) i / kSr);

        return buffer;
    }

    zarlino::source::audio::AudioDocument makeDocument (double seconds)
    {
        zarlino::source::audio::AudioDocument doc;
        doc.buffer = makeTone (seconds);
        doc.sampleRate = kSr;
        doc.name = "test";
        return doc;
    }

    /** TEST DOUBLE ONLY. Lives in tests/, never linked into the product. It
        exercises engine orchestration (progress, stages, region handling).
        Separators in the product must run real inference - placeholder or
        copy-input fallbacks are forbidden there (spec section 57). */
    class TestStubSeparator final : public zarlino::source::inference::ISeparator
    {
    public:
        juce::String getName() const override { return "test-stub"; }
        juce::String getVersion() const override { return "0"; }
        bool isAvailable() const override { return true; }

        zarlino::source::inference::SeparationResult separate (
            const juce::AudioBuffer<float>& mixture,
            const zarlino::source::inference::SeparationRequest& request,
            const zarlino::source::extraction::ProgressCallback&,
            const zarlino::source::extraction::CancellationToken&) override
        {
            zarlino::source::inference::SeparationResult r;
            r.success = true;
            r.sampleRate = request.sampleRate;
            r.modelName = "test-stub";
            r.modelVersion = "0";
            r.audio.makeCopyOf (mixture);
            return r;
        }
    };

    juce::File makeModelsRoot()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("zarlino_engine_tests_models");
        dir.createDirectory();
        return dir;
    }
}

ZS_TEST (EngineRefusesWithoutSeparatorHonestly)
{
    namespace ex = zarlino::source::extraction;
    namespace inf = zarlino::source::inference;

    inf::ModelManager models (makeModelsRoot());
    ex::ExtractionEngine engine (models);

    ex::ExtractionJob job;
    job.source = makeDocument (1.0);
    job.query.description = "Rhodes";

    ex::CancellationToken token;
    int progressCalls = 0;

    const auto result = engine.run (job, [&progressCalls] (const ex::ProgressUpdate&) { ++progressCalls; }, token);

    ZS_EXPECT (! result.successful);
    ZS_EXPECT (juce::String (result.errorMessage.c_str()).containsIgnoreCase ("model"));
    ZS_EXPECT (result.audio.getNumSamples() == 0);      // never placeholder audio
    ZS_EXPECT (progressCalls > 0);
    ZS_EXPECT (result.diagnostics.candidateConfidence > 0.0f);   // analysis still ran
}

ZS_TEST (EnginePipelineWithSeparator)
{
    namespace ex = zarlino::source::extraction;
    namespace inf = zarlino::source::inference;

    inf::ModelManager models (makeModelsRoot());
    ex::ExtractionEngine engine (models);

    TestStubSeparator stub;
    engine.addSeparator (&stub);

    ZS_EXPECT (engine.hasAvailableSeparator());

    ex::ExtractionJob job;
    job.source = makeDocument (1.0);
    job.query.description = "Rhodes";

    ex::CancellationToken token;
    std::vector<ex::ProgressUpdate> updates;

    const auto result = engine.run (job,
                                    [&updates] (const ex::ProgressUpdate& u) { updates.push_back (u); },
                                    token);

    ZS_EXPECT (result.successful);
    ZS_EXPECT (juce::String (result.errorMessage.c_str()).isEmpty());
    ZS_EXPECT (result.audio.getNumSamples() == job.source.buffer.getNumSamples());
    ZS_EXPECT (result.metadata.modelName == "test-stub");
    ZS_EXPECT (result.quality.targetPreservation > 0.8f);
    ZS_EXPECT (result.quality.transientIntegrity > 0.9f);
    ZS_EXPECT (! result.diagnostics.notes.empty());

    // Musical analysis ran on the extracted source (computed, not invented).
    ZS_EXPECT_NEAR (result.diagnostics.meanPitchHz, 440.0, 5.0);
    ZS_EXPECT (result.diagnostics.pitchConfidence > 0.3f);
    ZS_EXPECT (result.diagnostics.tempoBpm == 0.0);          // no onsets -> uncertain
    ZS_EXPECT (! result.diagnostics.keyDescription.empty());

    bool sawAnalyzing = false, sawFinding = false, sawSeparating = false, sawFinalizing = false;
    float lastOverall = -1.0f;
    bool monotonic = true;

    for (const auto& u : updates)
    {
        if (u.stage == ex::Stage::analyzingSource) sawAnalyzing = true;
        if (u.stage == ex::Stage::findingTarget)   sawFinding = true;
        if (u.stage == ex::Stage::separating)      sawSeparating = true;
        if (u.stage == ex::Stage::finalizing)      sawFinalizing = true;

        if (u.overallFraction + 1.0e-4f < lastOverall)
            monotonic = false;

        lastOverall = juce::jmax (lastOverall, u.overallFraction);
    }

    ZS_EXPECT (sawAnalyzing);
    ZS_EXPECT (sawFinding);
    ZS_EXPECT (sawSeparating);
    ZS_EXPECT (sawFinalizing);
    ZS_EXPECT (monotonic);
    ZS_EXPECT (lastOverall > 0.99f);
}

ZS_TEST (EngineRespectsRegion)
{
    namespace ex = zarlino::source::extraction;
    namespace inf = zarlino::source::inference;

    inf::ModelManager models (makeModelsRoot());
    ex::ExtractionEngine engine (models);

    TestStubSeparator stub;
    engine.addSeparator (&stub);

    ex::ExtractionJob job;
    job.source = makeDocument (2.0);
    job.query.description = "Rhodes";
    job.query.startTime = 0.5;
    job.query.endTime = 1.0;

    ex::CancellationToken token;
    const auto result = engine.run (job, {}, token);

    ZS_EXPECT (result.successful);
    ZS_EXPECT (std::abs (result.audio.getNumSamples() - (int) (0.5 * kSr)) < 2);
    ZS_EXPECT_NEAR (result.metadata.startTime, 0.5, 0.001);
    ZS_EXPECT_NEAR (result.metadata.endTime, 1.0, 0.001);
}

ZS_TEST (EngineRejectsTinyRegion)
{
    namespace ex = zarlino::source::extraction;
    namespace inf = zarlino::source::inference;

    inf::ModelManager models (makeModelsRoot());
    ex::ExtractionEngine engine (models);

    TestStubSeparator stub;
    engine.addSeparator (&stub);

    ex::ExtractionJob job;
    job.source = makeDocument (1.0);
    job.query.description = "Rhodes";
    job.query.startTime = 0.0;
    job.query.endTime = 0.05;    // 50 ms - below the 100 ms minimum

    ex::CancellationToken token;
    const auto result = engine.run (job, {}, token);

    ZS_EXPECT (! result.successful);
    ZS_EXPECT (juce::String (result.errorMessage.c_str()).containsIgnoreCase ("region"));
}

ZS_TEST (EngineCancelsBeforeWork)
{
    namespace ex = zarlino::source::extraction;
    namespace inf = zarlino::source::inference;

    inf::ModelManager models (makeModelsRoot());
    ex::ExtractionEngine engine (models);

    TestStubSeparator stub;
    engine.addSeparator (&stub);

    ex::ExtractionJob job;
    job.source = makeDocument (1.0);
    job.query.description = "Rhodes";

    ex::CancellationToken token;
    token.cancel();

    const auto result = engine.run (job, {}, token);

    ZS_EXPECT (! result.successful);
    ZS_EXPECT (result.diagnostics.cancelled);
    ZS_EXPECT (juce::String (result.errorMessage.c_str()).containsIgnoreCase ("cancel"));
}
