#include "inference/implementations/OnnxSeparator.h"

#include "core/analysis/FrameFeatures.h"
#include "dsp/ChannelConverter.h"
#include "dsp/Resampler.h"

#include <algorithm>
#include <cmath>

namespace zarlino::source::inference
{

namespace
{

/** Nearest sample rate from the package's declared list. */
double chooseModelRate (const ModelPackage& package, double requestRate)
{
    double best = requestRate;

    for (const double rate : package.sampleRates)
        if (std::abs (rate - requestRate) < std::abs (best - requestRate))
            best = rate;

    return best;
}

/** Scores a stem against the target profile using computed features only
    (band-distribution cosine + flatness/harmonicity agreement). Used to pick
    the target stem from a multi-stem model - not a stem-name lookup. */
float profileScore (const juce::AudioBuffer<float>& stem, double sampleRate,
                    const extraction::TargetProfile& profile)
{
    const auto features = analysis::computeFeatures (stem, sampleRate);

    if (features.frames.empty())
        return 0.0f;

    double low = 0.0, mid = 0.0, high = 0.0, flat = 0.0;
    const auto n = (double) features.frames.size();

    for (const auto& f : features.frames)
    {
        low += (double) f.lowEnergy;
        mid += (double) f.midEnergy;
        high += (double) f.highEnergy;
        flat += (double) f.flatness;
    }

    low /= n;
    mid /= n;
    high /= n;
    flat /= n;

    const double total = low + mid + high;

    if (total <= 0.0)
        return 0.0f;

    const double fn[3] = { low / total, mid / total, high / total };

    const double pl = (double) juce::jmax (0.0f, profile.lowBandWeight);
    const double pm = (double) juce::jmax (0.0f, profile.midBandWeight);
    const double ph = (double) juce::jmax (0.0f, profile.highBandWeight);
    const double ps = pl + pm + ph;

    if (ps <= 0.0)
        return 0.0f;

    const double pn[3] = { pl / ps, pm / ps, ph / ps };

    double dot = 0.0, na = 0.0, nb = 0.0;

    for (int i = 0; i < 3; ++i)
    {
        dot += fn[i] * pn[i];
        na += fn[i] * fn[i];
        nb += pn[i] * pn[i];
    }

    const float bandScore = (na > 0.0 && nb > 0.0)
                                ? (float) juce::jlimit (0.0, 1.0, dot / std::sqrt (na * nb))
                                : 0.5f;

    const float expectedFlat = 1.0f - juce::jlimit (0.0f, 1.0f, profile.harmonicity);
    const float textureScore = juce::jlimit (0.0f, 1.0f,
                                             1.0f - std::abs ((float) flat - expectedFlat));

    return 0.65f * bandScore + 0.35f * textureScore;
}

juce::AudioBuffer<float> ensureStereo (const juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() == 2)
        return buffer;

    if (buffer.getNumChannels() == 1)
        return dsp::ChannelConverter::toStereo (buffer);

    // More than two channels: keep the first two (documented simplification).
    juce::AudioBuffer<float> out (2, buffer.getNumSamples());
    out.copyFrom (0, 0, buffer, 0, 0, buffer.getNumSamples());
    out.copyFrom (1, 0, buffer, 1, 0, buffer.getNumSamples());
    return out;
}

} // namespace

OnnxSeparator::OnnxSeparator (const ModelRuntime& runtime, ModelPackage package)
    : runtime_ (runtime),
      package_ (std::move (package))
{
}

SeparatorCapabilities OnnxSeparator::getCapabilities() const
{
    SeparatorCapabilities caps;
    caps.queryConditioned = false;   // the profile guides stem selection instead
    caps.multiStem = package_.stemNames.size() > 1;

    for (const auto& s : package_.stemNames)
        caps.stems.push_back (s);

    return caps;
}

bool OnnxSeparator::isAvailable() const
{
    juce::String error;
    return ensureReady (error);
}

