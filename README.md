# Zarlino Source

Zarlino Source turns a **mixed recording plus a description of a sound** into a
clean, reusable musical asset:

```
MIXED RECORDING + "Rhodes"
        ↓
SOURCE IDENTIFICATION  (text query → source representation)
        ↓
SOURCE SEPARATION      (real neural inference, offline)
        ↓
ARTIFACT ANALYSIS + REFINEMENT
        ↓
CLEAN SOURCE → one-shot / phrase / loop / playable instrument
```

- No reference recording is required (and none is accepted in V1).
- Queries are free-form (`Rhodes`, `shaker`, `that synth`, `808`); the system
  does not reduce them to a fixed stem list.
- All heavy processing runs **offline**, never on a DAW real-time thread.

## Status (honest, updated as work progresses)

| Area | State |
|---|---|
| Repository scaffold, CMake, JUCE 8 standalone + VST3 targets | **built** - standalone + VST3 + tests; 22/22 tests green |
| UI shell (Extract / Processing / Result screens) | **implemented** - extraction wired end-to-end on a worker thread with real progress + cancel; playback not yet |
| DSP core (STFT/iSTFT, resampling, transients, zero-crossing) | **implemented + tested** |
| Extraction engine (analyze -> find -> separate -> artifacts -> finalize) | **implemented + tested** (worker thread, progress, cancellation, memory precheck) |
| Text query -> source representation | **implemented + tested** (descriptor lexicon -> TargetProfile; rule-based, not a trained encoder) |
| Artifact analysis | **implemented + tested** (preservation / leakage / transients / burstiness; phase integrity not yet computed and reported as such) |
| Refinement (spec section 28) | **not yet implemented** - diagnostics say so when a pass would be needed |
| Inference runtime (ONNX Runtime) + separator implementations | **implemented** - dynamic ORT 1.22 loading validated against the real DLL; chunked ONNX separator; honest errors when model/runtime is missing |
| Musical analysis (pitch / tempo / key) | **implemented + tested** - uncertainty is explicit (tempo/key reported as uncertain, never invented) |
| Sampling assets | one-shot creation **implemented + tested** (ONE-SHOT is wired in the result screen); phrase / loop / instrument still planned - their buttons are disabled and say so |
| Model checkpoints | first checkpoint (HTDemucs, MIT) being exported for integration - see `MODEL_BENCHMARK.md` / `MODEL_PROVENANCE.md`; no dummy/placeholder output path exists |

No benchmark numbers are published until they are measured — see
`MODEL_BENCHMARK.md` (created during Phase 0) for the evaluation protocol and
recorded results.

## Repository layout

```
zarlino-source/
├── CMakeLists.txt
├── README.md  BUILD.md  TESTING.md
├── MODEL_PROVENANCE.md   DATASET_LICENSES.md
├── apps/standalone/      # standalone-specific resources (entry point is the
│                         # JUCE standalone format of the product target)
├── plugin/               # PluginProcessor / PluginEditor (VST3 + Standalone)
├── core/
│   ├── audio/            # documents, file IO, capture buffers
│   ├── extraction/       # query, engine, candidates, refinement
│   ├── analysis/         # artifacts, pitch, tempo, key, loops
│   ├── sampling/         # one-shot / phrase / loop creation, export
│   ├── instruments/      # builder, sampler engine, MIDI
│   └── project/          # .zsource project format
├── dsp/                  # STFT/iSTFT, resampling, fades, transients, ZC
├── inference/            # ISeparator, ModelRuntime (ONNX), ModelManager
│   └── implementations/  # HTDemucs, BS-RoFormer, Zarlino (fine-tuned slot)
├── models/               # model packages (manifest + checkpoint), see README
├── ui/                   # Theme + screens (Extract / Process / Result / Instrument)
├── tests/                # unit + regression tests
├── benchmarks/           # benchmark fixtures + harness (Phase 0)
└── ml/                   # datasets / training / evaluation / export
```

## Building

See `BUILD.md`. Short version (Windows, from this directory):

```
tools\build.cmd ZarlinoSource_All ZarlinoSource_Tests
```

## Design language

Near-black surfaces, steel secondary panels, white typography, blue accent,
Inter for UI text, IBM Plex Mono for numerics, 4 px radius, 8 px spacing.
No gradients, no "AI magic" styling — this should look like a professional
audio instrument.
