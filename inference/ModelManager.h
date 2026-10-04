#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace zarlino::source::inference
{

/** A model package on disk (spec section 24):

        models/<id>/
            manifest.json      - metadata + integrity (see below)
            <checkpoint>       - e.g. model.onnx

    Required manifest fields:
        name, version, model_format, sample_rates, channels, sha256,
        license, license_url

    Optional manifest fields:
        checkpoint (default "model.onnx"), input_name, output_name,
        chunk_seconds, stem_names

    `sha256` is the SHA-256 of the checkpoint file, verified before the model
    may run. Licence fields must be genuinely filled in - a package without a
    licence is not usable (spec sections 24/53). */
struct ModelPackage
{
    juce::String id;                    // directory name
    juce::File directory;
    juce::File checkpointFile;

    juce::String name;
    juce::String version;
    juce::String format;                // "onnx" today
    std::vector<double> sampleRates;
    int channels = 0;
    juce::String sha256;
    juce::String license;
    juce::String licenseUrl;

    juce::String inputName, outputName;    // optional IO hints
    double chunkSeconds = 0.0;             // optional preferred chunk length
    std::vector<juce::String> stemNames;   // optional (multi-stem models)

    bool parseValid = false;
    juce::String parseError;
};

/** Scans and validates model packages under a models root directory.

    Gates before a model may run (never on the audio thread):
      1. manifest completeness - licence fields must be genuinely filled in
      2. checkpoint SHA-256 must match the manifest

    The manager does no inference itself; separator implementations consume
    validated packages from it. */
class ModelManager
{
public:
    explicit ModelManager (juce::File modelsRoot);

    /** (Re)scan the models root. Safe to call from message or worker thread. */
    void refresh();

    const std::vector<ModelPackage>& getPackages() const noexcept { return packages_; }

    const ModelPackage* findById (const juce::String& id) const;

    /** Full validation including checkpoint hash (potentially slow: hashes
        the whole checkpoint). Worker thread only. */
    static bool verify (const ModelPackage& package, juce::String& errorOut);

    const juce::File& getRoot() const noexcept { return root_; }

private:
    juce::File root_;
    std::vector<ModelPackage> packages_;
};

} // namespace zarlino::source::inference
