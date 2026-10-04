#include "TestRunner.h"

#include "core/analysis/ArtifactAnalyzer.h"
#include "core/analysis/CandidateAnalyzer.h"
#include "core/analysis/FrameFeatures.h"
#include "core/analysis/SourceRefiner.h"

#include <cmath>
#include <random>
#include <vector>

namespace
{
    constexpr double kSr = 48000.0;

    void fillSine (juce::AudioBuffer<float>& buffer, double frequencyHz, float amplitude)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer (ch);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
                data[i] = amplitude * (float) std::sin (juce::MathConstants<double>::twoPi
                                                        * frequencyHz * (double) i / kSr);
        }
    }

    juce::AudioBuffer<float> makeTone (double seconds, double frequencyHz, float amplitude)
    {
        juce::AudioBuffer<float> buffer (2, (int) std::llround (seconds * kSr));
        fillSine (buffer, frequencyHz, amplitude);
        return buffer;
    }

    /** Adds deterministic short broadband dashes ("specks") to both channels and
        returns their centre times in seconds. */
    std::vector<double> addSpecks (juce::AudioBuffer<float>& buffer, float amplitude,
                                   double intervalSeconds)
    {
        std::mt19937 rng (0x5eedu);
        std::uniform_real_distribution<float> dist (-1.0f, 1.0f);

        const int speckLength = (int) std::llround (0.011 * kSr);
        const double seconds = (double) buffer.getNumSamples() / kSr;

        std::vector<double> centres;

        for (double t = intervalSeconds; t < seconds - 0.05; t += intervalSeconds)
        {
            const int start = (int) std::llround (t * kSr);

            for (int i = 0; i < speckLength && start + i < buffer.getNumSamples(); ++i)
            {
                const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                                        * (float) i / (float) (speckLength - 1));
                const float value = dist (rng) * amplitude * w;

                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    buffer.addSample (ch, start + i, value);
            }

            centres.push_back ((double) (start + speckLength / 2) / kSr);
        }

        return centres;
    }

    /** RMS of (a - b) over both channels in a sample window. */
    float differenceRms (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b,
                         int start, int length)
    {
        double sum = 0.0;
        long long count = 0;

        const int channels = juce::jmin (a.getNumChannels(), b.getNumChannels());

        for (int ch = 0; ch < channels; ++ch)
        {
            const auto* pa = a.getReadPointer (ch);
            const auto* pb = b.getReadPointer (ch);
            const int end = juce::jmin (start + length, a.getNumSamples());

            for (int i = juce::jmax (0, start); i < end; ++i)
            {
                const double d = (double) pa[i] - (double) pb[i];
                sum += d * d;
                ++count;
            }
        }

        return count > 0 ? (float) std::sqrt (sum / (double) count) : 0.0f;
    }

    float signalRms (const juce::AudioBuffer<float>& a, int start, int length)
    {
        double sum = 0.0;
        long long count = 0;

        for (int ch = 0; ch < a.getNumChannels(); ++ch)
        {
            const auto* p = a.getReadPointer (ch);
            const int end = juce::jmin (start + length, a.getNumSamples());

            for (int i = juce::jmax (0, start); i < end; ++i)
            {
                sum += (double) p[i] * (double) p[i];
                ++count;
            }
        }

        return count > 0 ? (float) std::sqrt (sum / (double) count) : 0.0f;
    }

    bool buffersIdentical (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        if (a.getNumChannels() != b.getNumChannels() || a.getNumSamples() != b.getNumSamples())
            return false;

        for (int ch = 0; ch < a.getNumChannels(); ++ch)
            for (int i = 0; i < a.getNumSamples(); ++i)
                if (a.getReadPointer (ch)[i] != b.getReadPointer (ch)[i])
                    return false;

        return true;
    }
}

