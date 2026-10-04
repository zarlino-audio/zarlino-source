# Model Benchmark - Zarlino Source (Phase 0)

Status: **in progress.** This document records the candidate audit and the
measurement protocol. No quality numbers are published here until the harness
has actually measured them - fabricated scores are forbidden (spec sections 8,
56, 66).

## 1. Candidate audit

Facts below were read from the repositories' own documentation on 2026-09-29.
Anything not yet read is marked TO VERIFY.

| Candidate | Repository | Code licence (verified) | Stems | Training data (as stated) | Notes |
|---|---|---|---|---|---|
| HTDemucs (Demucs v4) | `facebookresearch/demucs` | **MIT** - README: "Demucs is released under the MIT license as found in the LICENSE file." | 4: drums, bass, other, vocals; `htdemucs_6s` adds guitar + piano (authors: piano "not working great") | MUSDB HQ + 800 extra songs | Archived Jan 2025; active fork `adefossez/demucs`. 44.1 kHz stereo output. HT models: max segment 7.8 s. `--two-stems` mode. Faster/smaller quantized variants (`mdx_q`). |
| BS-RoFormer | `lucidrains/BS-RoFormer` (architecture) | TO VERIFY (repo) | task-dependent (per checkpoint) | per checkpoint | Widely trained via ZFTurbo MSST; checkpoint licences vary per author - verify each. |
| Mel-Band RoFormer | `lucidrains` architecture family | TO VERIFY (repo) | per checkpoint | per checkpoint | Same caveat as BS-RoFormer. |
| MDX23C | via `ZFTurbo/Music-Source-Separation-Training` | **MIT** (repository, GitHub licence metadata; ZFTurbo MSST README) | per checkpoint | per checkpoint (MUSDB18 examples) | MSST also covers htdemucs, bandit, scnet, apollo, dttnet, bs_mamba2. |
| Open-Unmix | `sigsep/open-unmix-pytorch` | TO VERIFY | 4 | MUSDB18 | Baseline reference. |
| Spleeter | `deezer/spleeter` | TO VERIFY (code); weights TO VERIFY | 2/4/5 | internal Deezer + MUSDB-related | Baseline reference. |

**Known export path:** ZFTurbo's `MSS_ONNX_TensorRT` converts MSST models to
ONNX/TensorRT. Demucs itself has no official ONNX export; an export script will
live in `ml/export/` once a checkpoint is chosen.

**Open licence questions to close before any checkpoint is bundled:**

1. Demucs: confirm the LICENSE text and whether the downloadable checkpoints
   are covered by it (README states the project is MIT).
2. Per-checkpoint licences for RoFormer-family checkpoints (varies by author).
3. MUSDB18(-HQ): research licence; used at most for *internal evaluation*,
   never redistributed. Verify terms before evaluation use; prefer
   permissively licensed evaluation material where possible.

## 2. Evaluation protocol (spec section 8)

### 2.1 Test sources

vocals, kick, snare, hi-hat, percussion, bass, electric guitar, acoustic
guitar, piano, Rhodes, Wurlitzer, organ, synth, strings, brass, FX.

### 2.2 Ground-truth metrics (when isolated references exist)

- SDR, SI-SDR, SIR, SAR (standard BSS-Eval formulation)
- spectral error (log-mel L1 between target and reference)
- transient preservation (onset match rate at 25 ms tolerance - same definition
  as the product's internal metric)

### 2.3 Real-world material (mixed recordings, no ground truth)

Objective artifact diagnostics (the product's ArtifactAnalyzer metrics:
preservation/leakage/burstiness/onset-match) plus a controlled expert
evaluation with a written score sheet. SDR is never reported without ground
truth.

### 2.4 Reproducibility

- Fixed segmentation/chunking and overlap settings per model.
- Sample-rate policy recorded per run; inputs resampled with the project
  resampler (windowed sinc).
- Model file SHA-256 and version recorded with every result row.
- Results stored as JSON under `benchmarks/results/` and summarised here.

## 3. Harness plan

- `benchmarks/` - fixture generation (synthetic mixtures from isolated
  sources), runner, and result JSON.
- `ml/evaluation/` - Python-side ground-truth metrics (BSS-Eval) for the
  development workflow.
- `ml/export/` - PyTorch checkpoint -> ONNX export with an explicit IO spec
  recorded into the model `manifest.json` (input/output names, chunk seconds,
  sample rates, stems).

## 4. Selection gates

1. **Licence clearance** - `MODEL_PROVENANCE.md` entry complete for the exact
   checkpoint (no bundling before this passes).
2. **Coverage** - fixed-stem models cover drums/bass/vocals/other; keys and
   other targets are reached through the hierarchical path (spec sections
   16/17). The engine's query -> profile -> candidate machinery decides what
   to do when a query does not map onto a native stem (no silent 4-stem
   substitution).
3. **Measured quality** - thresholds will be set from the first measurements
   (no invented numbers); artifact scores must not degrade.
4. **Runtime feasibility** - ONNX CPU realtime factor measured; preview engine
   targets < 10 GFLOPs/s where achievable (spec section 22).

## 5. Status checklist

- [x] Candidate list assembled; initial licence facts recorded (this document)
- [ ] Close open licence questions (section 1)
- [ ] Download one permitted checkpoint (first candidate: HTDemucs)
- [ ] Export to ONNX; record IO spec into a model manifest
- [ ] Build benchmark fixtures; run the harness; record results
- [ ] Decide: integrate as-is, or fine-tune (spec sections 9/10); document
      the decision with evidence
