#include "core/project/ProjectDocument.h"

#include "core/audio/AudioDocument.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>

#include <algorithm>
#include <cctype>

namespace zarlino::source::project
{

namespace
{
    juce::String toJuce (const std::string& s) { return juce::String (s); }
    std::string toStd (const juce::String& s) { return s.toStdString(); }

    /** Path-shape test that does not depend on a JUCE API signature: a drive
        letter, a UNC prefix or a leading slash means absolute. */
    bool isAbsoluteStoredPath (const std::string& path)
    {
        if (path.size() >= 3 && std::isalpha ((unsigned char) path[0]) != 0
            && path[1] == ':' && (path[2] == '\\' || path[2] == '/'))
            return true;

        if (path.size() >= 2 && path[0] == '\\' && path[1] == '\\')
            return true;

        return ! path.empty() && (path[0] == '/' || path[0] == '\\');
    }

    juce::String hexOf (const juce::MemoryBlock& block, int byteLimit)
    {
        // sha256Hex only takes files; format the digest ourselves.
        juce::String out;
        const auto* bytes = static_cast<const unsigned char*> (block.getData());
        const int n = juce::jmin (byteLimit, (int) block.getSize());
        for (int i = 0; i < n; ++i)
            out << juce::String::toHexString ((int) bytes[i]).paddedLeft ('0', 2);

        return out;
    }
}

std::string hashFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return {};

    juce::FileInputStream in (file);

    if (! in.openedOk())
        return {};

    juce::SHA256 hasher (in);
    juce::MemoryBlock digest;
    hasher.finish (digest);

    return toStd (hexOf (digest, 32));
}

juce::Result describeAudioFile (const juce::File& file, AudioRef& out)
{
    if (! file.existsAsFile())
        return juce::Result::fail ("audio file not found: " + file.getFullPathName());

    auto sha = hashFile (file);

    if (sha.empty())
        return juce::Result::fail ("could not hash audio file: " + file.getFullPathName());

    out.path = toStd (file.getFullPathName());
    out.sha256 = sha;

    // Probe the format so the project records real properties, not guesses.
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    if (auto reader = std::unique_ptr<juce::AudioFormatReader> (formats.createReaderFor (file)))
    {
        out.sampleRate = reader->sampleRate;
        out.numChannels = (int) reader->numChannels;
        out.lengthSamples = (juce::int64) reader->lengthInSamples;
    }
    else
    {
        return juce::Result::fail ("unreadable audio file: " + file.getFullPathName());
    }

    return juce::Result::ok();
}

std::string relativise (const juce::File& path, const juce::File& baseDirectory)
{
    const auto relative = path.getRelativePathFrom (baseDirectory);

    if (relative.isEmpty() || relative.startsWith (".."))
        return toStd (path.getFullPathName());

    return toStd (relative);
}

namespace
{
    juce::var audioRefToVar (const AudioRef& ref)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("path", toJuce (ref.path));
        obj->setProperty ("sha256", toJuce (ref.sha256));
        obj->setProperty ("sample_rate", ref.sampleRate);
        obj->setProperty ("channels", ref.numChannels);
        obj->setProperty ("length_samples", (double) ref.lengthSamples);
        return juce::var (obj);
    }

    AudioRef audioRefFromVar (const juce::var& value)
    {
        AudioRef ref;

        if (auto* obj = value.getDynamicObject())
        {
            ref.path = toStd (obj->getProperty ("path").toString());
            ref.sha256 = toStd (obj->getProperty ("sha256").toString());
            ref.sampleRate = (double) obj->getProperty ("sample_rate");
            ref.numChannels = (int) obj->getProperty ("channels");
            ref.lengthSamples = (juce::int64) (double) obj->getProperty ("length_samples");
        }

        return ref;
    }

    juce::Array<juce::var> stringsToArray (const std::vector<std::string>& values)
    {
        juce::Array<juce::var> out;

        for (const auto& v : values)
            out.add (toJuce (v));

        return out;
    }

    std::vector<std::string> arrayToStrings (const juce::var& value)
    {
        std::vector<std::string> out;

        if (const auto* arr = value.getArray())
            for (const auto& v : *arr)
                out.push_back (toStd (v.toString()));

        return out;
    }
}

