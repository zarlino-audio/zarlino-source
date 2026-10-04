#include "TestRunner.h"

#include "inference/ModelManager.h"
#include "inference/ModelRuntime.h"
#include "inference/implementations/OnnxSeparator.h"

// Opt-in integration test: runs REAL inference through ModelRuntime +
// OnnxSeparator on the `models/htdemucs` package. It is skipped by default;
// set ZARLINO_SMOKE_MODEL=1 to run it (CPU inference takes minutes).

ZS_TEST (OnnxSeparatorRealModelSmoke_OPT_IN)
{
    if (juce::SystemStats::getEnvironmentVariable ("ZARLINO_SMOKE_MODEL", {})
            != juce::String ("1"))
    {
        return;   // opt-in only
    }

    namespace inf = zarlino::source::inference;

    const auto modelsRoot = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                                .getParentDirectory()    // Release
                                .getParentDirectory()    // ZarlinoSource_Tests_artefacts
                                .getParentDirectory()    // build
                                .getParentDirectory()    // zarlino-source
                                .getChildFile ("models");

    inf::ModelManager manager (modelsRoot);
    manager.refresh();

    const auto* package = manager.findById ("htdemucs");
    ZS_EXPECT (package != nullptr);

    if (package == nullptr)
        return;

    juce::String verifyError;
    ZS_EXPECT (inf::ModelManager::verify (*package, verifyError));

    inf::ModelRuntime runtime;
    ZS_EXPECT (runtime.isAvailable());

    inf::OnnxSeparator separator (runtime, *package);
    ZS_EXPECT (separator.isAvailable());

    // 1.5 s test mixture at 48 kHz (the separator resamples to the model rate).
    juce::AudioBuffer<float> mixture (2, (int) (48000 * 1.5));
    mixture.clear();

    for (int ch = 0; ch < 2; ++ch)
    {
        auto* data = mixture.getWritePointer (ch);

        for (int i = 0; i < mixture.getNumSamples(); ++i)
        {
            const double t = (double) i / 48000.0;
            data[i] = 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * t)
                    + 0.2f * (float) std::sin (juce::MathConstants<double>::twoPi * 880.0 * t);
        }

        // A click at 0.5 s so there is transient content.
        const int click = (int) (0.5 * 48000);

        for (int i = 0; i < 96 && click + i < mixture.getNumSamples(); ++i)
            mixture.getWritePointer (ch)[click + i] += 0.6f
                * (float) std::exp (-(double) i / 16.0) * (((i % 2) == 0) ? 1.0f : -1.0f);
    }

    inf::SeparationRequest request;
    request.sampleRate = 48000.0;
    request.quality = inf::SeparationRequest::Quality::highQuality;

    const auto result = separator.separate (mixture, request, {}, {});

    if (! result.success)
        std::cout << "  [smoke] separator error: " << result.error << std::endl;

    ZS_EXPECT (result.success);
    ZS_EXPECT (result.audio.getNumSamples() > 0);
    ZS_EXPECT (result.otherStems.size() == 3);
    ZS_EXPECT (result.modelName == "htdemucs");
}
