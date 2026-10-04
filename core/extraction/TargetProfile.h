#pragma once

#include <juce_core/juce_core.h>

namespace zarlino::source::extraction
{

/** Soft perceptual/physical profile describing the requested source.

    This is the bridge between the text query and the audio. Each dimension is
    a continuous target (not a class label), so "Rhodes" produces something
    like (warm mid-range, medium attack, long sustained harmonics, often wide)
    while "shaker" produces (high-band noise, sharp attack, no sustain).
    Candidate analysis and refinement score mixture content against this
    profile; nothing here is a fixed stem enumeration.
*/
struct TargetProfile
{
    // Spectral emphasis (relative, >= 0; 1.0 = neutral).
    float lowBandWeight = 1.0f;         // < ~250 Hz
    float midBandWeight = 1.0f;         // ~250 Hz .. 2 kHz
    float highBandWeight = 1.0f;        // > ~2 kHz

    float targetCentroidHz = 0.0f;      // 0 = no preference
    float centroidTolerance = 0.5f;     // fractional tolerance when set

    // Temporal character.
    float attackSharpness = 0.5f;       // 0 = slow swell, 1 = click transient
    float sustainRatio = 0.5f;          // 0 = one-shot decay, 1 = continuous
    float rhythmicRate = 0.0f;          // expected events/sec, 0 = agnostic

    // Harmonic character.
    float harmonicity = 0.5f;           // 0 = noise-like, 1 = strongly pitched
    float pitchHzHint = 0.0f;           // 0 = unknown (e.g. 808 ~ 40-60 Hz)
    float pitchToleranceOctaves = 1.0f; // accepted range around the hint

    // Spatial character.
    float stereoSpread = 0.5f;          // 0 = centre, 1 = wide

    // Confidence of the language interpretation itself (drives UI messaging
    // and prevents over-confident extraction of unknown words).
    float interpretationConfidence = 0.5f;

    // Short human-readable summary of the interpretation (diagnostics/UI).
    juce::String summary;
};

} // namespace zarlino::source::extraction
