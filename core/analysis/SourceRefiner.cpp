#include "core/analysis/SourceRefiner.h"

#include "core/analysis/ArtifactAnalyzer.h"
#include "dsp/IStft.h"

#include <algorithm>
#include <cmath>

namespace zarlino::source::analysis
{

namespace
{

constexpr float kGateFloorGain = 0.15f; // deepest gate gain
constexpr float kSilenceEps = 1.0e-9f;
constexpr float kAcceptMargin = 0.005f; // composite must improve by this much
constexpr float kPreservationGuard = 0.05f;

float crossfade (float a, float b, float t) noexcept
{
    return a + (b - a) * juce::jlimit (0.0f, 1.0f, t);
}

/** Soft band weight of the profile at a frequency (linear crossfades around
    the 250 Hz and 2 kHz boundaries). */
float profileBandWeight (const extraction::TargetProfile& p, float hz) noexcept
{
    const float lo = juce::jmax (0.0f, p.lowBandWeight);
    const float mid = juce::jmax (0.0f, p.midBandWeight);
    const float hi = juce::jmax (0.0f, p.highBandWeight);

    if (hz < 180.0f)
        return lo;

    if (hz < 320.0f)
        return crossfade (lo, mid, (hz - 180.0f) / 140.0f);

    if (hz < 1600.0f)
        return mid;

    if (hz < 2500.0f)
        return crossfade (mid, hi, (hz - 1600.0f) / 900.0f);

    return hi;
}

} // namespace

SpectralGateStats applySpectralGatePass (juce::AudioBuffer<float>& audio,
                                         double sampleRate,
                                         const extraction::TargetProfile& profile,
                                         const dsp::StftConfig& config,
                                         ProgressFn progress)
{
    SpectralGateStats stats;

    const int numSamples = audio.getNumSamples();
    const int numChannels = audio.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0 || ! config.isValid() || ! (sampleRate > 0.0))
    {
        stats.completed = true;
        return stats;
    }

    dsp::Stft stft;
    stft.prepare (config, sampleRate);

    const int numBins = stft.getNumBins();
    const int numFrames = stft.getNumFrames (numSamples);

    if (numBins <= 0 || numFrames <= 0)
    {
        stats.completed = true;
        return stats;
    }

    const float* samples0 = audio.getReadPointer (0);
    const float* samples1 = audio.getReadPointer (numChannels > 1 ? 1 : 0);

    std::vector<float> bins0 ((size_t) numBins * 2, 0.0f);
    std::vector<float> bins1 ((size_t) numBins * 2, 0.0f);

    // ---- analysis pass: mono (max-of-channels) magnitude per bin ----------------
    std::vector<float> magnitudes ((size_t) numFrames * (size_t) numBins, 0.0f);
    std::vector<float> gains ((size_t) numFrames * (size_t) numBins, 1.0f);

    for (int f = 0; f < numFrames; ++f)
    {
        if (progress && (f % 64 == 0))
            if (! progress (0.55f * (float) f / (float) juce::jmax (1, numFrames),
                            "spectral gate: analysing"))
            {
                stats.completed = false;
                return stats;
            }

        const float* b0 = stft.computeFrame (samples0, numSamples, f);
        std::copy (b0, b0 + numBins * 2, bins0.begin());

        const float* b1 = stft.computeFrame (samples1, numSamples, f);
        std::copy (b1, b1 + numBins * 2, bins1.begin());

        auto* row = magnitudes.data() + (size_t) f * (size_t) numBins;

        for (int k = 0; k < numBins; ++k)
            row[k] = juce::jmax (dsp::Stft::magnitudeOf (bins0.data(), k),
                                 dsp::Stft::magnitudeOf (bins1.data(), k));
    }

    // ---- band protection: excess profile weight above the profile mean ----------
    const double binHz = sampleRate / (double) stft.getFftSize();

    std::vector<float> protection ((size_t) numBins, 0.0f);
    double weightSum = 0.0;

    for (int k = 0; k < numBins; ++k)
    {
        const float w = profileBandWeight (profile, (float) ((double) k * binHz));
        protection[(size_t) k] = w;
        weightSum += (double) w;
    }

    const float meanWeight = numBins > 0 ? (float) (weightSum / (double) numBins) : 1.0f;

    for (int k = 0; k < numBins; ++k)
    {
        const float w = meanWeight > 1.0e-6f ? protection[(size_t) k] / meanWeight : 1.0f;

        // Protection = how far above the profile mean this band sits (0..1).
        protection[(size_t) k] = juce::jlimit (0.0f, 1.0f, w - 1.0f);
    }

