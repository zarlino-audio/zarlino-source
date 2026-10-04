#include "TestRunner.h"

#include "core/analysis/ArtifactAnalyzer.h"
#include "core/analysis/CandidateAnalyzer.h"
#include "core/analysis/FrameFeatures.h"
#include "core/extraction/QueryInterpreter.h"

#include <random>

namespace
{
    constexpr double kSr = 48000.0;

    juce::AudioBuffer<float> makeSine (double frequency, double seconds, double sampleRate, float amplitude = 0.5f)
    {
        const int numSamples = (int) std::llround (seconds * sampleRate);
        juce::AudioBuffer<float> buffer (1, numSamples);

        for (int i = 0; i < numSamples; ++i)
            buffer.getWritePointer (0)[i] = amplitude
                * (float) std::sin (juce::MathConstants<double>::twoPi * frequency * (double) i / sampleRate);

        return buffer;
    }
}

ZS_TEST (FrameFeaturesDistinguishToneAndNoise)
{
    auto tone = makeSine (440.0, 0.5, kSr, 0.6f);
    const auto toneFeatures = zarlino::source::analysis::computeFeatures (tone, kSr);

    ZS_EXPECT ((int) toneFeatures.frames.size() > 10);

    if (toneFeatures.frames.empty())
        return;

    const auto& tf = toneFeatures.frames[toneFeatures.frames.size() / 2];

    // A 440 Hz tone: centred in the mid band, strongly tonal (low flatness).
    ZS_EXPECT (tf.centroidHz > 350.0f && tf.centroidHz < 550.0f);
    ZS_EXPECT (tf.midEnergy > tf.lowEnergy);
    ZS_EXPECT (tf.midEnergy > tf.highEnergy);
    ZS_EXPECT (tf.flatness < 0.35f);

    // White noise: broadband and flat.
    juce::AudioBuffer<float> noise (1, (int) (kSr * 0.5));
    std::mt19937 rng (7);
    std::uniform_real_distribution<float> dist (-0.4f, 0.4f);

    for (int i = 0; i < noise.getNumSamples(); ++i)
        noise.getWritePointer (0)[i] = dist (rng);

    const auto noiseFeatures = zarlino::source::analysis::computeFeatures (noise, kSr);
    ZS_EXPECT (! noiseFeatures.frames.empty());

    if (noiseFeatures.frames.empty())
        return;

    const auto& nf = noiseFeatures.frames[noiseFeatures.frames.size() / 2];

    ZS_EXPECT (nf.flatness > 0.4f);
    ZS_EXPECT (nf.flatness > tf.flatness + 0.2f);
}

ZS_TEST (CandidateAnalyzerLocalisesTonalBursts)
{
    namespace an = zarlino::source::analysis;
    namespace ex = zarlino::source::extraction;

    // 2 s: a 440 Hz tone present 0.5-1.0 s and 1.4-1.9 s, silence elsewhere.
    juce::AudioBuffer<float> mixture (1, (int) (kSr * 2.0));
    mixture.clear();

    auto addTone = [&mixture] (double startSeconds, double endSeconds)
    {
        const int start = (int) (startSeconds * kSr);
        const int end = (int) (endSeconds * kSr);

        for (int i = start; i < end; ++i)
            mixture.getWritePointer (0)[i] += 0.5f
                * (float) std::sin (juce::MathConstants<double>::twoPi * 440.0 * (double) i / kSr);
    };

    addTone (0.5, 1.0);
    addTone (1.4, 1.9);

    const auto features = an::computeFeatures (mixture, kSr);

    ex::QueryInterpreter interpreter;
    const auto interpretation = interpreter.interpret ("Rhodes");
    ZS_EXPECT (interpretation.recognised);

    const auto candidates = an::analyse (features, interpretation.profile);

    ZS_EXPECT ((int) candidates.frameAffinity.size() == features.numFrames);
    ZS_EXPECT (! candidates.regions.empty());
    ZS_EXPECT (candidates.overallConfidence > 0.2f);

    // Affinity inside a burst must clearly exceed affinity in silence.
    const int burstFrame = (int) (0.9 * kSr / features.hopSize);
    const int silenceFrame = (int) (0.2 * kSr / features.hopSize);

    ZS_EXPECT (candidates.frameAffinity[(size_t) burstFrame] > 0.5f);
    ZS_EXPECT (candidates.frameAffinity[(size_t) burstFrame]
                   > candidates.frameAffinity[(size_t) silenceFrame] + 0.3f);

    // The strongest region must start inside one of the two bursts (equal
    // bursts may rank either first; what matters is it is not silence).
    if (! candidates.regions.empty())
    {
        const auto& best = candidates.regions.front();
        ZS_EXPECT (best.startTime > 0.3);
        ZS_EXPECT (best.startTime < 1.9);
    }

    ZS_EXPECT ((int) candidates.regions.size() >= 2);   // both bursts get a region
}

