#include "core/analysis/ArtifactAnalyzer.h"

#include "dsp/ChannelConverter.h"

#include <algorithm>
#include <cmath>

namespace zarlino::source::analysis
{

float burstyBinFraction (const float* mono, int numSamples, double sampleRate,
                         const dsp::StftConfig& config, int maxRunFrames, int skipEdgeFrames)
{
    if (mono == nullptr || numSamples <= 0 || sampleRate <= 0.0 || ! config.isValid())
        return 0.0f;

    dsp::Stft stft;
    stft.prepare (config, sampleRate);

    const int numBins = stft.getNumBins();
    const int numFrames = stft.getNumFrames (numSamples);

    if (numFrames <= 0 || numBins <= 0)
        return 0.0f;

    std::vector<float> power ((size_t) numFrames * (size_t) numBins, 0.0f);

    for (int f = 0; f < numFrames; ++f)
    {
        const float* bins = stft.computeFrame (mono, numSamples, f);

        for (int b = 0; b < numBins; ++b)
            power[(size_t) f * (size_t) numBins + (size_t) b] = dsp::Stft::powerOf (bins, b);
    }

    // Activity mask: above -40 dB relative to the loudest bin of the frame.
    // (-40 dB keeps stationary window-leakage bins steadily active instead of
    // flickering around a -60 dB fence, which would otherwise inflate runs.)
    std::vector<uint8_t> active ((size_t) numFrames * (size_t) numBins, 0);

    for (int f = 0; f < numFrames; ++f)
    {
        float frameMax = 0.0f;

        for (int b = 0; b < numBins; ++b)
            frameMax = juce::jmax (frameMax, power[(size_t) f * (size_t) numBins + (size_t) b]);

        const float threshold = juce::jmax (1.0e-12f, frameMax * 1.0e-4f);

        for (int b = 0; b < numBins; ++b)
            if (power[(size_t) f * (size_t) numBins + (size_t) b] > threshold)
                active[(size_t) f * (size_t) numBins + (size_t) b] = 1;
    }

    const int begin = juce::jlimit (0, numFrames, skipEdgeFrames);
    const int end = juce::jmax (begin, numFrames - skipEdgeFrames);

    long long activeCount = 0;
    long long burstyCount = 0;

    for (int b = 0; b < numBins; ++b)
    {
        int f = begin;

        while (f < end)
        {
            if (active[(size_t) f * (size_t) numBins + (size_t) b] == 0)
            {
                ++f;
                continue;
            }

            const int runStart = f;

            while (f < end && active[(size_t) f * (size_t) numBins + (size_t) b] != 0)
                ++f;

            const int run = f - runStart;

            activeCount += run;

            if (run <= maxRunFrames)
                burstyCount += run;
        }
    }

    if (activeCount == 0)
        return 0.0f;

    return juce::jlimit (0.0f, 1.0f, (float) ((double) burstyCount / (double) activeCount));
}

float onsetMatchRate (const std::vector<dsp::Onset>& reference,
                      const std::vector<dsp::Onset>& candidates,
                      double sampleRate,
                      double toleranceSeconds)
{
    if (reference.empty())
        return 1.0f;   // nothing to preserve

    if (sampleRate <= 0.0)
        return 0.0f;

    const int tolerance = (int) std::llround (toleranceSeconds * sampleRate);
    int matched = 0;

    for (const auto& r : reference)
    {
        for (const auto& c : candidates)
        {
            if (std::abs (c.sampleIndex - r.sampleIndex) <= tolerance)
            {
                ++matched;
                break;
            }
        }
    }

    return (float) matched / (float) reference.size();
}

ArtifactReport analyseArtifacts (const juce::AudioBuffer<float>& target,
                                 const juce::AudioBuffer<float>& mixture,
                                 double sampleRate,
                                 const extraction::TargetProfile& profile,
                                 const CandidateResult& mixtureCandidates)
{
    ArtifactReport report;
    auto& q = report.quality;

    if (target.getNumSamples() <= 0 || mixture.getNumSamples() <= 0 || sampleRate <= 0.0)
    {
        report.notes.push_back ("artifact analysis skipped: empty input");
        return report;
    }

    const auto targetFeatures = computeFeatures (target, sampleRate);
    const auto mixtureFeatures = computeFeatures (mixture, sampleRate);

    const size_t nA = targetFeatures.frames.size();
    const size_t nB = mixtureFeatures.frames.size();
    const size_t nC = mixtureCandidates.frameAffinity.size();
    const int nFrames = (int) std::min ({ nA, nB, nC });

    // ---- targetPreservation ------------------------------------------------------
    double strongTarget = 0.0, strongMixture = 0.0;
    int strongCount = 0;
    double allTarget = 0.0, allMixture = 0.0;
    double weakTarget = 0.0;

    for (int i = 0; i < nFrames; ++i)
    {
        const double mixE = (double) mixtureFeatures.frames[(size_t) i].totalEnergy;
        const double tgtE = (double) targetFeatures.frames[(size_t) i].totalEnergy;
        const float affinity = mixtureCandidates.frameAffinity[(size_t) i];

        allTarget += tgtE;
        allMixture += mixE;

        if (affinity >= 0.5f)
        {
            strongTarget += tgtE;
            strongMixture += mixE;
            ++strongCount;
        }
        else if (affinity < 0.3f)
        {
            weakTarget += tgtE;
        }
    }

    if (strongCount > 0 && strongMixture > 0.0)
    {
        q.targetPreservation = (float) juce::jlimit (0.0, 1.0, strongTarget / strongMixture);
        report.notes.push_back ("targetPreservation: energy ratio over strongly-matching frames");
    }
    else if (allMixture > 0.0)
    {
        q.targetPreservation = (float) juce::jlimit (0.0, 1.0, allTarget / allMixture);
        report.notes.push_back ("targetPreservation: whole-region fallback (no strongly-matching frames)");
    }

    // ---- leakageEstimate ----------------------------------------------------------
    if (allTarget > 0.0)
        q.leakageEstimate = (float) juce::jlimit (0.0, 1.0, weakTarget / allTarget);

    report.notes.push_back ("leakageEstimate: share of target energy in non-matching frames");

    // ---- transientIntegrity --------------------------------------------------------
    const auto mixtureOnsets = dsp::TransientDetector::detect (mixture, sampleRate);
    const auto targetOnsets = dsp::TransientDetector::detect (target, sampleRate);

    q.transientIntegrity = onsetMatchRate (mixtureOnsets, targetOnsets, sampleRate);
    report.transientIntegrityComputed = true;

    // ---- spectralIntegrity (musical-noise proxy) ------------------------------------
    const auto targetMono = dsp::ChannelConverter::toMono (target);
    const float bursty = burstyBinFraction (targetMono.getReadPointer (0),
                                            targetMono.getNumSamples(), sampleRate);
    q.spectralIntegrity = juce::jlimit (0.0f, 1.0f, 1.0f - bursty);

    // ---- aggregate -------------------------------------------------------------------
    q.artifactScore = juce::jlimit (0.0f, 1.0f,
                                    0.5f * q.leakageEstimate + 0.5f * (1.0f - q.spectralIntegrity));

    q.confidence = juce::jlimit (0.0f, 1.0f,
                                 profile.interpretationConfidence
                                     * (0.4f + 0.6f * q.targetPreservation)
                                     * (1.0f - 0.5f * q.artifactScore));

    report.notes.push_back ("spectralIntegrity: 1 - short-lived-bin fraction (heuristic)");
    report.notes.push_back ("artifactScore: combined leakage + musical-noise penalty");
    report.notes.push_back ("phaseIntegrity: NOT YET COMPUTED (no fabricated value)");
    report.notes.push_back ("warbling/chirping detection: NOT YET IMPLEMENTED");

    return report;
}

} // namespace zarlino::source::analysis
