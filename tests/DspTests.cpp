#include "TestRunner.h"

#include "core/audio/AudioFileIO.h"
#include "core/audio/CaptureBuffer.h"
#include "dsp/ChannelConverter.h"
#include "dsp/FadeProcessor.h"
#include "dsp/IStft.h"
#include "dsp/Resampler.h"
#include "dsp/Stft.h"
#include "dsp/TransientDetector.h"
#include "dsp/ZeroCrossing.h"

#include <random>

namespace
{
    constexpr double kSampleRate = 48000.0;

    juce::AudioBuffer<float> makeSine (double frequency, double seconds, double sampleRate,
                                       int numChannels = 1, float amplitude = 0.5f)
    {
        const int numSamples = (int) std::llround (seconds * sampleRate);
        juce::AudioBuffer<float> buffer (numChannels, numSamples);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            const double phaseOffset = ch * 0.25;

            for (int i = 0; i < numSamples; ++i)
                data[i] = amplitude * (float) std::sin (juce::MathConstants<double>::twoPi
                                                        * frequency * (double) i / sampleRate + phaseOffset);
        }

        return buffer;
    }
}

//==============================================================================
ZS_TEST (StftIStftRoundTripIsExactInInterior)
{
    juce::AudioBuffer<float> noise (1, 48000);
    std::mt19937 rng (1234);
    std::uniform_real_distribution<float> dist (-0.5f, 0.5f);
    auto* data = noise.getWritePointer (0);

    for (int i = 0; i < noise.getNumSamples(); ++i)
        data[i] = dist (rng);

    const zarlino::source::dsp::StftConfig config;   // default 2048 / 512 / Hann

    zarlino::source::dsp::Stft stft;
    stft.prepare (config, kSampleRate);

    zarlino::source::dsp::IStft istft;
    istft.prepare (config, kSampleRate);
    istft.begin (noise.getNumSamples(), 1);

    const int numFrames = stft.getNumFrames (noise.getNumSamples());

    for (int f = 0; f < numFrames; ++f)
        istft.addFrame (0, f, stft.computeFrame (data, noise.getNumSamples(), f));

    auto reconstructed = istft.finalise();

    ZS_EXPECT (reconstructed.getNumSamples() == noise.getNumSamples());

    // Exact reconstruction is expected wherever enough frames overlap.
    float maxError = 0.0f;

    for (int i = config.getFftSize(); i < noise.getNumSamples() - config.getFftSize(); ++i)
        maxError = juce::jmax (maxError, std::abs (reconstructed.getReadPointer (0)[i] - data[i]));

    ZS_EXPECT (maxError < 1.0e-4f);
}

ZS_TEST (ResamplerPreservesToneAndLength)
{
    auto sine = makeSine (440.0, 0.5, kSampleRate, 1, 0.8f);

    auto up = zarlino::source::dsp::Resampler::process (sine, kSampleRate, 96000.0);

    // 0.5 s of audio at 96 kHz -> 48000 samples.
    ZS_EXPECT (up.getNumSamples() == 48000);

    // Skip the first/last samples (filter edges) for the checks.
    float peak = 0.0f;

    for (int i = 1000; i < up.getNumSamples() - 1000; ++i)
        peak = juce::jmax (peak, std::abs (up.getReadPointer (0)[i]));

    ZS_EXPECT_NEAR (peak, 0.8, 0.02);

    const double freq = zarlino::source::dsp::ZeroCrossing::estimateFrequencyFromRate (
        up.getReadPointer (0) + 1000, up.getNumSamples() - 2000, 96000.0);

    ZS_EXPECT_NEAR (freq, 440.0, 4.0);

    auto down = zarlino::source::dsp::Resampler::process (sine, kSampleRate, 44100.0);
    ZS_EXPECT (down.getNumSamples() == 22050);
}