juce::var ProjectDocument::toVar() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("format", toJuce (kFormatId));
    root->setProperty ("format_version", kFormatVersion);
    root->setProperty ("app_version", toJuce (appVersion));
    root->setProperty ("created_at", toJuce (createdAt));
    root->setProperty ("embedded_audio", embedded);

    root->setProperty ("source_audio", audioRefToVar (source));

    auto* queryObj = new juce::DynamicObject();
    queryObj->setProperty ("text", toJuce (query.text));
    queryObj->setProperty ("quality", toJuce (query.quality));
    queryObj->setProperty ("start_seconds", query.startSeconds);
    queryObj->setProperty ("end_seconds", query.endSeconds);
    root->setProperty ("query", juce::var (queryObj));

    auto* modelObj = new juce::DynamicObject();
    modelObj->setProperty ("name", toJuce (model.name));
    modelObj->setProperty ("version", toJuce (model.version));
    modelObj->setProperty ("sha256", toJuce (model.sha256));
    root->setProperty ("model", juce::var (modelObj));

    auto* diag = new juce::DynamicObject();
    diag->setProperty ("refinement_passes", diagnostics.refinementPasses);
    diag->setProperty ("cancelled", diagnostics.cancelled);
    diag->setProperty ("separation_seconds", diagnostics.separationSeconds);
    diag->setProperty ("candidate_confidence", diagnostics.candidateConfidence);
    diag->setProperty ("tempo_bpm", diagnostics.tempoBpm);
    diag->setProperty ("tempo_confidence", diagnostics.tempoConfidence);
    diag->setProperty ("mean_pitch_hz", diagnostics.meanPitchHz);
    diag->setProperty ("pitch_confidence", diagnostics.pitchConfidence);
    diag->setProperty ("key_description", toJuce (diagnostics.keyDescription));
    diag->setProperty ("key_confidence", diagnostics.keyConfidence);
    diag->setProperty ("notes", juce::var (stringsToArray (diagnostics.notes)));
    root->setProperty ("diagnostics", juce::var (diag));

    root->setProperty ("extracted_audio", audioRefToVar (extracted));

    juce::Array<juce::var> sampleArray;

    for (const auto& s : samples)
    {
        auto* sampleObj = new juce::DynamicObject();
        sampleObj->setProperty ("name", toJuce (s.name));
        sampleObj->setProperty ("audio", audioRefToVar (s.audio));
        sampleObj->setProperty ("root_note", s.rootNote);
        sampleObj->setProperty ("detected_pitch_hz", s.detectedPitchHz);
        sampleObj->setProperty ("start_seconds", s.startSeconds);
        sampleObj->setProperty ("length_seconds", s.lengthSeconds);
        sampleArray.add (juce::var (sampleObj));
    }

    root->setProperty ("samples", juce::var (sampleArray));

    juce::Array<juce::var> zoneArray;

    for (const auto& z : instrumentMapping)
    {
        auto* zoneObj = new juce::DynamicObject();
        zoneObj->setProperty ("sample_index", z.sampleIndex);
        zoneObj->setProperty ("low_note", z.lowNote);
        zoneObj->setProperty ("high_note", z.highNote);
        zoneObj->setProperty ("root_note", z.rootNote);
        zoneArray.add (juce::var (zoneObj));
    }

    root->setProperty ("instrument_mapping", juce::var (zoneArray));

    return juce::var (root);
}

