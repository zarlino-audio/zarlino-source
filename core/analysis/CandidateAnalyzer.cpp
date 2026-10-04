#include "core/analysis/CandidateAnalyzer.h"

#include <algorithm>
#include <cmath>

namespace zarlino::source::analysis
{

namespace
{

float bandSimilarity (const FrameFeature& f, const extraction::TargetProfile& p)
{
    const double pl = (double) juce::jmax (0.0f, p.lowBandWeight);
    const double pm = (double) juce::jmax (0.0f, p.midBandWeight);
    const double ph = (double) juce::jmax (0.0f, p.highBandWeight);
    const double psum = pl + pm + ph;

    if (psum <= 0.0)
        return 0.5f;

    const double t = (double) f.bandTotal();

    if (t <= 0.0)
        return 0.0f;

    const double fn[3] = { (double) f.lowEnergy / t, (double) f.midEnergy / t, (double) f.highEnergy / t };
    const double pn[3] = { pl / psum, pm / psum, ph / psum };

    double dot = 0.0, na = 0.0, nb = 0.0;

    for (int i = 0; i < 3; ++i)
    {
        dot += fn[i] * pn[i];
        na += fn[i] * fn[i];
        nb += pn[i] * pn[i];
    }

    if (na <= 0.0 || nb <= 0.0)
        return 0.0f;

    return (float) juce::jlimit (0.0, 1.0, dot / std::sqrt (na * nb));
}

float centroidSimilarity (const FrameFeature& f, const extraction::TargetProfile& p)
{
    const double tol = juce::jmax (0.05f, p.centroidTolerance);
    const double ratio = (double) f.centroidHz / (double) juce::jmax (1.0f, p.targetCentroidHz);
    const double octaves = std::abs (std::log2 (juce::jmax (1.0, ratio)));

    // Gaussian-ish falloff: 1 at the target, ~0.6 at one tolerance unit, ~0.1 at two.
    return (float) std::exp (-0.5 * (octaves / tol) * (octaves / tol));
}

float textureSimilarity (const FrameFeature& f, const extraction::TargetProfile& p)
{
    // Flatness is noise-ness; harmonicity is its opposite. Compare both on
    // the same axis.
    const float expectedFlat = 1.0f - juce::jlimit (0.0f, 1.0f, p.harmonicity);
    return juce::jlimit (0.0f, 1.0f, 1.0f - std::abs (f.flatness - expectedFlat));
}

std::vector<float> smooth (const std::vector<float>& values, int window)
{
    if (window <= 1 || values.empty())
        return values;

    const int half = window / 2;
    const int n = (int) values.size();
    std::vector<float> out ((size_t) n, 0.0f);

    for (int i = 0; i < n; ++i)
    {
        float sum = 0.0f;
        int count = 0;

        for (int k = i - half; k <= i + half; ++k)
        {
            if (k >= 0 && k < n)
            {
                sum += values[(size_t) k];
                ++count;
            }
        }

        out[(size_t) i] = count > 0 ? sum / (float) count : 0.0f;
    }

    return out;
}

} // namespace

CandidateResult analyse (const FeatureMap& features,
                         const extraction::TargetProfile& profile,
                         CandidateConfig config)
{
    CandidateResult result;

    const int n = (int) features.frames.size();

    if (n <= 0)
        return result;

    const bool hasCentroid = profile.targetCentroidHz > 0.0f;

    constexpr float wBand = 0.55f;
    constexpr float wTexture = 0.25f;
    const float wCentroid = hasCentroid ? 0.20f : 0.0f;
    const float wSum = wBand + wTexture + wCentroid;

    std::vector<float> raw ((size_t) n, 0.0f);

    for (int i = 0; i < n; ++i)
    {
        const auto& f = features.frames[(size_t) i];

        float s = wBand * bandSimilarity (f, profile)
                + wTexture * textureSimilarity (f, profile);

        if (hasCentroid)
            s += wCentroid * centroidSimilarity (f, profile);

        raw[(size_t) i] = s / wSum;
    }

    result.frameAffinity = smooth (raw, config.smoothingFrames);

    // ---- region extraction (with hysteresis) ---------------------------------
    const float enter = config.minRegionAffinity;
    const float exit = config.minRegionAffinity * 0.8f;

    std::vector<CandidateRegion> regions;
    int regionStart = -1;

    auto closeRegion = [&] (int endIndexExclusive)
    {
        if (regionStart < 0)
            return;

        const double startTime = features.frameTime (regionStart);
        const double endTime = features.frameTime (endIndexExclusive);

        float sum = 0.0f;

        for (int i = regionStart; i < endIndexExclusive && i < n; ++i)
            sum += result.frameAffinity[(size_t) i];

        const float mean = (endIndexExclusive > regionStart)
                               ? sum / (float) (endIndexExclusive - regionStart)
                               : 0.0f;

        if (endTime - startTime >= config.minRegionSeconds)
            regions.push_back ({ startTime, endTime, mean });

        regionStart = -1;
    };

    for (int i = 0; i < n; ++i)
    {
        const float a = result.frameAffinity[(size_t) i];

        if (regionStart < 0)
        {
            if (a >= enter)
                regionStart = i;
        }
        else if (a < exit)
        {
            closeRegion (i);
        }
    }

    closeRegion (n);

    std::sort (regions.begin(), regions.end(),
               [] (const CandidateRegion& a, const CandidateRegion& b) { return a.affinity > b.affinity; });

    if ((int) regions.size() > config.maxRegions)
        regions.resize ((size_t) config.maxRegions);

    result.regions = std::move (regions);

    // ---- confidence -------------------------------------------------------------
    const float best = *std::max_element (result.frameAffinity.begin(), result.frameAffinity.end());

    result.overallConfidence = juce::jlimit (0.0f, 1.0f,
                                             best * juce::jlimit (0.0f, 1.0f, profile.interpretationConfidence));

    return result;
}

} // namespace zarlino::source::analysis
