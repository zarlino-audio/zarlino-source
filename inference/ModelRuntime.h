#pragma once

#include <juce_core/juce_core.h>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace zarlino::source::inference
{

/** Dynamically loaded ONNX Runtime wrapper (spec section 23).

    Nothing is linked at build time: the DLL is located at runtime (see
    getSearchedPaths()). CPU inference with configurable thread count; CUDA is
    not required. When the runtime is missing, everything degrades to honest
    errors - the product never fabricates inference.

    Sessions must not outlive the ModelRuntime that created them (enforced by
    shared ownership internally). All calls run on worker threads. */
class ModelRuntime
{
public:
    /** A dense float32 tensor. */
    struct Tensorf
    {
        std::vector<std::int64_t> shape;
        std::vector<float> data;
    };

    struct RunResult
    {
        bool success = false;
        juce::String error;
        std::vector<Tensorf> outputs;    // in the session's output order
    };

    class Session;

    ModelRuntime();
    ~ModelRuntime();

    bool isAvailable() const noexcept { return impl_ != nullptr; }

    /** Version string reported by the loaded runtime ("" when unavailable). */
    juce::String getVersion() const;

    /** DLL search locations, in order (documented; shown in diagnostics). */
    static juce::StringArray getSearchedPaths();

    /** Creates a session over a validated model file. Returns nullptr and
        fills errorOut on failure. numThreads <= 0 picks a default. */
    std::shared_ptr<Session> createSession (const juce::File& modelFile,
                                            int numThreads,
                                            juce::String& errorOut) const;

    /** A loaded inference session. */
    class Session
    {
    public:
        ~Session();

        bool isValid() const noexcept;

        juce::StringArray getInputNames() const;
        juce::StringArray getOutputNames() const;

        /** Runs the session. Inputs are matched to session inputs by name;
            pass an empty name to use positional matching. */
        RunResult run (const std::vector<std::pair<juce::String, Tensorf>>& inputs) const;

    private:
        friend class ModelRuntime;
        Session();

        struct Impl;
        std::unique_ptr<Impl> impl_;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Session)
    };

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModelRuntime)
};

} // namespace zarlino::source::inference