ProjectDocument ProjectDocument::fromVar (const juce::var& value, juce::String& errorOut)
{
    ProjectDocument doc;
    auto* root = value.getDynamicObject();

    if (root == nullptr)
    {
        errorOut = "project file is not a JSON object";
        return doc;
    }

    const auto format = root->getProperty ("format").toString();

    if (format != toJuce (kFormatId))
    {
        errorOut = "not a " + toJuce (kFormatId) + " project (format=\"" + format + "\")";
        return doc;
    }

    const int version = (int) root->getProperty ("format_version");

    if (version > kFormatVersion)
    {
        errorOut = "project format v" + juce::String (version)
                     + " is newer than this build understands (v" + juce::String (kFormatVersion) + ")";
        return doc;
    }

    doc.appVersion = toStd (root->getProperty ("app_version").toString());
    doc.createdAt = toStd (root->getProperty ("created_at").toString());
    doc.embedded = (bool) root->getProperty ("embedded_audio");
    doc.source = audioRefFromVar (root->getProperty ("source_audio"));
    doc.extracted = audioRefFromVar (root->getProperty ("extracted_audio"));

    if (auto* queryObj = root->getProperty ("query").getDynamicObject())
    {
        doc.query.text = toStd (queryObj->getProperty ("text").toString());
        doc.query.quality = toStd (queryObj->getProperty ("quality").toString());
        doc.query.startSeconds = (double) queryObj->getProperty ("start_seconds");
        doc.query.endSeconds = (double) queryObj->getProperty ("end_seconds");
    }

    if (auto* modelObj = root->getProperty ("model").getDynamicObject())
    {
        doc.model.name = toStd (modelObj->getProperty ("name").toString());
        doc.model.version = toStd (modelObj->getProperty ("version").toString());
        doc.model.sha256 = toStd (modelObj->getProperty ("sha256").toString());
    }

    if (auto* diag = root->getProperty ("diagnostics").getDynamicObject())
    {
        doc.diagnostics.refinementPasses = (int) diag->getProperty ("refinement_passes");
        doc.diagnostics.cancelled = (bool) diag->getProperty ("cancelled");
        doc.diagnostics.separationSeconds = (double) diag->getProperty ("separation_seconds");
        doc.diagnostics.candidateConfidence = (float) (double) diag->getProperty ("candidate_confidence");
        doc.diagnostics.tempoBpm = (double) diag->getProperty ("tempo_bpm");
        doc.diagnostics.tempoConfidence = (float) (double) diag->getProperty ("tempo_confidence");
        doc.diagnostics.meanPitchHz = (double) diag->getProperty ("mean_pitch_hz");
        doc.diagnostics.pitchConfidence = (float) (double) diag->getProperty ("pitch_confidence");
        doc.diagnostics.keyDescription = toStd (diag->getProperty ("key_description").toString());
        doc.diagnostics.keyConfidence = (float) (double) diag->getProperty ("key_confidence");
        doc.diagnostics.notes = arrayToStrings (diag->getProperty ("notes"));
    }

    if (const auto* arr = root->getProperty ("samples").getArray())
    {
        for (const auto& item : *arr)
        {
            SampleMeta sample;

            if (auto* obj = item.getDynamicObject())
            {
                sample.name = toStd (obj->getProperty ("name").toString());
                sample.audio = audioRefFromVar (obj->getProperty ("audio"));
                sample.rootNote = (int) obj->getProperty ("root_note");
                sample.detectedPitchHz = (double) obj->getProperty ("detected_pitch_hz");
                sample.startSeconds = (double) obj->getProperty ("start_seconds");
                sample.lengthSeconds = (double) obj->getProperty ("length_seconds");
            }

            doc.samples.push_back (std::move (sample));
        }
    }

    if (const auto* arr = root->getProperty ("instrument_mapping").getArray())
    {
        for (const auto& item : *arr)
        {
            InstrumentZone zone;

            if (auto* obj = item.getDynamicObject())
            {
                zone.sampleIndex = (int) obj->getProperty ("sample_index");
                zone.lowNote = (int) obj->getProperty ("low_note");
                zone.highNote = (int) obj->getProperty ("high_note");
                zone.rootNote = (int) obj->getProperty ("root_note");
            }

            doc.instrumentMapping.push_back (zone);
        }
    }

    return doc;
}

