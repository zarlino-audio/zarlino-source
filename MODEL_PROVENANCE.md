# Model Provenance — Zarlino Source

This file records **every model** that ships with, or is downloadable by,
Zarlino Source. Per spec §53 no model may be bundled into a commercial build
until its redistribution rights have been verified. Nothing in this file may
be guessed — every field must come from the model's repository/licence text.

## Required fields per model

```
model name
repository
checkpoint
version
license
weights license
training-data information
commercial-use status
redistribution status
modifications
```

## Current status

**Development package present: `models/htdemucs/`.** The extraction pipeline
performs real ONNX inference through this package (spec §57 - there is
deliberately no dummy/placeholder separation path). The package's sha256 is
verified by `ModelManager` against `models/htdemucs/manifest.json` at load
(see `ml/export/` for the full export/repair/validation toolchain and the
numerical parity gate).

**Redistribution gate (spec §53):** the weights' commercial-redistribution
status is *pending review* (registry row 1). Development and internal
testing may use the package; do not bundle it into a commercial build until
the review is recorded here.

## Candidates under audit (Phase 0 - nothing bundled yet)

Facts read from repositories on 2026-09-29; see MODEL_BENCHMARK.md section 1
for sources. Fields not yet read are marked TO VERIFY.

| # | Model | Repository | Licence (verified) | Weights status | Notes |
|---|---|---|---|---|---|
| 1 | HTDemucs (Demucs v4) | facebookresearch/demucs | MIT — LICENSE file read 2026-10-02 | Weights published by the original author via HF `adefossez/HTDemucs`; no separate weights licence file | Archived; fork adefossez/demucs; 4 stems (+6s variant); MUSDB HQ + 800 songs. Integrated: `models/htdemucs/` (registry row 1) |
| 2 | BS-RoFormer / Mel-Band RoFormer | lucidrains architectures; ZFTurbo MSST training | code MIT (MSST repo); per-checkpoint TO VERIFY | TO VERIFY per checkpoint | export via MSS_ONNX_TensorRT |
| 3 | MDX23C and others | ZFTurbo/Music-Source-Separation-Training | MIT (repo) | TO VERIFY per checkpoint | large zoo: bandit, scnet, apollo, dttnet, bs_mamba2 |
| 4 | Open-Unmix | sigsep/open-unmix-pytorch | TO VERIFY | TO VERIFY | baseline reference |
| 5 | Spleeter | deezer/spleeter | TO VERIFY | TO VERIFY | baseline reference |

## Model registry

_(the bundled/redistributed model registry - empty until licence clearance and
the first benchmark run are complete; do not add an entry until the licence
text has been read and the redistribution/commercial-use status recorded)_

| # | Model | Repository | Checkpoint | Licence | Weights licence | Commercial use | Redistribution | Modifications |
|---|---|---|---|---|---|---|---|---|
| 1 | HTDemucs (Demucs v4) | facebookresearch/demucs (fork of adefossez/demucs), v4.1.0 | HF `adefossez/HTDemucs` file `955717e8.safetensors` (84 MB, fp16) -> `models/htdemucs/model.onnx` (fp32, 171 MB, sha256 in manifest.json) | MIT (LICENSE read 2026-10-02) | Distributed by the original author as part of the MIT-licensed project; no separate weights licence file published | Permitted under the MIT code licence; weights redistribution review PENDING (see Current status) | PENDING review - dev/test only until cleared | ONNX export via dynamo exporter (static 7.8 s segment): BagOfModels weighted-average wrapper, complex-pad shim, int32->int64 Cast repair, parity-validated by `ml/export/validate_onnx.py` |

## Runtime note

The native inference runtime is **ONNX Runtime 1.22.0** (MIT licence),
loaded dynamically at runtime - not linked into the binary. The vendored
header lives in `third_party/onnxruntime/include` and the DLL ships beside
the executables (or via `ONNXRUNTIME_DLL_DIR`).