ZS_TEST (ArtifactAnalyzerScoresExtraction)
{
    namespace an = zarlino::source::analysis;
    namespace ex = zarlino::source::extraction;

    // Mixture: sustained tone plus a click train (so there are transients).
    juce::AudioBuffer<float> mixture (1, (int) (kSr * 2.0));
    mixture.clear();

    for (int i = 0; i < mixture.getNumSamples(); ++i)
        mixture.getWritePointer (0)[i] = 0.3f
            * (float) std::sin (juce::MathConstants<double>::twoPi * 440.0 * (double) i / kSr);

    for (int k = 1; k <= 3; ++k)
    {
        const int start = (int) (0.5 * kSr * k);

        for (int i = 0; i < 96 && start + i < mixture.getNumSamples(); ++i)
            mixture.getWritePointer (0)[start + i] += 0.6f
                * (float) std::exp (-(double) i / 16.0) * (((i % 2) == 0) ? 1.0f : -1.0f);
    }

    ex::QueryInterpreter interpreter;
    const auto interpretation = interpreter.interpret ("Rhodes");
    const auto features = an::computeFeatures (mixture, kSr);
    const auto candidates = an::analyse (features, interpretation.profile);

    // Perfect "separation": target == mixture.
    const auto perfect = an::analyseArtifacts (mixture, mixture, kSr, interpretation.profile, candidates);

    ZS_EXPECT (perfect.quality.targetPreservation > 0.85f);
    ZS_EXPECT (perfect.quality.transientIntegrity > 0.9f);
    ZS_EXPECT (perfect.quality.artifactScore < 0.5f);
    ZS_EXPECT (perfect.transientIntegrityComputed);
    ZS_EXPECT (! perfect.phaseIntegrityComputed);   // not implemented - reported, not faked

    // Broken "separation": quiet noise at -40 dB.
    juce::AudioBuffer<float> broken (1, mixture.getNumSamples());
    std::mt19937 rng (11);
    std::uniform_real_distribution<float> dist (-0.01f, 0.01f);

    for (int i = 0; i < broken.getNumSamples(); ++i)
        broken.getWritePointer (0)[i] = dist (rng);

    const auto bad = an::analyseArtifacts (broken, mixture, kSr, interpretation.profile, candidates);

    ZS_EXPECT (bad.quality.targetPreservation < 0.3f);
    ZS_EXPECT (bad.quality.transientIntegrity < 0.5f);
    ZS_EXPECT (bad.quality.targetPreservation < perfect.quality.targetPreservation);
}

ZS_TEST (BurstyBinFractionSeparatesStationaryFromBlobs)
{
    namespace an = zarlino::source::analysis;

    // A steady tone is one long run per bin: no short-lived blobs.
    auto tone = makeSine (440.0, 1.0, kSr, 0.6f);
    const float toneFraction = an::burstyBinFraction (tone.getReadPointer (0),
                                                      tone.getNumSamples(), kSr);

    ZS_EXPECT (toneFraction < 0.15f);

    // Sparse impulses smear into short broadband blobs (~4 frames each at
    // 2048/512): nearly every active bin lives in a short run.
    juce::AudioBuffer<float> spiky (1, (int) (kSr * 1.0));
    spiky.clear();

    for (int i = 0; i < spiky.getNumSamples(); i += 4096)
        spiky.getWritePointer (0)[i] = 0.9f;

    const float spikeFraction = an::burstyBinFraction (spiky.getReadPointer (0),
                                                       spiky.getNumSamples(), kSr);

    std::cout << "  [bursty] tone=" << toneFraction << " spikes=" << spikeFraction << std::endl;

    ZS_EXPECT (spikeFraction > 0.5f);
    ZS_EXPECT (spikeFraction > toneFraction + 0.3f);
}
