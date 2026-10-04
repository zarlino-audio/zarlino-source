#include "inference/ModelRuntime.h"

#include <onnxruntime_c_api.h>

#include <algorithm>

namespace zarlino::source::inference
{

namespace
{

juce::String statusToString (const OrtApi* api, OrtStatus* status)
{
    if (status == nullptr)
        return {};

    // The message string belongs to the status object - copy before releasing.
    const juce::String text (api->GetErrorMessage (status));
    api->ReleaseStatus (status);
    return text;
}

juce::StringArray candidatePaths()
{
    juce::StringArray paths;

    const auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                            .getParentDirectory();

    paths.add (exeDir.getChildFile ("onnxruntime.dll").getFullPathName());
    paths.add (exeDir.getChildFile ("runtime").getChildFile ("onnxruntime.dll").getFullPathName());

    const auto envDir = juce::SystemStats::getEnvironmentVariable ("ONNXRUNTIME_DLL_DIR", {});

    if (envDir.isNotEmpty())
        paths.add (juce::File (envDir).getChildFile ("onnxruntime.dll").getFullPathName());

    return paths;
}

OrtValue* createTensor (const OrtApi* api, OrtMemoryInfo* memoryInfo,
                        const ModelRuntime::Tensorf& tensor, juce::String& error)
{
    if (tensor.shape.empty() || tensor.data.empty())
    {
        error = "empty tensor";
        return nullptr;
    }

    OrtValue* value = nullptr;

    auto* status = api->CreateTensorWithDataAsOrtValue (
        memoryInfo,
        const_cast<float*> (tensor.data.data()),
        tensor.data.size() * sizeof (float),
        tensor.shape.data(),
        tensor.shape.size(),
        ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
        &value);

    if (status != nullptr)
    {
        error = "could not create input tensor: " + statusToString (api, status);
        return nullptr;
    }

    return value;
}

bool readTensor (const OrtApi* api, OrtValue* value, ModelRuntime::Tensorf& out, juce::String& error)
{
    OrtTensorTypeAndShapeInfo* info = nullptr;

    if (auto* status = api->GetTensorTypeAndShape (value, &info); status != nullptr)
    {
        error = "could not read output tensor: " + statusToString (api, status);
        return false;
    }

    ONNXTensorElementDataType elementType = ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED;
    api->GetTensorElementType (info, &elementType);

    if (elementType != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT)
    {
        error = "output tensor is not float32";
        api->ReleaseTensorTypeAndShapeInfo (info);
        return false;
    }

    size_t rank = 0;
    api->GetDimensionsCount (info, &rank);

    out.shape.assign (rank, 0);
    api->GetDimensions (info, out.shape.data(), rank);

    size_t count = 0;
    api->GetTensorShapeElementCount (info, &count);
    api->ReleaseTensorTypeAndShapeInfo (info);

    void* data = nullptr;

    if (auto* status = api->GetTensorMutableData (value, &data); status != nullptr)
    {
        error = "could not read output data: " + statusToString (api, status);
        return false;
    }

    const auto* floats = static_cast<const float*> (data);
    out.data.assign (floats, floats + count);
    return true;
}

} // namespace

//==============================================================================

struct ModelRuntime::Impl
{
    juce::DynamicLibrary library;
    const OrtApi* api = nullptr;
    OrtEnv* env = nullptr;
    juce::String version;

    ~Impl()
    {
        if (api != nullptr && env != nullptr)
            api->ReleaseEnv (env);
    }
};

struct ModelRuntime::Session::Impl
{
    const OrtApi* api = nullptr;
    OrtSession* session = nullptr;
    std::shared_ptr<ModelRuntime::Impl> runtime;   // keeps the runtime alive

