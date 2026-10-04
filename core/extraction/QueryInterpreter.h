#pragma once

#include "core/extraction/TargetProfile.h"

#include <juce_core/juce_core.h>

#include <vector>

namespace zarlino::source::extraction
{

/** Result of interpreting one free-form description. */
struct Interpretation
{
    TargetProfile profile;

    /** Lexicon terms that were recognised in the description (diagnostics/UI). */
    std::vector<juce::String> matchedTerms;

    /** True when at least one term was recognised with reasonable strength. */
    bool recognised = false;
};

/** Free-form text -> TargetProfile (spec sections 4, 5, 58).

    This is deliberately NOT a name -> stem lookup. The lexicon maps words and
    phrases to soft *acoustic* leanings (band weights, attack, sustain,
    harmonicity, pitch hints, stereo spread); the result is a continuous
    representation that guides candidate analysis and extraction. Unknown
    words lower the confidence instead of being forced onto a fixed stem list,
    and multi-word phrases ("electric guitar") override the single words they
    contain.

    The interpreter is rule-based - there is no trained text encoder in V1 -
    and reports that honestly through TargetProfile::interpretationConfidence
    and TargetProfile::summary. */
class QueryInterpreter
{
public:
    /** One lexicon entry: a word/phrase with soft acoustic leanings.
        All value fields use -1 to mean "unspecified". Band weights are 1.0
        for neutral, > 1.0 for emphasis. */
    struct Descriptor
    {
        juce::String text;
        bool multiWord = false;
        float weight = 1.0f;

        float lowBand = -1.0f;              // < ~250 Hz
        float midBand = -1.0f;              // ~250 Hz .. 2 kHz
        float highBand = -1.0f;             // > ~2 kHz
        float centroidHz = -1.0f;
        float centroidTolerance = -1.0f;
        float attackSharpness = -1.0f;
        float sustainRatio = -1.0f;
        float rhythmicRate = -1.0f;         // events/sec hint
        float harmonicity = -1.0f;
        float pitchHzHint = -1.0f;
        float pitchToleranceOctaves = -1.0f;
        float stereoSpread = -1.0f;
    };

    QueryInterpreter();

    Interpretation interpret (const juce::String& description) const;

    const std::vector<Descriptor>& getLexicon() const noexcept { return descriptors_; }

private:
    std::vector<Descriptor> descriptors_;
};

} // namespace zarlino::source::extraction
