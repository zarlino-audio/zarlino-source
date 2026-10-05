#include "TestRunner.h"

#include "core/project/ProjectDocument.h"

#include <juce_audio_formats/juce_audio_formats.h>

// Project format tests (spec section 42). A .zsource project must reopen
// reproducibly: the query, selection, settings, model identity, diagnostics,
// sample metadata and instrument mapping all survive a round trip, and the
// referenced audio is verified by hash so a project can never silently reopen
// a different sound.

namespace
{
    using namespace zarlino::source::project;

    juce::File freshTempDir (const juce::String& name)
    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("zarlino-source-tests")
                       .getChildFile (name);

        if (dir.exists())
            dir.deleteRecursively();

        dir.createDirectory();
        return dir;
    }

    /** Write a real 16-bit WAV so hashing and format probing both work. */
    juce::File writeTestWav (const juce::File& file, double sampleRate = 48000.0, int lengthSamples = 4800)
    {
        file.getParentDirectory().createDirectory();
        file.deleteFile();

        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer (
            format.createWriterFor (new juce::FileOutputStream (file), sampleRate, 2, 16, {}, 0));

        if (writer == nullptr)
            return {};

        juce::AudioBuffer<float> buffer (2, lengthSamples);
        buffer.clear();

        for (int ch = 0; ch < 2; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);

            for (int i = 0; i < lengthSamples; ++i)
                data[i] = 0.25f * (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * (double) i / sampleRate);
        }

        writer->writeFromAudioSampleBuffer (buffer, 0, lengthSamples);
        writer.reset();

        return file;
    }

    AudioRef describe (const juce::File& file)
    {
        AudioRef ref;
        const auto result = describeAudioFile (file, ref);
        ZS_EXPECT (result.wasOk());
        return ref;
    }

    ProjectDocument makeDocument (const juce::File& sourceWav, const juce::File& extractedWav)
    {
        ProjectDocument doc;
        doc.appVersion = "0.1.0";
        doc.createdAt = "2026-10-05T00:00:00Z";
        doc.source = describe (sourceWav);
        doc.query.text = "the Rhodes";
        doc.query.quality = "Maximum";
        doc.query.startSeconds = 32.0;
        doc.query.endSeconds = 38.5;
        doc.model.name = "htdemucs";
        doc.model.version = "1.0.0";
        doc.model.sha256 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
        doc.extracted = describe (extractedWav);

        doc.diagnostics.refinementPasses = 3;
        doc.diagnostics.cancelled = false;
        doc.diagnostics.separationSeconds = 12.5;
        doc.diagnostics.candidateConfidence = 0.75f;
        doc.diagnostics.tempoBpm = 96.0;
        doc.diagnostics.tempoConfidence = 0.8f;
        doc.diagnostics.meanPitchHz = 261.63;
        doc.diagnostics.pitchConfidence = 0.9f;
        doc.diagnostics.keyDescription = "C major (uncertain)";
        doc.diagnostics.keyConfidence = 0.55f;
        doc.diagnostics.notes = { "region 32.0-38.5 s", "HQ mode" };

        SampleMeta sample;
        sample.name = "Rhodes C4";
        sample.audio = describe (extractedWav);
        sample.rootNote = 60;
        sample.detectedPitchHz = 261.63;
        sample.startSeconds = 0.0;
        sample.lengthSeconds = 0.1;
        doc.samples.push_back (sample);

        doc.instrumentMapping.push_back ({ 0, 48, 72, 60 });
        return doc;
    }
}