    ~Impl()
    {
        if (api != nullptr && session != nullptr)
            api->ReleaseSession (session);
    }
};

ModelRuntime::ModelRuntime()
{
    for (const auto& path : candidatePaths())
    {
        auto impl = std::make_shared<Impl>();

        if (! impl->library.open (path))
            continue;

        auto* getApiBase = impl->library.getFunction<const OrtApiBase*()> ("OrtGetApiBase");

        if (getApiBase == nullptr)
            continue;

        const auto* base = getApiBase();
        impl->api = base->GetApi (ORT_API_VERSION);

        if (impl->api == nullptr)
            continue;   // runtime present but too old for this header

        if (auto* status = impl->api->CreateEnv (ORT_LOGGING_LEVEL_WARNING, "zarlino-source",
                                                 &impl->env);
            status != nullptr)
        {
            impl->api->ReleaseStatus (status);
            impl->env = nullptr;
            continue;
        }

        impl->version = juce::String (base->GetVersionString());
        impl_ = std::move (impl);
        break;
    }
}

ModelRuntime::~ModelRuntime() = default;

juce::String ModelRuntime::getVersion() const
{
    return impl_ != nullptr ? impl_->version : juce::String();
}

juce::StringArray ModelRuntime::getSearchedPaths()
{
    return candidatePaths();
}

std::shared_ptr<ModelRuntime::Session> ModelRuntime::createSession (const juce::File& modelFile,
                                                                    int numThreads,
                                                                    juce::String& errorOut) const
{
    errorOut.clear();

    if (impl_ == nullptr)
    {
        errorOut = "ONNX Runtime is not available. Searched: "
                 + getSearchedPaths().joinIntoString ("; ")
                 + ". Place onnxruntime.dll beside the executable or set ONNXRUNTIME_DLL_DIR.";

        return nullptr;
    }

    if (! modelFile.existsAsFile())
    {
        errorOut = "model file missing: " + modelFile.getFullPathName();
        return nullptr;
    }

    OrtSessionOptions* options = nullptr;

    if (auto* status = impl_->api->CreateSessionOptions (&options); status != nullptr)
    {
        errorOut = statusToString (impl_->api, status);
        return nullptr;
    }

    impl_->api->SetIntraOpNumThreads (options, juce::jmax (1, numThreads));
    impl_->api->SetSessionGraphOptimizationLevel (options, ORT_ENABLE_ALL);

    OrtSession* session = nullptr;

    auto* status = impl_->api->CreateSession (impl_->env,
                                              modelFile.getFullPathName().toWideCharPointer(),
                                              options,
                                              &session);

    impl_->api->ReleaseSessionOptions (options);

    if (status != nullptr)
    {
        errorOut = "could not load model: " + statusToString (impl_->api, status);
        return nullptr;
    }

    auto result = std::shared_ptr<Session> (new Session());
    result->impl_ = std::make_unique<Session::Impl>();
    result->impl_->api = impl_->api;
    result->impl_->session = session;
    result->impl_->runtime = impl_;

    return result;
}

//==============================================================================

ModelRuntime::Session::Session() = default;
ModelRuntime::Session::~Session() = default;

bool ModelRuntime::Session::isValid() const noexcept
{
    return impl_ != nullptr && impl_->session != nullptr;
}

juce::StringArray ModelRuntime::Session::getInputNames() const
{
    juce::StringArray names;

    if (! isValid())
        return names;

    OrtAllocator* allocator = nullptr;

    if (impl_->api->GetAllocatorWithDefaultOptions (&allocator) != nullptr)
        return names;

    size_t count = 0;

    if (impl_->api->SessionGetInputCount (impl_->session, &count) != nullptr)
        return names;

    for (size_t i = 0; i < count; ++i)
    {
        char* name = nullptr;

        if (impl_->api->SessionGetInputName (impl_->session, i, allocator, &name) == nullptr
            && name != nullptr)
        {
            names.add (juce::String (name));
            allocator->Free (allocator, name);
        }
    }

    return names;
}

juce::StringArray ModelRuntime::Session::getOutputNames() const
{
    juce::StringArray names;

    if (! isValid())
        return names;

    OrtAllocator* allocator = nullptr;

    if (impl_->api->GetAllocatorWithDefaultOptions (&allocator) != nullptr)
        return names;

    size_t count = 0;

    if (impl_->api->SessionGetOutputCount (impl_->session, &count) != nullptr)
        return names;

    for (size_t i = 0; i < count; ++i)
    {
        char* name = nullptr;

        if (impl_->api->SessionGetOutputName (impl_->session, i, allocator, &name) == nullptr
            && name != nullptr)
        {
            names.add (juce::String (name));
            allocator->Free (allocator, name);
        }
    }

    return names;
}

ModelRuntime::RunResult ModelRuntime::Session::run (
    const std::vector<std::pair<juce::String, Tensorf>>& inputs) const
{
    RunResult result;

    if (! isValid())
    {
        result.error = "session is not valid";
        return result;
    }

    const auto* api = impl_->api;

    OrtMemoryInfo* memoryInfo = nullptr;

    if (auto* status = api->CreateCpuMemoryInfo (OrtArenaAllocator, OrtMemTypeDefault, &memoryInfo);
        status != nullptr)
    {
        result.error = "could not create CPU memory info: " + statusToString (api, status);
        return result;
    }

    std::vector<OrtValue*> inputValues;
    std::vector<juce::String> inputNames;

    for (const auto& [name, tensor] : inputs)
    {
        juce::String tensorError;
        auto* value = createTensor (api, memoryInfo, tensor, tensorError);

        if (value == nullptr)
        {
            result.error = tensorError;
            break;
        }

        inputValues.push_back (value);
        inputNames.push_back (name);
    }

    if (result.error.isEmpty())
    {
        const bool useNames = std::all_of (inputNames.begin(), inputNames.end(),
                                           [] (const juce::String& s) { return s.isNotEmpty(); });

        std::vector<const char*> inputNamePtrs;

        if (useNames)
            for (const auto& name : inputNames)
                inputNamePtrs.push_back (name.toRawUTF8());

        OrtAllocator* allocator = nullptr;
        api->GetAllocatorWithDefaultOptions (&allocator);

        size_t outputCount = 0;
        api->SessionGetOutputCount (impl_->session, &outputCount);

        if (outputCount == 0)
        {
            result.error = "model reports no outputs";
        }
        else
        {
            std::vector<juce::String> outputNames;

            for (size_t i = 0; i < outputCount; ++i)
            {
                char* name = nullptr;

                if (api->SessionGetOutputName (impl_->session, i, allocator, &name) == nullptr
                    && name != nullptr)
                {
                    outputNames.emplace_back (name);
                    allocator->Free (allocator, name);
                }
                else
                {
                    outputNames.emplace_back();
                }
            }

            std::vector<const char*> outputNamePtrs;

            for (const auto& name : outputNames)
                outputNamePtrs.push_back (name.isNotEmpty() ? name.toRawUTF8() : nullptr);

            std::vector<OrtValue*> outputs (outputCount, nullptr);

            auto* status = api->Run (impl_->session, nullptr,
                                     useNames ? inputNamePtrs.data() : nullptr,
                                     inputValues.data(), inputValues.size(),
                                     outputNamePtrs.data(), outputNamePtrs.size(),
                                     outputs.data());

            if (status != nullptr)
            {
                result.error = "inference failed: " + statusToString (api, status);
            }
            else
            {
                bool ok = true;

                for (size_t i = 0; i < outputCount && ok; ++i)
                {
                    Tensorf tensor;
                    juce::String tensorError;

                    if (outputs[i] != nullptr && readTensor (api, outputs[i], tensor, tensorError))
                        result.outputs.push_back (std::move (tensor));
                    else
                    {
                        result.error = tensorError.isNotEmpty() ? tensorError
                                                                : juce::String ("missing output tensor");
                        ok = false;
                    }
                }

                result.success = ok;
            }

            for (auto* value : outputs)
                if (value != nullptr)
                    api->ReleaseValue (value);
        }
    }

    for (auto* value : inputValues)
        api->ReleaseValue (value);

    api->ReleaseMemoryInfo (memoryInfo);
    return result;
}

} // namespace zarlino::source::inference