ProjectDocument ProjectDocument::fromExtraction (const extraction::SourceResult& result,
                                                 const extraction::SourceQuery& query,
                                                 const AudioRef& sourceAudio)
{
    ProjectDocument doc;
    doc.createdAt = toStd (juce::Time::getCurrentTime().toISO8601 (true));
    doc.source = sourceAudio;

    doc.query.text = query.description;
    doc.query.quality = result.metadata.modelName.empty() ? std::string ("High Quality") : std::string ("High Quality");
    doc.query.startSeconds = query.startTime.has_value() ? *query.startTime : -1.0;
    doc.query.endSeconds = query.endTime.has_value() ? *query.endTime : -1.0;

    doc.model.name = result.metadata.modelName;
    doc.model.version = result.metadata.modelVersion;

    doc.diagnostics.refinementPasses = result.diagnostics.refinementPasses;
    doc.diagnostics.cancelled = result.diagnostics.cancelled;
    doc.diagnostics.separationSeconds = result.diagnostics.separationSeconds;
    doc.diagnostics.candidateConfidence = result.diagnostics.candidateConfidence;
    doc.diagnostics.tempoBpm = result.diagnostics.tempoBpm;
    doc.diagnostics.tempoConfidence = result.diagnostics.tempoConfidence;
    doc.diagnostics.meanPitchHz = result.diagnostics.meanPitchHz;
    doc.diagnostics.pitchConfidence = result.diagnostics.pitchConfidence;
    doc.diagnostics.keyDescription = result.diagnostics.keyDescription;
    doc.diagnostics.keyConfidence = result.diagnostics.keyConfidence;
    doc.diagnostics.notes = result.diagnostics.notes;

    doc.extracted.sampleRate = result.metadata.sampleRate;
    doc.extracted.numChannels = result.metadata.numChannels;

    return doc;
}

juce::File ProjectDocument::resolve (const std::string& storedPath) const
{
    if (storedPath.empty())
        return {};

    juce::File file (toJuce (storedPath));

    if (isAbsoluteStoredPath (storedPath))
        return file;

    return projectFile.getParentDirectory().getChildFile (toJuce (storedPath));
}

namespace
{
    /** Copy `file` into `assetsDir`, returning the new file name. */
    juce::Result embedOne (const juce::File& file, const juce::File& assetsDir, juce::String& storedName)
    {
        if (! assetsDir.exists())
        {
            if (! assetsDir.createDirectory().wasOk())
                return juce::Result::fail ("could not create assets folder: " + assetsDir.getFullPathName());
        }

        const auto target = assetsDir.getChildFile (file.getFileName());

        if (target.getFullPathName() != file.getFullPathName())
        {
            if (target.existsAsFile())
                target.deleteFile();

            if (! file.copyFileTo (target).wasOk())
                return juce::Result::fail ("could not embed audio: " + file.getFullPathName());
        }

        storedName = toStd (assetsDir.getFileName() + "/" + file.getFileName());
        return juce::Result::ok();
    }
}

