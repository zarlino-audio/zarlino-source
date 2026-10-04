#include "inference/ModelManager.h"

#include <juce_cryptography/juce_cryptography.h>

namespace zarlino::source::inference
{

namespace
{

juce::var parseManifest (const juce::File& file)
{
    if (! file.existsAsFile())
        return {};

    // Malformed JSON must never take the scan down; a bad package is simply
    // marked invalid and stays unusable.
    try
    {
        return juce::JSON::parse (file);
    }
    catch (...)
    {
        return {};
    }
}

std::vector<double> readSampleRates (const juce::var& v)
{
    std::vector<double> rates;

    if (auto* arr = v.getArray())
        for (const auto& item : *arr)
        {
            const double r = (double) item;

            if (r > 0.0)
                rates.push_back (r);
        }

    return rates;
}

std::vector<juce::String> readStrings (const juce::var& v)
{
    std::vector<juce::String> out;

    if (auto* arr = v.getArray())
        for (const auto& item : *arr)
        {
            const auto s = item.toString().trim();

            if (s.isNotEmpty())
                out.push_back (s);
        }

    return out;
}

} // namespace

ModelManager::ModelManager (juce::File modelsRoot)
    : root_ (std::move (modelsRoot))
{
}

void ModelManager::refresh()
{
    packages_.clear();

    if (! root_.isDirectory())
        return;

    for (const auto& entry : juce::RangedDirectoryIterator (root_, false, "*",
                                                            juce::File::findDirectories))
    {
        ModelPackage package;
        package.id = entry.getFile().getFileName();
        package.directory = entry.getFile();

        const auto manifest = parseManifest (entry.getFile().getChildFile ("manifest.json"));

        if (! manifest.isObject())
        {
            package.parseError = "manifest.json missing or not a JSON object";
            packages_.push_back (std::move (package));
            continue;
        }

        package.name = manifest.getProperty ("name", {}).toString().trim();
        package.version = manifest.getProperty ("version", {}).toString().trim();
        package.format = manifest.getProperty ("model_format", {}).toString().trim();
        package.sampleRates = readSampleRates (manifest.getProperty ("sample_rates", {}));
        package.channels = (int) manifest.getProperty ("channels", 0);
        package.sha256 = manifest.getProperty ("sha256", {}).toString().trim().toLowerCase();
        package.license = manifest.getProperty ("license", {}).toString().trim();
        package.licenseUrl = manifest.getProperty ("license_url", {}).toString().trim();

        package.inputName = manifest.getProperty ("input_name", {}).toString().trim();
        package.outputName = manifest.getProperty ("output_name", {}).toString().trim();
        package.chunkSeconds = (double) manifest.getProperty ("chunk_seconds", 0.0);
        package.stemNames = readStrings (manifest.getProperty ("stem_names", {}));

        const auto checkpointName = manifest.getProperty ("checkpoint", "model.onnx").toString().trim();
        package.checkpointFile = entry.getFile().getChildFile (checkpointName.isNotEmpty()
                                                                   ? checkpointName
                                                                   : juce::String ("model.onnx"));

        // Completeness checks. Licence fields are mandatory: a model with an
        // unattributed licence must not become usable (spec sections 24/53).
        juce::StringArray problems;

        if (package.name.isEmpty())          problems.add ("name");
        if (package.version.isEmpty())       problems.add ("version");
        if (package.format.isEmpty())        problems.add ("model_format");
        if (package.sampleRates.empty())     problems.add ("sample_rates");
        if (package.channels <= 0)           problems.add ("channels");
        if (package.sha256.length() != 64)   problems.add ("sha256 (64 hex digits)");
        if (package.license.isEmpty())       problems.add ("license");
        if (package.licenseUrl.isEmpty())    problems.add ("license_url");

        if (problems.isEmpty())
        {
            package.parseValid = true;
        }
        else
        {
            package.parseError = "manifest missing/invalid fields: " + problems.joinIntoString (", ");
        }

        packages_.push_back (std::move (package));
    }
}

const ModelPackage* ModelManager::findById (const juce::String& id) const
{
    for (const auto& p : packages_)
        if (p.id == id)
            return &p;

    return nullptr;
}

bool ModelManager::verify (const ModelPackage& package, juce::String& errorOut)
{
    errorOut.clear();

    if (! package.parseValid)
    {
        errorOut = package.parseError.isNotEmpty() ? package.parseError
                                                   : juce::String ("model package manifest is not valid");
        return false;
    }

    if (! package.checkpointFile.existsAsFile())
    {
        errorOut = "checkpoint file missing: " + package.checkpointFile.getFullPathName();
        return false;
    }

    const auto actual = juce::SHA256 (package.checkpointFile).toHexString().toLowerCase();

    if (actual != package.sha256)
    {
        errorOut = "checkpoint SHA-256 mismatch (manifest " + package.sha256 + ", file " + actual + ")";
        return false;
    }

    return true;
}

} // namespace zarlino::source::inference