ZS_TEST (ChannelConverterBasics)
{
    juce::AudioBuffer<float> stereo (2, 100);
    stereo.getWritePointer (0)[0] = 1.0f;
    stereo.getWritePointer (1)[0] = -1.0f;

    auto mono = zarlino::source::dsp::ChannelConverter::toMono (stereo);

    ZS_EXPECT (mono.getNumChannels() == 1);
    ZS_EXPECT_NEAR (mono.getReadPointer (0)[0], 0.0, 1.0e-7);

    auto backToStereo = zarlino::source::dsp::ChannelConverter::toStereo (mono);
    ZS_EXPECT (backToStereo.getNumChannels() == 2);
    ZS_EXPECT_NEAR (backToStereo.getReadPointer (0)[0], backToStereo.getReadPointer (1)[0], 1.0e-7);

    juce::AudioBuffer<float> mid, side;
    zarlino::source::dsp::ChannelConverter::toMidSide (stereo, mid, side);
    auto recombined = zarlino::source::dsp::ChannelConverter::fromMidSide (mid, side);

    ZS_EXPECT_NEAR (recombined.getReadPointer (0)[0], 1.0, 1.0e-6);
    ZS_EXPECT_NEAR (recombined.getReadPointer (1)[0], -1.0, 1.0e-6);
}

ZS_TEST (FadeProcessorEndpoints)
{
    juce::AudioBuffer<float> buffer (1, 4800);

    // Full-scale material so the fade gains are directly observable at every
    // sample (a cleared buffer would hide every gain except at sample 0).
    auto* data = buffer.getWritePointer (0);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
        data[i] = 1.0f;

    zarlino::source::dsp::FadeProcessor::applyFadeIn (buffer, 0, 480);

    ZS_EXPECT_NEAR (buffer.getReadPointer (0)[0], 0.0, 1.0e-9);
    ZS_EXPECT_NEAR (buffer.getReadPointer (0)[479], 1.0, 1.0e-6);

    zarlino::source::dsp::FadeProcessor::applyFadeOut (buffer, 4800, 480);

    ZS_EXPECT_NEAR (buffer.getReadPointer (0)[4799], 0.0, 1.0e-9);
    ZS_EXPECT_NEAR (buffer.getReadPointer (0)[4800 - 480], 1.0, 1.0e-6);
}

ZS_TEST (ZeroCrossingCountsAndEstimates)
{
    auto sine = makeSine (100.0, 1.0, kSampleRate, 1, 1.0f);

    const int crossings = zarlino::source::dsp::ZeroCrossing::countCrossings (
        sine.getReadPointer (0), sine.getNumSamples());

    ZS_EXPECT_NEAR (crossings, 200, 2);

    const double freq = zarlino::source::dsp::ZeroCrossing::estimateFrequencyFromRate (
        sine.getReadPointer (0), sine.getNumSamples(), kSampleRate);

    ZS_EXPECT_NEAR (freq, 100.0, 1.0);

    // Crossing snapping: noisy ramp to a known crossing near sample 1000.
    juce::AudioBuffer<float> ramp (1, 2000);
    auto* d = ramp.getWritePointer (0);

    for (int i = 0; i < 2000; ++i)
    {
        const float base = ((float) i - 1000.4f) * 0.001f;
        d[i] = base + (((i * 37) % 7) - 3) * 0.0001f;
    }

    const int snapped = zarlino::source::dsp::ZeroCrossing::findNearestCrossing (d, 2000, 1000, 20);
    ZS_EXPECT (std::abs (snapped - 1000) <= 2);
}