bool OnnxSeparator::ensureReady (juce::String& errorOut) const
{
    if (readinessChecked_)
    {
        errorOut = readinessError_;
        return ready_;
    }

    readinessChecked_ = true;

    if (! runtime_.isAvailable())
    {
        readinessError_ = "ONNX Runtime is not available (see ModelRuntime::getSearchedPaths)";
        errorOut = readinessError_;
        return false;
    }

    if (! ModelManager::verify (package_, readinessError_))
    {
        errorOut = readinessError_;
        return false;
    }

    const int threads = juce::jmax (1, juce::SystemStats::getNumCpus() - 1);
    session_ = runtime_.createSession (package_.checkpointFile, threads, readinessError_);

    ready_ = session_ != nullptr;
    errorOut = readinessError_;
    return ready_;
}

SeparationResult OnnxSeparator::separate (const juce::AudioBuffer<float>& mixture,
                                          const SeparationRequest& request,
                                          const extraction::ProgressCallback& progress,
                                          const extraction::CancellationToken& cancellation)
{
    SeparationResult result;
    result.modelName = package_.name;
    result.modelVersion = package_.version;

    juce::String readyError;

    if (! ensureReady (readyError))
    {
        result.error = readyError;
        return result;
    }

    if (mixture.getNumSamples() <= 0 || request.sampleRate <= 0.0)
    {
        result.error = "empty mixture";
        return result;
    }

    const double modelRate = chooseModelRate (package_, request.sampleRate);

    // ---- to model rate + stereo ------------------------------------------------
    auto stereo = ensureStereo (mixture);

    if (std::abs (modelRate - request.sampleRate) > 0.5)
        stereo = dsp::Resampler::process (stereo, request.sampleRate, modelRate);

    const int numSamples = stereo.getNumSamples();

    if (numSamples <= 0)
    {
        result.error = "resampling produced no samples";
        return result;
    }

    // ---- chunking parameters ------------------------------------------------------
    const double chunkSeconds = package_.chunkSeconds > 0.5 ? package_.chunkSeconds : 10.0;
    const int chunkFrames = juce::jmax (256, (int) std::llround (chunkSeconds * modelRate));
    const int overlapFrames = juce::jlimit (1, chunkFrames / 2, (int) std::llround (0.25 * chunkFrames));
    const int stepFrames = chunkFrames - overlapFrames;

    int processFrames = numSamples;

    if (request.maxSeconds > 0.0)
        processFrames = juce::jmin (numSamples, (int) std::llround (request.maxSeconds * modelRate));

    if (processFrames <= 0)
    {
        result.error = "nothing to process";
        return result;
    }

    // ---- run ----------------------------------------------------------------------
    const auto inputNames = session_->getInputNames();
    const juce::String inputName = package_.inputName.isNotEmpty()
                                       ? package_.inputName
                                       : (inputNames.isEmpty() ? juce::String() : inputNames[0]);

    std::vector<juce::AudioBuffer<float>> accumulated;
    juce::AudioBuffer<float> weightSum;
    int stemCount = 0;
    bool structureKnown = false;
    int chunkIndex = 0;

    for (int start = 0; start < processFrames; start += stepFrames)
    {
        if (cancellation.isCancelled())
        {
            result.error = "cancelled";
            return result;
        }

        const int frames = juce::jmin (chunkFrames, processFrames - start);

        // Fixed-length inference: exported graphs may bake shape constants
        // from the trace segment, so every call uses exactly chunkFrames
        // samples (the final chunk is zero-padded; the padded region is
        // trimmed below via `usable`).
        ModelRuntime::Tensorf input;
        input.shape = { 1, 2, (std::int64_t) chunkFrames };
        input.data.assign ((size_t) 2 * (size_t) chunkFrames, 0.0f);

        for (int ch = 0; ch < 2; ++ch)
            std::copy_n (stereo.getReadPointer (ch) + start, frames,
                         input.data.data() + (size_t) ch * (size_t) chunkFrames);

        auto runResult = session_->run ({ { inputName, std::move (input) } });

        if (! runResult.success || runResult.outputs.empty())
        {
            result.error = runResult.error.isNotEmpty() ? runResult.error
                                                        : juce::String ("inference returned no output");
            return result;
        }

        const auto& tensor = runResult.outputs.front();
        const int rank = (int) tensor.shape.size();

        int stems = 1;
        int channels = 1;
        int outFrames = 0;

        if (rank == 2)
        {
            channels = (int) tensor.shape[0];
            outFrames = (int) tensor.shape[1];
        }
        else if (rank == 3)
        {
            channels = (int) tensor.shape[1];
            outFrames = (int) tensor.shape[2];
        }
        else if (rank == 4)
        {
            stems = (int) tensor.shape[1];
            channels = (int) tensor.shape[2];
            outFrames = (int) tensor.shape[3];
        }
        else
        {
            result.error = "unsupported model output rank " + juce::String (rank);
            return result;
        }

        const int outChannels = juce::jlimit (1, 2, channels);

        if (tensor.data.size() < (size_t) stems * (size_t) channels * (size_t) outFrames)
        {
            result.error = "model output tensor is smaller than its declared shape";
            return result;
        }

        if (! structureKnown)
        {
            stemCount = stems;
            accumulated.assign ((size_t) stems, {});
            weightSum.setSize (1, processFrames, false, true, false);

            for (auto& buf : accumulated)
                buf.setSize (outChannels, processFrames, false, true, false);

            structureKnown = true;
        }
        else if (stems != stemCount)
        {
            result.error = "model returned an inconsistent stem count between chunks";
            return result;
        }

        const int usable = juce::jmin (outFrames, juce::jmin (chunkFrames, processFrames - start));
        auto* weight = weightSum.getWritePointer (0);

        for (int s = 0; s < stemCount; ++s)
        {
            auto& dst = accumulated[(size_t) s];

            for (int ch = 0; ch < channels && ch < outChannels; ++ch)
            {
                const size_t base = ((size_t) s * (size_t) channels + (size_t) ch) * (size_t) outFrames;
                const float* src = tensor.data.data() + base;
                auto* dstData = dst.getWritePointer (ch);

                for (int i = 0; i < usable; ++i)
                {
                    float w = 1.0f;

                    if (start > 0 && i < overlapFrames)
                        w = (float) i / (float) overlapFrames;

                    if (start + usable < processFrames && i >= usable - overlapFrames)
                        w = juce::jmin (w, (float) (usable - 1 - i) / (float) overlapFrames);

                    dstData[start + i] += src[i] * w;
                    weight[start + i] += w;
                }
            }
        }

        const float frac = (float) juce::jlimit (0.0, 1.0,
                                                 (double) (start + usable) / (double) processFrames);

        if (progress)
            progress ({ extraction::Stage::separating, frac, frac,
                        "chunk " + juce::String (chunkIndex + 1) });

        ++chunkIndex;
    }

    // ---- normalise, build stems ------------------------------------------------------
    std::vector<juce::AudioBuffer<float>> stems;
    stems.reserve ((size_t) stemCount);

    for (int s = 0; s < stemCount; ++s)
    {
        auto& accum = accumulated[(size_t) s];
        const auto* weight = weightSum.getReadPointer (0);

        for (int ch = 0; ch < accum.getNumChannels(); ++ch)
        {
            auto* data = accum.getWritePointer (ch);

            for (int i = 0; i < processFrames; ++i)
                if (weight[i] > 1.0e-6f)
                    data[i] /= weight[i];
        }

        if (accum.getNumChannels() == 1)
            stems.push_back (dsp::ChannelConverter::toStereo (accum));
        else
            stems.push_back (std::move (accum));
    }

    // ---- back to the request sample rate ----------------------------------------------
    auto toRequestRate = [&] (juce::AudioBuffer<float> buffer)
    {
        if (std::abs (modelRate - request.sampleRate) > 0.5)
            buffer = dsp::Resampler::process (buffer, modelRate, request.sampleRate);

        return buffer;
    };

    // ---- choose the target stem ---------------------------------------------------------
    int targetIndex = 0;

    if (stemCount > 1)
    {
        float best = -1.0f;

        for (int s = 0; s < stemCount; ++s)
        {
            const float score = profileScore (stems[(size_t) s], modelRate, request.profile);

            if (score > best)
            {
                best = score;
                targetIndex = s;
            }
        }
    }

    result.audio = toRequestRate (std::move (stems[(size_t) targetIndex]));
    result.sampleRate = request.sampleRate;

    for (int s = 0; s < stemCount; ++s)
    {
        if (s == targetIndex)
            continue;

        const auto name = (size_t) s < package_.stemNames.size()
                              ? package_.stemNames[(size_t) s]
                              : "stem " + juce::String (s + 1);

        result.otherStems.emplace_back (name, toRequestRate (std::move (stems[(size_t) s])));
    }

    result.success = true;
    return result;
}

} // namespace zarlino::source::inference