ZS_TEST (SpectralGateSuppressesSpecksAndKeepsTone)
{
    namespace an = zarlino::source::analysis;

    auto buffer = makeTone (2.0, 440.0, 0.5f);
    const auto reference = makeTone (2.0, 440.0, 0.5f);
    const auto centres = addSpecks (buffer, 0.35f, 0.25);

    const auto before = juce::AudioBuffer<float> (buffer);

    ZS_EXPECT ((int) centres.size() >= 4);

    const zarlino::source::extraction::TargetProfile profile; // neutral: full gate everywhere

    const auto stats = an::applySpectralGatePass (buffer, kSr, profile);

    ZS_EXPECT (stats.completed);
    ZS_EXPECT (stats.attenuatedBinFraction > 0.0f);
    ZS_EXPECT (stats.meanGain <= 1.0f);

    // 1) The residual (specks) must be strongly reduced in the speck windows.
    const int half = (int) std::llround (0.025 * kSr);

    double residualIn = 0.0;
    double residualOut = 0.0;

    for (double centre : centres)
    {
        const int start = (int) std::llround (centre * kSr) - half;
        residualIn += (double) differenceRms (before, reference, start, 2 * half);
        residualOut += (double) differenceRms (buffer, reference, start, 2 * half);
    }

    residualIn /= (double) centres.size();
    residualOut /= (double) centres.size();

    ZS_EXPECT (residualIn > 0.01);                // the test signal really has specks
    ZS_EXPECT (residualOut < residualIn * 0.5);   // >= 6 dB attenuation

    // A dash-only signal (nothing else in the buffer) must gate down close to
    // the 0.15 floor: the gate attacks the burst itself, not some by-product.
    {
        juce::AudioBuffer<float> solo (2, (int) (1.0 * kSr));
        solo.clear();

        std::mt19937 r2 (0x5eedu);
        std::uniform_real_distribution<float> d2 (-1.0f, 1.0f);

        const int len = (int) std::llround (0.011 * kSr);
        const int st = (int) std::llround (0.4 * kSr);

        for (int i = 0; i < len; ++i)
        {
            const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                                    * (float) i / (float) (len - 1));
            const float v = d2 (r2) * 0.35f * w;

            for (int ch = 0; ch < 2; ++ch)
                solo.addSample (ch, st + i, v);
        }

        const auto soloBefore = juce::AudioBuffer<float> (solo);
        const zarlino::source::extraction::TargetProfile p2;
        const auto s2 = an::applySpectralGatePass (solo, kSr, p2);

        const float rmsBefore = signalRms (soloBefore, 0, soloBefore.getNumSamples());
        const float rmsAfter = signalRms (solo, 0, solo.getNumSamples());
        const float soloRatio = rmsAfter / juce::jmax (1.0e-9f, rmsBefore);

        ZS_EXPECT (s2.completed);
        ZS_EXPECT (soloRatio < 0.3f);

        const float sampleBefore = soloBefore.getReadPointer (0)[st + len / 2];
        const float sampleAfter = solo.getReadPointer (0)[st + len / 2];

        ZS_EXPECT (std::abs (sampleAfter) < std::abs (sampleBefore) * 0.3f);
    }

    // 2) The tone between the specks must survive (amplitude within ~1 dB).
    double toneIn = 0.0;
    double toneOut = 0.0;
    int toneWindows = 0;

    for (size_t w = 0; w + 1 < centres.size(); ++w)
    {
        const int start = (int) std::llround ((centres[w] + 0.07) * kSr);
        const int end = (int) std::llround ((centres[w + 1] - 0.07) * kSr);
        const int length = juce::jmin (end - start, buffer.getNumSamples() - start);

        if (length > 2000)
        {
            toneIn += (double) signalRms (before, start, length);
            toneOut += (double) signalRms (buffer, start, length);
            ++toneWindows;
        }
    }

    ZS_EXPECT (toneWindows > 0);

    if (toneWindows > 0)
    {
        const double ratio = toneOut / toneIn;
        ZS_EXPECT (ratio > 0.9 && ratio < 1.06);
    }
}

ZS_TEST (RefineLoopRejectsNonImprovingPass)
{
    namespace an = zarlino::source::analysis;
    namespace ex = zarlino::source::extraction;

    auto audio = makeTone (1.5, 440.0, 0.5f);
    const auto mixture = juce::AudioBuffer<float> (audio);
    const auto original = juce::AudioBuffer<float> (audio);

    const ex::TargetProfile profile; // neutral

    const auto features = an::computeFeatures (mixture, kSr);
    const auto candidates = an::analyse (features, profile);

    auto quality = an::analyseArtifacts (audio, mixture, kSr, profile, candidates).quality;

    const auto outcome = an::refine (audio, mixture, kSr, profile, candidates, quality, {}, 3, {});

    ZS_EXPECT (outcome.passesRun <= 3);

    // A steady tone contains no short-lived specks: the gate is approximately an
    // identity and the metrics cannot improve, so every pass must be rejected.
    ZS_EXPECT (outcome.passesAccepted == 0);
    ZS_EXPECT (! outcome.improved);
    ZS_EXPECT (! outcome.notes.empty());

    // A rejected pass must leave the audio bit-identical.
    ZS_EXPECT (buffersIdentical (audio, original));
}

ZS_TEST (RefineStopsOnCancellation)
{
    namespace an = zarlino::source::analysis;
    namespace ex = zarlino::source::extraction;

    auto audio = makeTone (1.0, 440.0, 0.5f);
    const auto mixture = juce::AudioBuffer<float> (audio);
    const auto original = juce::AudioBuffer<float> (audio);

    const ex::TargetProfile profile;

    const auto features = an::computeFeatures (mixture, kSr);
    const auto candidates = an::analyse (features, profile);

    auto quality = an::analyseArtifacts (audio, mixture, kSr, profile, candidates).quality;

    int progressCalls = 0;

    const auto outcome = an::refine (audio, mixture, kSr, profile, candidates, quality, {}, 3,
                                     [&progressCalls] (float, const juce::String&)
                                     {
                                         ++progressCalls;
                                         return false; // cancel immediately
                                     });

    ZS_EXPECT (progressCalls > 0);
    ZS_EXPECT (outcome.passesRun == 0);
    ZS_EXPECT (outcome.passesAccepted == 0);
    ZS_EXPECT (! outcome.notes.empty());
    ZS_EXPECT (buffersIdentical (audio, original));
}