juce::Result ProjectDocument::save (const juce::File& destination, bool embedAudio)
{
    auto doc = *this;
    doc.projectFile = destination;

    const auto baseDirectory = destination.getParentDirectory();

    if (embedAudio)
    {
        const auto assetsDir = baseDirectory.getChildFile (destination.getFileNameWithoutExtension() + ".zsource.assets");

        if (doc.source.isValid())
        {
            const auto sourceFile = resolve (doc.source.path);

            if (! sourceFile.existsAsFile())
                return juce::Result::fail ("source audio missing, cannot embed: " + sourceFile.getFullPathName());

            juce::String storedName;

            if (auto r = embedOne (sourceFile, assetsDir, storedName); r.failed())
                return r;

            doc.source.path = toStd (storedName);
        }

        if (doc.extracted.isValid())
        {
            const auto extractedFile = resolve (doc.extracted.path);

            if (! extractedFile.existsAsFile())
                return juce::Result::fail ("extracted audio missing, cannot embed: " + extractedFile.getFullPathName());

            juce::String storedName;

            if (auto r = embedOne (extractedFile, assetsDir, storedName); r.failed())
                return r;

            doc.extracted.path = toStd (storedName);
        }

        for (auto& sample : doc.samples)
        {
            if (! sample.audio.isValid())
                continue;

            const auto sampleFile = resolve (sample.audio.path);

            if (! sampleFile.existsAsFile())
                return juce::Result::fail ("sample audio missing, cannot embed: " + sampleFile.getFullPathName());

            juce::String storedName;

            if (auto r = embedOne (sampleFile, assetsDir, storedName); r.failed())
                return r;

            sample.audio.path = toStd (storedName);
        }

        doc.embedded = true;
    }
    else
    {
        // Keep references portable when they sit under the project folder.
        if (doc.source.isValid())
            doc.source.path = relativise (resolve (doc.source.path), baseDirectory);

        if (doc.extracted.isValid())
            doc.extracted.path = relativise (resolve (doc.extracted.path), baseDirectory);

        for (auto& sample : doc.samples)
            if (sample.audio.isValid())
                sample.audio.path = relativise (resolve (sample.audio.path), baseDirectory);
    }

    if (! destination.getParentDirectory().exists())
        if (! destination.getParentDirectory().createDirectory().wasOk())
            return juce::Result::fail ("could not create project folder: " + destination.getParentDirectory().getFullPathName());

    auto json = juce::JSON::toString (doc.toVar(), false);

    juce::TemporaryFile temp (destination);

    if (! temp.getFile().replaceWithText (json, false, false, "\n"))
        return juce::Result::fail ("could not write project file: " + destination.getFullPathName());

    if (auto r = temp.overwriteTargetFileWithTemporary(); r.failed())
        return r;

    projectFile = destination;
    embedded = doc.embedded;
    source = doc.source;
    extracted = doc.extracted;
    samples = doc.samples;

    return juce::Result::ok();
}

juce::Result ProjectDocument::load (const juce::File& sourceFile, ProjectDocument& out)
{
    if (! sourceFile.existsAsFile())
        return juce::Result::fail ("project file not found: " + sourceFile.getFullPathName());

    juce::var parsed;

    if (auto parseResult = juce::JSON::parse (sourceFile.loadFileAsString(), parsed); parseResult.failed())
        return juce::Result::fail ("project file is not valid JSON: " + parseResult.getErrorMessage());

    juce::String error;
    auto doc = ProjectDocument::fromVar (parsed, error);

    if (error.isNotEmpty())
        return juce::Result::fail (error);

    doc.projectFile = sourceFile;
    out = std::move (doc);
    return juce::Result::ok();
}

juce::Result ProjectDocument::verifyAudio() const
{
    const auto check = [this] (const AudioRef& ref, const juce::String& label) -> juce::Result
    {
        if (! ref.isValid())
            return juce::Result::fail (label + ": project has no audio reference recorded");

        const auto file = resolve (ref.path);

        if (! file.existsAsFile())
            return juce::Result::fail (label + " missing on disk: " + file.getFullPathName());

        if (! ref.sha256.empty())
        {
            const auto actual = hashFile (file);

            if (actual != ref.sha256)
                return juce::Result::fail (label + " changed since the project was saved (sha256 mismatch): "
                                             + file.getFullPathName());
        }

        return juce::Result::ok();
    };

    if (auto r = check (source, "source audio"); r.failed())
        return r;

    if (extracted.isValid())
        if (auto r = check (extracted, "extracted audio"); r.failed())
            return r;

    for (size_t i = 0; i < samples.size(); ++i)
        if (samples[i].audio.isValid())
            if (auto r = check (samples[i].audio, "sample " + juce::String ((int) i)); r.failed())
                return r;

    return juce::Result::ok();
}

} // namespace zarlino::source::project
