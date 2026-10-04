#include "core/analysis/KeyAnalyzer.h"

#include "dsp/ChannelConverter.h"
#include "dsp/Stft.h"

#include <cmath>

namespace zarlino::source::analysis
{

namespace
{
    // Krumhansl & Kessler (1982) key profiles, as used in the standard
    // Krumhansl-Schmuckler key-finding algorithm.
    constexpr double kMajorProfile[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09,
                                           2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    constexpr double kMinorProfile[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53,
                                           2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

    const char* const kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F",
                                         "F#", "G", "G#", "A", "A#", "B" };

    double correlation (const double* a, const double* b)
    {
        double meanA = 0.0, meanB = 0.0;

        for (int i = 0; i < 12; ++i)
        {
            meanA += a[i];
            meanB += b[i];
        }

        meanA /= 12.0;
        meanB /= 12.0;

        double num = 0.0, denA = 0.0, denB = 0.0;

        for (int i = 0; i < 12; ++i)
        {
            const double da = a[i] - meanA;
            const double db = b[i] - meanB;
            num += da * db;
            denA += da * da;
            denB += db * db;
        }

        if (denA <= 1.0e-12 || denB <= 1.0e-12)
            return 0.0;

        return num / std::sqrt (denA * denB);
    }
}

KeyEstimate KeyAnalyzer::analyse (const juce::AudioBuffer<float>& buffer,
                                  double sampleRate,
                                  Settings settings)
{
    KeyEstimate estimate;
    estimate.description = "Key uncertain";

    if (buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0 || sampleRate <= 0.0)
        return estimate;

    const auto mono = dsp::ChannelConverter::toMono (buffer);

    dsp::StftConfig config;
    dsp::Stft stft;
    stft.prepare (config, sampleRate);

    const int numBins = stft.getNumBins();
    const double binHz = sampleRate / (double) config.getFftSize();

    double chroma[12] = {};

    stft.forEachFrame (mono.getReadPointer (0), mono.getNumSamples(),
                       [&] (int, const float* bins)
    {
        for (int b = 1; b < numBins; ++b)
        {
            const double frequency = (double) b * binHz;

            // Keep the musically meaningful range; skip rumble and air.
            if (frequency < 55.0 || frequency > 2500.0)
                continue;

            const double midi = 69.0 + 12.0 * std::log2 (frequency / 440.0);
            const int pitchClass = ((int) std::lround (midi)) % 12;

            chroma[(pitchClass + 12) % 12] += (double) dsp::Stft::powerOf (bins, b);
        }
    });

    double total = 0.0;

    for (const double c : chroma)
        total += c;

    if (total <= 1.0e-12)
        return estimate;   // silence -> uncertain

    double normalised[12];

    for (int i = 0; i < 12; ++i)
        normalised[i] = chroma[i] / total;

    // Compare with all 24 rotated profiles.
    double bestScore = -2.0, secondScore = -2.0;
    int bestTonic = 0;
    bool bestMajor = true;

    for (int tonic = 0; tonic < 12; ++tonic)
    {
        double rotatedMajor[12], rotatedMinor[12];

        for (int i = 0; i < 12; ++i)
        {
            rotatedMajor[i] = kMajorProfile[(i - tonic + 12) % 12];
            rotatedMinor[i] = kMinorProfile[(i - tonic + 12) % 12];
        }

        const double majorScore = correlation (normalised, rotatedMajor);
        const double minorScore = correlation (normalised, rotatedMinor);

        if (majorScore > bestScore)
        {
            secondScore = bestScore;
            bestScore = majorScore;
            bestTonic = tonic;
            bestMajor = true;
        }
        else if (majorScore > secondScore)
        {
            secondScore = majorScore;
        }

        if (minorScore > bestScore)
        {
            secondScore = bestScore;
            bestScore = minorScore;
            bestTonic = tonic;
            bestMajor = false;
        }
        else if (minorScore > secondScore)
        {
            secondScore = minorScore;
        }
    }

    estimate.confidence = juce::jlimit (0.0f, 1.0f, (float) (bestScore - secondScore));

    if (bestScore >= (double) settings.minCorrelation
        && (bestScore - secondScore) >= (double) settings.minMargin)
    {
        estimate.valid = true;
        estimate.tonic = bestTonic;
        estimate.major = bestMajor;
        estimate.description = juce::String (kNoteNames[bestTonic])
                             + (bestMajor ? " major" : " minor");
    }

    return estimate;
}

} // namespace zarlino::source::analysis