ZS_TEST (ProjectRoundTripPreservesEverything)
{
    const auto dir = freshTempDir ("roundtrip");
    const auto sourceWav = writeTestWav (dir.getChildFile ("mixture.wav"));
    const auto extractedWav = writeTestWav (dir.getChildFile ("rhodes.wav"), 44100.0, 2205);

    ZS_EXPECT (sourceWav.existsAsFile());
    ZS_EXPECT (extractedWav.existsAsFile());

    const auto doc = makeDocument (sourceWav, extractedWav);
    const auto projectFile = dir.getChildFile ("session.zsource");

    const auto saved = doc.save (projectFile, false);
    ZS_EXPECT (saved.wasOk());
    ZS_EXPECT (projectFile.existsAsFile());

    ProjectDocument reloaded;
    const auto loaded = ProjectDocument::load (projectFile, reloaded);
    ZS_EXPECT (loaded.wasOk());

    ZS_EXPECT (reloaded.query.text == "the Rhodes");
    ZS_EXPECT (reloaded.query.quality == "Maximum");
    ZS_EXPECT_NEAR (reloaded.query.startSeconds, 32.0, 1.0e-9);
    ZS_EXPECT_NEAR (reloaded.query.endSeconds, 38.5, 1.0e-9);
    ZS_EXPECT (reloaded.model.name == "htdemucs");
    ZS_EXPECT (reloaded.model.version == "1.0.0");
    ZS_EXPECT (reloaded.diagnostics.refinementPasses == 3);
    ZS_EXPECT_NEAR (reloaded.diagnostics.tempoBpm, 96.0, 1.0e-9);
    ZS_EXPECT (reloaded.diagnostics.notes.size() == 2);
    ZS_EXPECT (reloaded.diagnostics.notes[1] == "HQ mode");
    ZS_EXPECT (reloaded.samples.size() == 1);
    ZS_EXPECT (reloaded.samples[0].rootNote == 60);
    ZS_EXPECT_NEAR (reloaded.samples[0].detectedPitchHz, 261.63, 1.0e-6);
    ZS_EXPECT (reloaded.instrumentMapping.size() == 1);
    ZS_EXPECT (reloaded.instrumentMapping[0].lowNote == 48);
    ZS_EXPECT (reloaded.source.sha256 == doc.source.sha256);
    ZS_EXPECT (reloaded.extracted.sha256 == doc.extracted.sha256);

    // Same directory -> portable relative references, and audio verifies.
    ZS_EXPECT (! juce::File (juce::String (reloaded.source.path)).isAbsolutePath());
    ZS_EXPECT (reloaded.verifyAudio().wasOk());
}

ZS_TEST (ProjectRejectsWrongFormatAndFutureVersion)
{
    const auto dir = freshTempDir ("format");

    const auto notAProject = dir.getChildFile ("other.json");
    notAProject.replaceWithText ("{\"hello\":\"world\"}");

    ProjectDocument doc;
    ZS_EXPECT (ProjectDocument::load (notAProject, doc).failed());

    const auto future = dir.getChildFile ("future.zsource");
    future.replaceWithText ("{\"format\":\"zarlino.zsource\",\"format_version\":99}");

    juce::String error;
    auto parsed = juce::JSON::parse (future.loadFileAsString());
    auto futureDoc = ProjectDocument::fromVar (parsed, error);
    ZS_EXPECT (error.contains ("newer than this build"));
    (void) futureDoc;
}

ZS_TEST (ProjectVerifiesAudioByHash)
{
    const auto dir = freshTempDir ("verify");
    const auto sourceWav = writeTestWav (dir.getChildFile ("mixture.wav"));
    const auto extractedWav = writeTestWav (dir.getChildFile ("rhodes.wav"));

    const auto doc = makeDocument (sourceWav, extractedWav);
    const auto projectFile = dir.getChildFile ("session.zsource");
    ZS_EXPECT (doc.save (projectFile, false).wasOk());

    ProjectDocument reloaded;
    ZS_EXPECT (ProjectDocument::load (projectFile, reloaded).wasOk());
    ZS_EXPECT (reloaded.verifyAudio().wasOk());

    // The source audio changes underneath the project: reopening must fail
    // loudly instead of loading a different sound.
    writeTestWav (sourceWav, 48000.0, 9600);
    const auto mismatch = reloaded.verifyAudio();
    ZS_EXPECT (mismatch.failed());
    ZS_EXPECT (mismatch.getErrorMessage().contains ("sha256 mismatch"));

    // A missing file is reported by path, not silently ignored.
    sourceWav.deleteFile();
    const auto missing = reloaded.verifyAudio();
    ZS_EXPECT (missing.failed());
    ZS_EXPECT (missing.getErrorMessage().contains ("missing on disk"));
}

