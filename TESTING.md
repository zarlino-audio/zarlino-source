# Testing Zarlino Source

## Running the tests

```
tools\build.cmd ZarlinoSource_Tests

build\ZarlinoSource_Tests_artefacts\Release\ZarlinoSource_Tests.exe
```

The runner prints one line per test (`[ RUN ]`, `[ OK ]` / `[ FAIL ]`), a final
`N passed, M failed`, and exits non-zero if anything failed.

## What is covered today

The suite (in `tests/`) currently covers the DSP and audio-IO foundation:

| Test | What it proves |
|---|---|
| `StftIStftRoundTripIsExactInInterior` | STFT → iSTFT reconstructs the input to < 1e-4 error away from the edges (COLA-correct overlap-add). |
| `ResamplerPreservesToneAndLength` | 48k → 96k and 48k → 44.1k conversion keeps length and a 440 Hz tone at correct frequency/amplitude. |
| `ChannelConverterBasics` | Mono/stereo/mid-side conversions are consistent and reversible. |
| `FadeProcessorEndpoints` | Fade-in/out hit exactly 0 and 1 at their endpoints. |
| `ZeroCrossingCountsAndEstimates` | Crossing counts and frequency estimates on known signals; crossing snapping lands within tolerance of the ideal point. |
| `TransientDetectorFindsClickTrain` | Three synthetic onsets are detected, within 20 ms of ground truth. |
| `CaptureBufferPushPopAndWrap` | The lock-free capture ring preserves samples across wraparound pushes/pops. |
| `WavRoundTripAllDepths` | WAV PCM16 / PCM24 / float32 round-trips meet their tolerance budgets. |

## Testing policy (required for anything added from here on)

These follow the project's correctness mandate (spec §8, §26, §66):

1. **Null tests against a golden reference.** Any processing stage that claims
   transparency must null against the identity within its stated tolerance.
2. **Impulse / step / frequency-response checks.** Filters, resamplers and
   STFT configurations are characterised on synthetic signals with known
   ground truth before they touch music.
3. **Determinism.** Same input + same settings = bit-identical output
   (needed for reproducible extraction and reproducible projects).
4. **Adversarial input injection.** NaN/Inf/denormals in the input; silence;
   DC; full-scale square waves; 1-sample buffers; abrupt sample-rate and
   buffer-size changes mid-stream. Nothing may crash or produce NaN on output.
5. **Cancellation checks.** Long operations must stop promptly at chunk
   boundaries when cancellation is requested (spec §45).
6. **No fabricated metrics.** Quality numbers in docs or UI are measured by
   these tests or the Phase 0 benchmark harness, never estimated by hand.

## Extraction-quality benchmarking

Sound-quality metrics (SDR, SI-SDR, SIR, SAR, artifact metrics) live in the
Phase 0 benchmark harness under `benchmarks/` and are reported in
`MODEL_BENCHMARK.md`. Unit tests are for correctness, not for quality claims.

## Conventions

- Test macros: `ZS_TEST(name)`, `ZS_EXPECT(cond)`, `ZS_EXPECT_NEAR(a, b, tol)`
  (`tests/TestRunner.h`). Zero external test dependencies.
- One file per area, named after the area (`DspTests.cpp`, next:
  `ExtractionTests.cpp`, `AnalysisTests.cpp`, ...).
- Tests must be runnable headless (no windows, no audio devices, no network).
