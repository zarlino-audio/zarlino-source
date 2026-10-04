# Dataset Licences — Zarlino Source

Per spec §12, **no dataset may be incorporated into a production training
pipeline until it is recorded here.** A dataset being hosted on Hugging Face
(or anywhere else) does not make it commercially usable; the licence must be
read and recorded explicitly.

## Required fields per dataset

```
dataset
source URL
license
commercial-use status
redistribution status
attribution requirements
```

## Current status

**No dataset has been downloaded, and no training has occurred.** This project
is in Phase 0 (research audit); see `MODEL_BENCHMARK.md`.

## Dataset registry

_(empty — populate before the first training or fine-tuning run)_

| # | Dataset | Source URL | Licence | Commercial use | Redistribution | Attribution required |
|---|---|---|---|---|---|---|
| — | — | — | — | — | — | — |

## Rules

1. Record the **exact licence text/version** (e.g. "CC BY 4.0", "CC BY-NC 4.0")
   — not a summary someone else wrote.
2. `CC BY-NC` and similar non-commercial licences are **not usable** for the
   production pipeline.
3. Track **source recordings** and **synthetic mixtures** separately: mixtures
   generated in-house from licensed sources inherit the strictest licence of
   their constituents.
4. Any dataset with unclear provenance is excluded until resolved.