ZS_TEST (ProjectEmbeddingMakesAFolderSelfContained)
{
    const auto dir = freshTempDir ("embed");
    const auto originals = dir.getChildFile ("originals");

    const auto sourceWav = writeTestWav (originals.getChildFile ("mixture.wav"));
    const auto extractedWav = writeTestWav (originals.getChildFile ("rhodes.wav"));

    const auto doc = makeDocument (sourceWav, extractedWav);

    // Move the project to its own folder so we can prove it travels alone.
    const auto projectDir = dir.getChildFile ("song");
    projectDir.createDirectory();
    const auto projectFile = projectDir.getChildFile ("take01.zsource");

    const auto saved = doc.save (projectFile, true);
    ZS_EXPECT (saved.wasOk());

    const auto assets = projectDir.getChildFile ("take01.zsource.assets");
    ZS_EXPECT (assets.isDirectory());
    ZS_EXPECT (assets.getChildFile ("mixture.wav").existsAsFile());
    ZS_EXPECT (assets.getChildFile ("rhodes.wav").existsAsFile());

    // Destroy the originals: the project must still verify from its own folder.
    originals.deleteRecursively();

    ProjectDocument reloaded;
    ZS_EXPECT (ProjectDocument::load (projectFile, reloaded).wasOk());
    ZS_EXPECT (reloaded.embedded);
    ZS_EXPECT (reloaded.verifyAudio().wasOk());
    ZS_EXPECT (reloaded.query.text == "the Rhodes");
}

ZS_TEST (ProjectFromExtractionCarriesDiagnostics)
{
    using namespace zarlino::source::extraction;

    SourceResult result;
    result.metadata.modelName = "htdemucs";
    result.metadata.modelVersion = "1.0.0";
    result.metadata.sampleRate = 48000.0;
    result.metadata.numChannels = 2;
    result.diagnostics.refinementPasses = 2;
    result.diagnostics.tempoBpm = 120.0;
    result.diagnostics.keyDescription = "Key uncertain";
    result.diagnostics.notes = { "one note" };

    SourceQuery query;
    query.description = "the shaker";
    query.startTime = 1.0;
    query.endTime = 3.0;

    AudioRef sourceRef;
    sourceRef.path = "mixture.wav";
    sourceRef.sha256 = "abc";

    const auto doc = ProjectDocument::fromExtraction (result, query, sourceRef);

    ZS_EXPECT (doc.query.text == "the shaker");
    ZS_EXPECT (doc.query.hasSelection());
    ZS_EXPECT_NEAR (doc.query.startSeconds, 1.0, 1.0e-9);
    ZS_EXPECT (doc.model.name == "htdemucs");
    ZS_EXPECT (doc.diagnostics.refinementPasses == 2);
    ZS_EXPECT_NEAR (doc.diagnostics.tempoBpm, 120.0, 1.0e-9);
    ZS_EXPECT (doc.diagnostics.keyDescription == "Key uncertain");
    ZS_EXPECT (! doc.createdAt.empty());
}

ZS_TEST (ProjectHashesAreStableAndCaseFree)
{
    const auto dir = freshTempDir ("hash");
    const auto wav = writeTestWav (dir.getChildFile ("a.wav"));

    const auto first = hashFile (wav);
    const auto second = hashFile (wav);

    ZS_EXPECT (first.size() == 64);
    ZS_EXPECT (first == second);
    ZS_EXPECT (first == juce::String (first).toLowerCase());

    ZS_EXPECT (hashFile (dir.getChildFile ("does-not-exist.wav")).empty());
}