    // ---- gain curve: short-run ("bursty") bin suppression --------------------------
    // Mirrors the burstyBinFraction definition used by the artifact analyzer: a
    // bin is active when its magnitude exceeds -40 dB relative to the bin's own
    // 95th-percentile peak; active runs of at most `kMaxRunFrames` frames are
    // short-lived blobs (musical noise, clicks, truncated leakage), while
    // sustained or evolving content forms long runs and passes untouched.
    // Edge frames are excluded, as in the analyzer - partial analysis windows
    // at the boundaries would otherwise register as spurious short runs.
    constexpr int kMaxRunFrames = 6;
    constexpr int kEdgeSkipFrames = 2;

    std::vector<float> column ((size_t) numFrames, 0.0f);
    std::vector<float> sorted ((size_t) numFrames, 0.0f);

    for (int k = 0; k < numBins; ++k)
    {
        for (int f = 0; f < numFrames; ++f)
            column[(size_t) f] = magnitudes[(size_t) f * (size_t) numBins + (size_t) k];

        // nth_element rearranges its range, so take the percentile from a copy
        // and keep `column` in time order for the run scanner below.
        const size_t peakIdx = (size_t) juce::jmin (numFrames - 1, (numFrames * 95) / 100);
        std::copy (column.begin(), column.end(), sorted.begin());
        std::nth_element (sorted.begin(), sorted.begin() + (long) peakIdx, sorted.end());
        const float peak = sorted[peakIdx];

        if (peak <= kSilenceEps)
            continue;

        const float activeThreshold = peak * 0.01f; // -40 dB

        int f = kEdgeSkipFrames;

        while (f < numFrames - kEdgeSkipFrames)
        {
            if (column[(size_t) f] <= activeThreshold)
            {
                ++f;
                continue;
            }

            int runEnd = f;

            while (runEnd < numFrames - kEdgeSkipFrames
                   && column[(size_t) runEnd] > activeThreshold)
                ++runEnd;

            if (runEnd - f <= kMaxRunFrames)
            {
                // Short-lived blob: gate towards the floor, lifted by band
                // protection for target-dominant bands.
                const float gated = kGateFloorGain
                                    + (1.0f - kGateFloorGain) * protection[(size_t) k];

                for (int j = f; j < runEnd; ++j)
                    gains[(size_t) j * (size_t) numBins + (size_t) k] = gated;
            }

            f = runEnd;
        }
    }

    // NOTE: the gains are deliberately NOT smoothed along time. A 3-tap
    // smoother dilutes short-run suppression (0.5 * floor + 0.5 * unity) and
    // would leave most of a blob's energy untouched - the exact artifact this
    // pass exists to remove.

    // ---- apply pass -----------------------------------------------------------------
    dsp::IStft istft;
    istft.prepare (config, sampleRate);
    istft.begin (numSamples, numChannels);

    double gainSum = 0.0;
    long long binCount = 0;
    long long attenuated = 0;

    for (int f = 0; f < numFrames; ++f)
    {
        if (progress && (f % 64 == 0))
            if (! progress (0.55f + 0.4f * (float) f / (float) juce::jmax (1, numFrames),
                            "spectral gate: applying"))
            {
                stats.completed = false;
                return stats;
            }

        const float* b0 = stft.computeFrame (samples0, numSamples, f);
        std::copy (b0, b0 + numBins * 2, bins0.begin());

        if (numChannels > 1)
        {
            const float* b1 = stft.computeFrame (samples1, numSamples, f);
            std::copy (b1, b1 + numBins * 2, bins1.begin());
        }

        const auto* row = gains.data() + (size_t) f * (size_t) numBins;

        for (int k = 0; k < numBins; ++k)
        {
            const float gain = row[k];

            bins0[(size_t) k * 2] *= gain;
            bins0[(size_t) k * 2 + 1] *= gain;

            if (numChannels > 1)
            {
                bins1[(size_t) k * 2] *= gain;
                bins1[(size_t) k * 2 + 1] *= gain;
            }

            gainSum += (double) gain;
            ++binCount;

            if (gain < 0.9f)
                ++attenuated;
        }

        istft.addFrame (0, f, bins0.data());

        if (numChannels > 1)
            istft.addFrame (1, f, bins1.data());
    }

    auto refined = istft.finalise();

    for (int ch = 0; ch < numChannels; ++ch)
        if (ch < refined.getNumChannels())
            audio.copyFrom (ch, 0, refined.getReadPointer (ch),
                            juce::jmin (numSamples, refined.getNumSamples()));

