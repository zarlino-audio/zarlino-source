# Zarlino Source — agent instructions

Product: a local system that turns a sound **described in words** into an
extracted, reusable musical object. Not "a better stem splitter".

Read the production specification before touching behaviour:
`specs/ZARLINO_SOURCE_SPEC_V2.md`. It is authoritative — especially section 66
(critical AI-agent rules) and section 67 (definition of done). The Zarlino
Constitution also binds this repository (`zarlino-executive-os/docs/CONSTITUTION.md`).

Non-negotiables from the spec and the constitution:

1. **No placeholders in product code.** If a capability cannot be achieved yet:
   state the limitation, implement the strongest real solution, measure it,
   document the gap, and keep building everything independent.
2. **No fabricated metrics or quality.** Values come from `ArtifactAnalyzer` on
   real audio. Uncertainty is reported explicitly (0 BPM, "Key uncertain") —
   never smoothed into a confident answer.
3. **Arbitrary-source extraction, not a fixed stem set.** Class-based models are
   selected/combined through the `TargetProfile`; never silently substitute a
   fixed four-stem split.
4. **No reference-audio requirement may creep back in.** The query is free text
   plus an optional region.
5. **ASCII only in source files** (mojibake rule), and keep the module layout:
   `dsp/`, `core/{audio,analysis,extraction,project,sampling}`, `inference/`,
   `plugin/`, `ui/`, `tests/`.

Build and test (the definition of proof for this repo):

```
cmd /c "cd /d <repo> && tools\build.cmd ZarlinoSource_All > build\_build_out.log 2>&1"
build\ZarlinoSource_Tests_artefacts\Release\ZarlinoSource_Tests.exe
```

Both must pass, and the release gate is `ctest -L release-gate` when configured
with tests enabled. Builds are verified on the machine that produced the
artifact; binaries are published with their SHA256.

Working rules:

- Tests use the zero-dependency harness in `tests/TestRunner.h` (`ZS_TEST`,
  `ZS_EXPECT`, `ZS_EXPECT_NEAR`). Real audio fixtures are written with JUCE's
  `WavAudioFormat`; there are no checked-in binaries.
- A project file (`.zsource`) must reopen reproducibly; audio references carry
  SHA256 and are verified on load.
- Commit and push real work; the repository is the record of the product.
