#include "TestRunner.h"

#include "core/extraction/Progress.h"
#include "inference/ISeparator.h"
#include "inference/ModelManager.h"
#include "inference/ModelRuntime.h"
#include "inference/implementations/OnnxSeparator.h"

#include <juce_cryptography/juce_cryptography.h>

ZS_TEST (ModelRuntimeWithoutDllIsHonest)
{
    namespace inf = zarlino::source::inference;

    inf::ModelRuntime runtime;

    std::cout << "  [runtime] available=" << (runtime.isAvailable() ? "yes" : "no")
              << " version=" << runtime.getVersion() << std::endl;

    ZS_EXPECT (inf::ModelRuntime::getSearchedPaths().size() >= 2);

    juce::String error;
    auto session = runtime.createSession (juce::File ("C:\\nonexistent\\zarlino\\model.onnx"), 2, error);

    ZS_EXPECT (session == nullptr);
    ZS_EXPECT (error.isNotEmpty());

    if (runtime.isAvailable())
        ZS_EXPECT (runtime.getVersion().isNotEmpty());
    else
        ZS_EXPECT (runtime.getVersion().isEmpty());
}

ZS_TEST (OnnxSeparatorFailsHonestlyWithoutRuntimeOrModel)
{
    namespace inf = zarlino::source::inference;

    // A syntactically valid package whose checkpoint is not a real model.
    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("zarlino_runtime_tests_models");
    root.deleteRecursively();
    root.createDirectory();

    const auto dir = root.getChildFile ("fake-model");
    dir.createDirectory();

    const auto checkpoint = dir.getChildFile ("model.onnx");
    checkpoint.replaceWithText ("definitely not an onnx model");

    const auto hash = juce::SHA256 (checkpoint).toHexString().toLowerCase();

    dir.getChildFile ("manifest.json").replaceWithText (
        "{\"name\":\"fake\",\"version\":\"0\",\"model_format\":\"onnx\","
        "\"sample_rates\":[48000],\"channels\":2,\"sha256\":\"" + hash + "\","
        "\"license\":\"test\",\"license_url\":\"https://example.com/license\"}");

    inf::ModelManager manager (root);
    manager.refresh();

    const auto* package = manager.findById ("fake-model");
    ZS_EXPECT (package != nullptr);

    if (package == nullptr)
        return;

    inf::ModelRuntime runtime;
    inf::OnnxSeparator separator (runtime, *package);

    // Without the runtime: unavailable. With the runtime: the fake checkpoint
    // must fail session creation. Either way - no audio, clear error.
    ZS_EXPECT (! separator.isAvailable());

    juce::AudioBuffer<float> mixture (2, 48000);
    mixture.clear();

    inf::SeparationRequest request;
    request.sampleRate = 48000.0;

    const auto result = separator.separate (mixture, request, {}, {});

    ZS_EXPECT (! result.success);
    ZS_EXPECT (result.error.isNotEmpty());
    ZS_EXPECT (result.audio.getNumSamples() == 0);

    root.deleteRecursively();
}