ZS_TEST (TransientDetectorFindsClickTrain)
{
    const int numSamples = (int) kSampleRate * 2;
    juce::AudioBuffer<float> buffer (1, numSamples);
    buffer.clear();

    const int clickInterval = (int) kSampleRate / 2;   // 2 clicks per second
    const int expectedOnsets = 3;                       // at 0.5 s, 1.0 s, 1.5 s

    for (int k = 1; k <= expectedOnsets; ++k)
    {
        const int start = k * clickInterval;
        auto* d = buffer.getWritePointer (0);

        for (int i = 0; i < 96 && start + i < numSamples; ++i)
            d[start + i] += 0.9f * (float) std::exp (-(double) i / 16.0) * (((i % 2) == 0) ? 1.0f : -1.0f);
    }

    auto onsets = zarlino::source::dsp::TransientDetector::detect (buffer, kSampleRate);

    ZS_EXPECT ((int) onsets.size() == expectedOnsets);

    for (int k = 0; k < (int) onsets.size() && k < expectedOnsets; ++k)
    {
        const int expected = (k + 1) * clickInterval;
        // detection is allowed to sit a little late (half-window latency)
        ZS_EXPECT (std::abs (onsets[(size_t) k].sampleIndex - expected) < (int) (0.02 * kSampleRate));
    }
}

ZS_TEST (CaptureBufferPushPopAndWrap)
{
    zarlino::source::audio::CaptureBuffer capture;
    capture.prepare (2, 4096);

    juce::AudioBuffer<float> push (2, 1000);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 1000; ++i)
            push.getWritePointer (ch)[i] = (float) (ch + 1) + (float) i * 0.001f;

    ZS_EXPECT (capture.pushBlock (push, 1000) == 1000);
    ZS_EXPECT (capture.getNumReady() == 1000);

    juce::AudioBuffer<float> pop (2, 400);
    ZS_EXPECT (capture.popFrames (pop, 400) == 400);
    ZS_EXPECT_NEAR (pop.getReadPointer (0)[0], 1.0, 1.0e-6);
    ZS_EXPECT_NEAR (pop.getReadPointer (1)[0], 2.0, 1.0e-6);
    ZS_EXPECT (capture.getNumReady() == 600);

    // Push more to force a wrap around the ring.
    ZS_EXPECT (capture.pushBlock (push, 1000) == 1000);
    ZS_EXPECT (capture.getNumReady() == 1600);

    juce::AudioBuffer<float> popAll (2, 1600);
    ZS_EXPECT (capture.popFrames (popAll, 1600) == 1600);
    ZS_EXPECT (capture.getNumReady() == 0);
}

ZS_TEST (WavRoundTripAllDepths)
{
    auto sine = makeSine (220.0, 0.1, 44100.0, 2, 0.7f);
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("zarlino_source_tests");
    dir.createDirectory();

    struct Case { zarlino::source::audio::AudioFileIO::BitDepth depth; float tolerance; const char* name; };

    const Case cases[] = {
        { zarlino::source::audio::AudioFileIO::BitDepth::pcm16,  4.0e-4f, "wav16.wav" },
        { zarlino::source::audio::AudioFileIO::BitDepth::pcm24,  1.0e-5f, "wav24.wav" },
        { zarlino::source::audio::AudioFileIO::BitDepth::float32, 1.0e-6f, "wav32f.wav" },
    };

    for (const auto& c : cases)
    {
        const auto file = dir.getChildFile (c.name);
        file.deleteFile();

        juce::String error;

        ZS_EXPECT (zarlino::source::audio::AudioFileIO::writeWav (file, sine, 44100.0, c.depth, &error));

        if (error.isNotEmpty())
            std::cout << "  write error: " << error << std::endl;

        auto loaded = zarlino::source::audio::AudioFileIO::load (file);

        ZS_EXPECT (loaded.success);
        ZS_EXPECT (loaded.document.buffer.getNumChannels() == 2);
        ZS_EXPECT (loaded.document.buffer.getNumSamples() == sine.getNumSamples());
        ZS_EXPECT_NEAR (loaded.document.sampleRate, 44100.0, 0.001);

        float maxError = 0.0f;

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < sine.getNumSamples(); ++i)
                maxError = juce::jmax (maxError, std::abs (loaded.document.buffer.getReadPointer (ch)[i]
                                                           - sine.getReadPointer (ch)[i]));

        ZS_EXPECT (maxError < c.tolerance);
    }
}
