#include "TestRunner.h"

#include "core/analysis/KeyAnalyzer.h"
#include "core/analysis/PitchAnalyzer.h"
#include "core/analysis/TempoAnalyzer.h"
#include "core/extraction/SourceResult.h"
#include "core/sampling/OneShotCreator.h"

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

ZS_TEST (OneShotCreatorFindsTransientAndTail)
{
    namespace sampling = zarlino::source::sampling;
    namespace extraction = zarlino::source::extraction;

    // 2 s of silence with a decaying 440 Hz hit starting at 0.2 s.
    juce::AudioBuffer<float> buffer (2, (int) (kSr * 2.0));
    buffer.clear();

    const int start = (int) (0.2 * kSr);

    for (int ch = 0; ch < 2; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);

        for (int i = 0; i < (int) (0.4 * kSr) && start + i < buffer.getNumSamples(); ++i)
        {
            const double t = (double) i / kSr;
            data[start + i] = 0.8f * (float) std::exp (-t / 0.06)
                * (float) std::sin (juce::MathConstants<double>::twoPi * 440.0 * t);
        }
    }

    extraction::SourceResult result;
    result.audio.makeCopyOf (buffer);
    result.metadata.sampleRate = kSr;
    result.requestedDescription = "hit";
    result.successful = true;

    const auto shot = sampling::OneShotCreator::create (result);

    ZS_EXPECT (shot.success);
    ZS_EXPECT (shot.startTime > 0.15 && shot.startTime < 0.30);
    ZS_EXPECT (shot.endTime > 0.30 && shot.endTime < 0.95);
    ZS_EXPECT (shot.audio.getNumSamples() > 0);
    ZS_EXPECT (shot.audio.getNumChannels() == 2);

    if (shot.audio.getNumSamples() > 4)
    {
        // Short fades: both edges start/end at exactly zero.
        ZS_EXPECT_NEAR (shot.audio.getReadPointer (0)[0], 0.0, 1.0e-4);
        ZS_EXPECT_NEAR (shot.audio.getReadPointer (0)[shot.audio.getNumSamples() - 1], 0.0, 1.0e-4);
    }
}

ZS_TEST (PitchAnalyzerTracksSineAndReportsUnvoicedSilence)
{
    namespace an = zarlino::source::analysis;

    auto sine = makeSine (220.0, 1.0, kSr, 0.6f);
    const auto track = an::PitchAnalyzer::analyse (sine, kSr);

    ZS_EXPECT (track.anyVoiced);

    double sum = 0.0;
    int voiced = 0;

    for (const auto& p : track.points)
        if (p.frequencyHz > 0.0)
        {
            sum += p.frequencyHz;
            ++voiced;
        }

    ZS_EXPECT (voiced > 20);

    const double meanHz = sum / (double) juce::jmax (1, voiced);
    ZS_EXPECT_NEAR (meanHz, 220.0, 2.0);
    ZS_EXPECT (track.overallConfidence > 0.5f);

    juce::AudioBuffer<float> silence (1, (int) kSr);
    silence.clear();
    const auto silentTrack = an::PitchAnalyzer::analyse (silence, kSr);
    ZS_EXPECT (! silentTrack.anyVoiced);

    juce::AudioBuffer<float> noise (1, (int) (kSr * 0.5));
    std::mt19937 rng (21);
    std::uniform_real_distribution<float> dist (-0.4f, 0.4f);

    for (int i = 0; i < noise.getNumSamples(); ++i)
        noise.getWritePointer (0)[i] = dist (rng);

    const auto noiseTrack = an::PitchAnalyzer::analyse (noise, kSr);
    ZS_EXPECT (noiseTrack.overallConfidence < track.overallConfidence * 0.8f);
}

ZS_TEST (TempoAnalyzerFindsClickTrainBpm)
{
    namespace an = zarlino::source::analysis;

    // 120 BPM click train over 4 s (8 clicks, 0.5 s apart).
    juce::AudioBuffer<float> buffer (1, (int) (kSr * 4.0));
    buffer.clear();

    const int interval = (int) (0.5 * kSr);

    for (int k = 0; k < 8; ++k)
    {
        const int start = k * interval;

        for (int i = 0; i < 96 && start + i < buffer.getNumSamples(); ++i)
            buffer.getWritePointer (0)[start + i] += 0.9f
                * (float) std::exp (-(double) i / 16.0) * (((i % 2) == 0) ? 1.0f : -1.0f);
    }

    const auto tempo = an::TempoAnalyzer::analyse (buffer, kSr);

    ZS_EXPECT (tempo.valid);
    ZS_EXPECT_NEAR (tempo.bpm, 120.0, 4.0);
    ZS_EXPECT (tempo.confidence > 0.2f);

    juce::AudioBuffer<float> silence (1, (int) kSr);
    silence.clear();
    const auto none = an::TempoAnalyzer::analyse (silence, kSr);
    ZS_EXPECT (! none.valid);
    ZS_EXPECT (none.uncertain());
}

ZS_TEST (KeyAnalyzerFindsCMajorAndReportsSilence)
{
    namespace an = zarlino::source::analysis;

    // C major arpeggio C4 E4 G4 C5, each 0.5 s.
    juce::AudioBuffer<float> buffer (1, (int) (kSr * 2.0));
    buffer.clear();

    const double notes[] = { 261.63, 329.63, 392.0, 523.25 };

    for (int n = 0; n < 4; ++n)
    {
        const int start = n * (int) (0.5 * kSr);

        for (int i = 0; i < (int) (0.5 * kSr); ++i)
        {
            const double t = (double) i / kSr;
            const double env = juce::jmin (1.0, juce::jmin (t / 0.01, (0.5 - t) / 0.05));

            buffer.getWritePointer (0)[start + i] += (float) (0.4 * env)
                * (float) std::sin (juce::MathConstants<double>::twoPi * notes[n] * t);
        }
    }

    const auto key = an::KeyAnalyzer::analyse (buffer, kSr);

    ZS_EXPECT (key.valid);
    ZS_EXPECT (key.major);
    ZS_EXPECT (key.tonic == 0);
    ZS_EXPECT (key.description == "C major");

    juce::AudioBuffer<float> silence (1, (int) kSr);
    silence.clear();
    const auto none = an::KeyAnalyzer::analyse (silence, kSr);
    ZS_EXPECT (! none.valid);
    ZS_EXPECT (none.description == "Key uncertain");
}
