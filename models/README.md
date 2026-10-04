# Models

Model packages live here. **No model ships with the repository yet** — see
`../MODEL_PROVENANCE.md` for the candidate audit and licence status. Extraction
performs real inference only; until a valid package exists below, extraction
reports "no model available" rather than producing placeholder audio.

## Package layout

```
models/
└── <package-id>/
    ├── manifest.json
    └── model.onnx          (name configurable via "checkpoint")
```

## manifest.json

```json
{
  "name": "source-extractor",
  "version": "1.0.0",
  "model_format": "onnx",
  "sample_rates": [44100, 48000],
  "channels": 2,
  "sha256": "<64 hex digits of the checkpoint file>",
  "license": "<licence identifier, e.g. MIT>",
  "license_url": "<link to the licence text>",

  "checkpoint": "model.onnx",
  "input_name": "mixture",
  "output_name": "target",
  "chunk_seconds": 10.0,
  "stem_names": ["drums", "bass", "other", "vocals"]
}
```

Required: `name`, `version`, `model_format`, `sample_rates`, `channels`,
`sha256`, `license`, `license_url`. Optional: `checkpoint`, `input_name`,
`output_name`, `chunk_seconds`, `stem_names`.

`chunk_seconds` must equal the segment length used when the ONNX graph was
 traced (e.g. 7.8 s for HTDemucs): the separator pads the final chunk to this
length and trims the padded region from the output. `sample_rates` should list
only the rates the model actually accepts - the app resamples in and out.

## Validation gates

Before a model may run, `ModelManager` enforces:

1. **Manifest completeness** — including licence fields. A package without a
   licence is unusable, by design (spec §24/§53).
2. **Integrity** — the checkpoint's SHA-256 must match the manifest. A
   mismatch refuses the model.

Compute a checkpoint hash with (PowerShell):

```powershell
(Get-FileHash .\model.onnx -Algorithm SHA256).Hash.ToLower()
```

## Where packages come from

Phase 0 (`../MODEL_BENCHMARK.md`) evaluates open-weight candidates and their
licences. The export path for a chosen checkpoint (including ONNX export and
metadata) is documented in `../ml/export/`. Do not add a package here until
its provenance entry is complete in `../MODEL_PROVENANCE.md`.