    stats.completed = true;
    stats.meanGain = binCount > 0 ? (float) (gainSum / (double) binCount) : 1.0f;
    stats.attenuatedBinFraction = binCount > 0 ? (float) ((double) attenuated / (double) binCount) : 0.0f;

    return stats;
}

RefinementOutcome refine (juce::AudioBuffer<float>& audio,
                          const juce::AudioBuffer<float>& mixture,
                          double sampleRate,
                          const extraction::TargetProfile& profile,
                          const CandidateResult& mixtureCandidates,
                          extraction::QualityMetrics& quality,
                          AcceptanceThresholds thresholds,
                          int maxPasses,
                          ProgressFn progress)
{
    RefinementOutcome outcome;

    if (maxPasses <= 0 || audio.getNumSamples() <= 0 || audio.getNumChannels() <= 0)
        return outcome;

    const auto composite = [] (const extraction::QualityMetrics& q)
    {
        return q.targetPreservation - q.artifactScore;
    };

    for (int pass = 1; pass <= maxPasses; ++pass)
    {
        const float passBase = (float) (pass - 1) / (float) maxPasses;

        const auto withinPass = [&] (float within, const juce::String& message) -> bool
        {
            if (! progress)
                return true;

            return progress (passBase + juce::jlimit (0.0f, 1.0f, within) / (float) maxPasses,
                             message);
        };

        if (! withinPass (0.0f, "refinement pass " + juce::String (pass)
                                     + " of " + juce::String (maxPasses)))
        {
            outcome.notes.push_back ("refinement cancelled before pass "
                                     + std::to_string (pass));
            return outcome;
        }

        ++outcome.passesRun;

        juce::AudioBuffer<float> trial (audio); // pass works on a copy

        const auto stats = applySpectralGatePass (
            trial, sampleRate, profile, {},
            [&] (float within, const juce::String& message)
            {
                return withinPass (0.05f + 0.60f * within, message);
            });

        if (! stats.completed)
        {
            outcome.notes.push_back ("refinement cancelled during spectral gate pass "
                                     + std::to_string (pass));
            --outcome.passesRun;
            return outcome;
        }

        if (! withinPass (0.70f, "re-measuring quality"))
        {
            outcome.notes.push_back ("refinement cancelled after gate pass "
                                     + std::to_string (pass));
            --outcome.passesRun;
            return outcome;
        }

        const auto measured = analyseArtifacts (trial, mixture, sampleRate,
                                                profile, mixtureCandidates);

        const auto before = quality;
        const float compositeBefore = composite (before);
        const float compositeAfter = composite (measured.quality);

        const bool improves = compositeAfter > compositeBefore + kAcceptMargin
                           && measured.quality.targetPreservation
                                  >= before.targetPreservation - kPreservationGuard;

        if (! improves)
        {
            outcome.notes.push_back ("pass " + std::to_string (pass)
                                     + " rejected (no improvement): preservation "
                                     + juce::String (before.targetPreservation, 3).toStdString()
                                     + " -> "
                                     + juce::String (measured.quality.targetPreservation, 3).toStdString()
                                     + ", artifactScore "
                                     + juce::String (before.artifactScore, 3).toStdString()
                                     + " -> "
                                     + juce::String (measured.quality.artifactScore, 3).toStdString());
            break;
        }

        audio = trial;
        quality = measured.quality;
        ++outcome.passesAccepted;
        outcome.improved = true;

        outcome.notes.push_back ("pass " + std::to_string (pass)
                                 + " accepted: preservation "
                                 + juce::String (before.targetPreservation, 3).toStdString()
                                 + " -> "
                                 + juce::String (quality.targetPreservation, 3).toStdString()
                                 + ", artifactScore "
                                 + juce::String (before.artifactScore, 3).toStdString()
                                 + " -> "
                                 + juce::String (quality.artifactScore, 3).toStdString()
                                 + "; gated "
                                 + juce::String (stats.attenuatedBinFraction * 100.0f, 1).toStdString()
                                 + "% of time-frequency bins (mean gain "
                                 + juce::String (stats.meanGain, 3).toStdString() + ")");

        if (! withinPass (1.0f, "pass " + juce::String (pass) + " accepted"))
            return outcome;

        if (thresholds.accepts (quality))
        {
            outcome.notes.push_back ("quality now within thresholds - refinement complete");
            break;
        }

        if (pass == maxPasses)
            outcome.notes.push_back ("refinement pass budget exhausted ("
                                     + std::to_string (maxPasses) + " passes)");
    }

    return outcome;
}

} // namespace zarlino::source::analysis
