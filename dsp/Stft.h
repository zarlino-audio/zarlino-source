#pragma once

#include <juce_dsp/juce_dsp.h>

#include <cmath>
#include <functional>
#include <memory>
#include <vector>

namespace zarlino::source::dsp
{

/** STFT configuration.

    Defaults follow the specification: FFT 2048, hop 512, (periodic) Hann
    window. Model-specific configurations are supported by simply changing
    these values - all consumers read the configuration at runtime and never
    assume 48 kHz.
*/
struct StftConfig
{
    enum class WindowType
    {
        rectangular,
        hann,
        hannSqrt          // sqrt(hann): analysis + synthesis pair for spectral edits
    };

    int fftOrder = 11;                       // 2^11 = 2048
    int hopSize = 512;
    WindowType window = WindowType::hann;

    int getFftSize() const noexcept { return 1 << fftOrder; }

    bool isValid() const noexcept
    {
        return fftOrder >= 5 && fftOrder <= 16
            && hopSize > 0 && hopSize <= (1 << fftOrder);
    }
};

/** Builds a periodic window of the given type and size. */
std::vector<float> makeStftWindow (StftConfig::WindowType type, int size);

/** Streaming short-time Fourier transform.

    Frames are produced on demand from a sample buffer, zero-padded past the
    end of the buffer. The complex bins are delivered in JUCE's packed
    real-FFT layout: bin k = { data[2*k], data[2*k+1] } for k = 0 .. fftSize/2
    (inclusive), i.e. numBins = fftSize/2 + 1. Every frame is windowed with the
    configured window.

    Not thread-safe. prepare() allocates; computeFrame() does not.
*/
class Stft
{
public:
    Stft() = default;

    /** Allocates. May be called repeatedly. */
    void prepare (const StftConfig& config, double sampleRate);

    const StftConfig& getConfig() const noexcept { return config_; }
    double getSampleRate() const noexcept { return sampleRate_; }
    int getFftSize() const noexcept { return fftSize_; }
    int getHopSize() const noexcept { return config_.hopSize; }
    int getNumBins() const noexcept { return numBins_; }
    const std::vector<float>& getWindow() const noexcept { return window_; }

    /** Number of frames needed to cover numSamples samples (ceil division).
        The final frames may extend past the end of the buffer (zero samples). */
    int getNumFrames (int numSamples) const noexcept;

    /** First sample covered by a frame. */
    int getFrameStartSample (int frameIndex) const noexcept { return frameIndex * getHopSize(); }

    /** Computes one frame; returns a pointer to 2*numBins interleaved (re, im)
        values. The pointer is valid until the next call to computeFrame(). */
    const float* computeFrame (const float* samples, int numSamples, int frameIndex) const;

    /** Convenience: visits every frame of the buffer. The callback receives the
        frame index and the packed complex bins (2*numBins values). */
    void forEachFrame (const float* samples, int numSamples,
                       const std::function<void (int frameIndex, const float* complexBins)>& fn) const;

    /** Magnitude of a packed complex bin. */
    static float magnitudeOf (const float* packed, int bin) noexcept
    {
        const float re = packed[2 * bin];
        const float im = packed[2 * bin + 1];
        return std::sqrt (re * re + im * im);
    }

    /** Power (magnitude squared) of a packed complex bin. */
    static float powerOf (const float* packed, int bin) noexcept
    {
        const float re = packed[2 * bin];
        const float im = packed[2 * bin + 1];
        return re * re + im * im;
    }

private:
    StftConfig config_ {};
    double sampleRate_ = 0.0;
    int fftSize_ = 0;
    int numBins_ = 0;

    std::unique_ptr<juce::dsp::FFT> fft_;
    std::vector<float> window_;
    mutable std::vector<float> work_;        // 2 * fftSize floats

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Stft)
};

} // namespace zarlino::source::dsp
