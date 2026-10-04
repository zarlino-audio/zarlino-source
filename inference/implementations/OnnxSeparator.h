#pragma once

#include "inference/ISeparator.h"
#include "inference/ModelManager.h"
#include "inference/ModelRuntime.h"

#include <memory>

namespace zarlino::source::inference
{

/** Runs a validated ONNX model package as a separator (spec section 57).

    Contract (see also models/README.md):
      - input : float32 [1, 2, T] at the model sample rate
      - output: float32 [1, T] | [1, 2, T] (single target) or
                [1, S, 2, T] (S stems -> chosen by TargetProfile scoring)

    The mixture is resampled to the model rate, processed in overlapping
    chunks (25 %), overlap-added with weight normalisation, and resampled
    back. Cancellation is checked between chunks. When the runtime or the
    model is unavailable, separate() fails with an honest error - there is no
    fallback that produces audio. */
class OnnxSeparator final : public ISeparator
{
public:
    OnnxSeparator (const ModelRuntime& runtime, ModelPackage package);

    juce::String getName() const override { return package_.name; }
    juce::String getVersion() const override { return package_.version; }

    SeparatorCapabilities getCapabilities() const override;

    bool isAvailable() const override;

    SeparationResult separate (const juce::AudioBuffer<float>& mixture,
                               const SeparationRequest& request,
                               const extraction::ProgressCallback& progress,
                               const extraction::CancellationToken& cancellation) override;

private:
    /** Runtime check + model verification + session creation, once. */
    bool ensureReady (juce::String& errorOut) const;

    const ModelRuntime& runtime_;
    ModelPackage package_;

    mutable bool readinessChecked_ = false;
    mutable bool ready_ = false;
    mutable juce::String readinessError_;
    mutable std::shared_ptr<ModelRuntime::Session> session_;
};

} // namespace zarlino::source::inference
