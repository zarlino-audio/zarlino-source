#include "TestRunner.h"

#include "inference/ModelManager.h"

#include <juce_cryptography/juce_cryptography.h>

namespace
{

juce::String makeManifest (const juce::String& hash, bool withLicence)
{
    juce::String s;

    s << "{\"name\":\"test\",\"version\":\"1.0.0\",\"model_format\":\"onnx\","
         "\"sample_rates\":[44100,48000],\"channels\":2,"
         "\"sha256\":\"" << hash << "\"";

    if (withLicence)
        s << ",\"license\":\"MIT\",\"license_url\":\"https://example.com/license\"";

    s << "}";
    return s;
}

} // namespace

ZS_TEST (ModelManagerValidatesPackagesAndHashes)
{
    namespace inf = zarlino::source::inference;

    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("zarlino_source_model_tests");
    root.deleteRecursively();
    root.createDirectory();

    // 1. A valid package: manifest hash matches the checkpoint file.
    {
        const auto dir = root.getChildFile ("good-model");
        dir.createDirectory();

        const auto checkpoint = dir.getChildFile ("model.onnx");
        checkpoint.replaceWithText ("not really onnx - test payload only");

        const auto hash = juce::SHA256 (checkpoint).toHexString().toLowerCase();
        dir.getChildFile ("manifest.json").replaceWithText (makeManifest (hash, true));
    }

    // 2. Complete manifest but no licence fields - must not be usable.
    {
        const auto dir = root.getChildFile ("nolicence-model");
        dir.createDirectory();
        dir.getChildFile ("model.onnx").replaceWithText ("payload");
        dir.getChildFile ("manifest.json").replaceWithText (
            makeManifest (juce::String::repeatedString ("0", 64), false));
    }

    // 3. Complete manifest with a wrong hash - verify() must refuse it.
    {
        const auto dir = root.getChildFile ("badhash-model");
        dir.createDirectory();
        dir.getChildFile ("model.onnx").replaceWithText ("payload");
        dir.getChildFile ("manifest.json").replaceWithText (
            makeManifest (juce::String::repeatedString ("0", 64), true));
    }

    inf::ModelManager manager (root);
    manager.refresh();

    ZS_EXPECT (manager.getPackages().size() == 3);

    const auto* good = manager.findById ("good-model");
    ZS_EXPECT (good != nullptr);

    if (good != nullptr)
    {
        ZS_EXPECT (good->parseValid);
        ZS_EXPECT (good->license == "MIT");

        juce::String error;
        ZS_EXPECT (inf::ModelManager::verify (*good, error));
        ZS_EXPECT (error.isEmpty());
    }

    const auto* noLicence = manager.findById ("nolicence-model");
    ZS_EXPECT (noLicence != nullptr);

    if (noLicence != nullptr)
    {
        ZS_EXPECT (! noLicence->parseValid);
        ZS_EXPECT (noLicence->parseError.containsIgnoreCase ("license"));
    }

    const auto* badHash = manager.findById ("badhash-model");
    ZS_EXPECT (badHash != nullptr);

    if (badHash != nullptr)
    {
        ZS_EXPECT (badHash->parseValid);    // manifest itself is complete ...

        juce::String error;
        ZS_EXPECT (! inf::ModelManager::verify (*badHash, error));   // ... hash is not
        ZS_EXPECT (error.containsIgnoreCase ("SHA-256"));
    }

    root.deleteRecursively();
}
