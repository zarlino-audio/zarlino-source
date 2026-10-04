#include "TestRunner.h"

#include "core/extraction/QueryInterpreter.h"

#include <algorithm>

namespace
{

bool hasTerm (const zarlino::source::extraction::Interpretation& r, const juce::String& term)
{
    return std::find (r.matchedTerms.begin(), r.matchedTerms.end(), term) != r.matchedTerms.end();
}

} // namespace

ZS_TEST (QueryInterpreterMapsRhodesToProfile)
{
    namespace ex = zarlino::source::extraction;

    const ex::QueryInterpreter interpreter;
    const auto r = interpreter.interpret ("Rhodes");

    ZS_EXPECT (r.recognised);
    ZS_EXPECT (hasTerm (r, "rhodes"));
    ZS_EXPECT (r.profile.midBandWeight > r.profile.highBandWeight);
    ZS_EXPECT (r.profile.midBandWeight > r.profile.lowBandWeight);
    ZS_EXPECT (r.profile.midBandWeight > 1.0f);
    ZS_EXPECT (r.profile.harmonicity > 0.6f);
    ZS_EXPECT (r.profile.sustainRatio > 0.5f);
    ZS_EXPECT (r.profile.interpretationConfidence > 0.3f);
    ZS_EXPECT (r.profile.summary.isNotEmpty());
    ZS_EXPECT (r.profile.summary.containsIgnoreCase ("mid"));
}

ZS_TEST (QueryInterpreterPrefersPhrasesAndIgnoresFillerWords)
{
    namespace ex = zarlino::source::extraction;

    const ex::QueryInterpreter interpreter;

    // "the electric guitar" must resolve through the two-word phrase, not by
    // treating "electric" and "guitar" as separate (weaker) hints.
    const auto r = interpreter.interpret ("the electric guitar");

    ZS_EXPECT (r.recognised);
    ZS_EXPECT (hasTerm (r, "electric guitar"));
    ZS_EXPECT (r.profile.midBandWeight > 1.0f);

    // Filler words around a real term must not break recognition.
    const auto thatSynth = interpreter.interpret ("that synth");
    ZS_EXPECT (thatSynth.recognised);
    ZS_EXPECT (hasTerm (thatSynth, "synth"));

    // A stop-word-only description recognises nothing (and must not invent).
    const auto filler = interpreter.interpret ("the that some");
    ZS_EXPECT (! filler.recognised);
}

ZS_TEST (QueryInterpreterSeparatesNoiseAndTonalTargets)
{
    namespace ex = zarlino::source::extraction;

    const ex::QueryInterpreter interpreter;

    const auto shaker = interpreter.interpret ("shaker");
    ZS_EXPECT (shaker.recognised);
    ZS_EXPECT (shaker.profile.highBandWeight > shaker.profile.lowBandWeight);
    ZS_EXPECT (shaker.profile.attackSharpness > 0.6f);
    ZS_EXPECT (shaker.profile.harmonicity < 0.4f);

    const auto pad = interpreter.interpret ("synth pad");
    ZS_EXPECT (pad.recognised);
    ZS_EXPECT (hasTerm (pad, "synth pad"));
    ZS_EXPECT (pad.profile.sustainRatio > 0.7f);
    ZS_EXPECT (pad.profile.attackSharpness < 0.4f);
    ZS_EXPECT (pad.profile.harmonicity > shaker.profile.harmonicity);

    const auto bg = interpreter.interpret ("background vocals");
    ZS_EXPECT (bg.recognised);
    ZS_EXPECT (bg.profile.stereoSpread > 0.55f);
}

ZS_TEST (QueryInterpreterHandlesUnknownWordsHonestly)
{
    namespace ex = zarlino::source::extraction;

    const ex::QueryInterpreter interpreter;

    // Nonsense: nothing recognised, low confidence, no fabricated profile.
    const auto nonsense = interpreter.interpret ("qwertyuiop");
    ZS_EXPECT (! nonsense.recognised);
    ZS_EXPECT (nonsense.matchedTerms.empty());
    ZS_EXPECT (nonsense.profile.interpretationConfidence < 0.3f);

    // Half-known input: recognised, but less confident than the pure term.
    const auto mixed = interpreter.interpret ("qwertyuiop shaker");
    const auto pure = interpreter.interpret ("shaker");

    ZS_EXPECT (mixed.recognised);
    ZS_EXPECT (mixed.profile.interpretationConfidence < pure.profile.interpretationConfidence);

    // Plural fallback.
    const auto plurals = interpreter.interpret ("wurlitzers");
    ZS_EXPECT (plurals.recognised);
    ZS_EXPECT (hasTerm (plurals, "wurlitzer"));
}
