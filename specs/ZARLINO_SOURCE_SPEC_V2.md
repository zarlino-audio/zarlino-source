# ZARLINO SOURCE

## Production Implementation Specification — v2

> Persisted by the implementation agent on 2026-09-28 from the project owner's
> directive. This file is the in-repo source of truth for V1 scope.

You are the principal software engineer, DSP engineer, ML engineer, product engineer, and QA engineer responsible for implementing Zarlino Source.

This is not a prototype, UI mockup, research proposal, or architectural exercise.

Build a fully functional customer-facing offline audio plugin and standalone application.

The central problem is:

«Given only a mixed recording, let the user identify a sound by description and extract that sound into a clean, reusable musical asset.»

The user does not need to possess an isolated reference recording.

---

## 1. PRODUCT PROMISE

The customer provides:

**A MIXED RECORDING**

and asks for:

"kick", "Rhodes", "Wurlitzer", "electric guitar", "bass", "that synth", "lead vocal", "background vocal", "snare", "shaker", "piano", "brass", "strings"

The system returns the best available isolated version of that source.

The resulting source can then become:

- a clean audio file
- a one-shot
- a phrase
- a loop
- a playable instrument

The product is therefore:

```
MIXED RECORDING
      ↓
SOURCE DESCRIPTION
      ↓
SOURCE IDENTIFICATION
      ↓
SOURCE SEPARATION
      ↓
ARTIFACT REFINEMENT
      ↓
CLEAN SOURCE
      ↓
SAMPLE / LOOP / PHRASE / INSTRUMENT
```

---

## 2. FUNDAMENTAL PRODUCT RULE

No reference audio is required.

Do not design the primary workflow around reference audio.

Do not require the customer to provide:

- `Rhodes reference.wav`
- `Guitar reference.wav`
- `Kick reference.wav`

The system must attempt extraction from the mixture using the user's description and the audio itself.

A reference-audio workflow may be investigated as a future research feature, but it is not part of the V1 production specification.

Do not implement it in V1.

---

## 3. CUSTOMER WORKFLOW

The primary workflow is:

1. Import or capture mixed audio.
2. Select the region to analyze, optionally.
3. Enter what should be extracted.
4. Choose quality mode.
5. Click EXTRACT.
6. Listen to the result.
7. Export or transform the result.

Example:

```
Audio:   full song.wav
Query:   "Rhodes"
Quality: Maximum

→ EXTRACT

Output:  Rhodes.wav
```

Another:

```
Query:   "the electric guitar"
Region:  00:42.000 → 00:51.000

→ EXTRACT

Output:  Electric_Guitar.wav
```

---

## 4. SOURCE QUERY

The source query is the core customer input.

Implement:

```cpp
struct SourceQuery
{
    std::string description;

    std::optional<double> startTime;
    std::optional<double> endTime;
};
```

The description must be free-form.

Do not restrict the UI to a fixed dropdown such as:

```
Vocals
Drums
Bass
Other
```

Those may exist as suggestions, but the customer must be able to type arbitrary descriptions.

Examples:

```
Rhodes
Wurlitzer
electric guitar
acoustic guitar
kick
snare
closed hi-hat
shaker
male vocal
female vocal
background vocals
synth pad
synth lead
808
brass section
violin
piano
organ
percussion
```

---

## 5. SOURCE IDENTIFICATION

The system must translate the user's description into a representation usable by the extraction engine.

Architecture:

```
TEXT QUERY
    ↓
TEXT / SEMANTIC ENCODER
    ↓
SOURCE REPRESENTATION
    ↓
AUDIO ANALYSIS
    ↓
CANDIDATE SOURCE IDENTIFICATION
    ↓
SOURCE SEPARATION
```

Do not implement a hard-coded dictionary such as:

```cpp
if (query == "kick")
    return kickStem;
```

That is explicitly prohibited.

The system must be capable of generalizing beyond a fixed list of instrument names.

---

## 6. IMPORTANT ML DEVELOPMENT RULE

Do not automatically train a new model.

Before training anything:

1. Identify available open-weight source-separation models.
2. Verify their licenses.
3. Download the permitted checkpoints.
4. Build an evaluation harness.
5. Test them against representative target sources.
6. Determine what capabilities already exist.
7. Only then decide whether fine-tuning is necessary.

The first implementation should leverage existing research wherever possible.

---

## 7. MODEL BENCHMARK

At minimum evaluate available open-weight candidates in the source-separation ecosystem.

Candidates may include, where licenses permit:

- Demucs / HTDemucs
- BS-RoFormer
- other high-quality open-weight music separation models

Do not assume one model is superior.

Build an interchangeable interface:

```cpp
class ISeparator
{
public:
    virtual ~ISeparator() = default;

    virtual SeparationResult separate(
        const AudioBuffer& mixture,
        const SeparationRequest& request,
        ProgressCallback progress,
        CancellationToken cancellation) = 0;
};
```

Each model becomes an implementation:

```
HTDemucsSeparator
BSRoFormerSeparator
ZarlinoSeparator
```

---

## 8. MODEL EVALUATION BEFORE TRAINING

Build a reproducible benchmark.

Test sources including:

vocals, kick, snare, hi-hat, percussion, bass, electric guitar, acoustic guitar, piano, Rhodes, Wurlitzer, organ, synth, strings, brass, FX

The benchmark must measure actual extraction quality.

Where ground truth is available, calculate:

SDR, SI-SDR, SIR, SAR, spectral error, transient preservation

Also calculate artifact-oriented metrics where technically justified.

Do not fabricate quality scores.

---

## 9. MODEL DECISION TREE

After benchmarking:

```
                 OPEN MODELS
                     │
                     ▼
             Can they extract
             required sources?
                     │
             ┌───────┴───────┐
             │               │
            YES              NO
             │               │
             ▼               ▼
      integrate them    investigate
                        fine-tuning
                             │
                             ▼
                    specialized model
```

Do not train from scratch unless evidence shows that fine-tuning existing models is inadequate.

---

## 10. FINE-TUNING STRATEGY

If existing models are insufficient:

Fine-tune an existing architecture before creating a new architecture.

Priority:

```
pretrained model
      ↓
Zarlino-specific dataset
      ↓
fine-tuning
      ↓
evaluation
      ↓
production model
```

Training from random initialization is not the default path.

---

## 11. TRAINING COMPUTE

The training pipeline must support:

- CUDA GPU
- mixed precision
- gradient accumulation
- checkpointing
- resumable training
- validation
- experiment logging

A T4 16 GB should be treated as a valid development/fine-tuning target.

CPU training may be supported for debugging and tiny experiments but must not be the assumed production training environment.

Do not design the training process around a multi-week CPU training run.

---

## 12. DATASET STRATEGY

The first dataset may use legally usable public datasets, including datasets distributed through platforms such as Hugging Face.

However:

Do not assume that a dataset hosted on Hugging Face is automatically commercially usable.

Every dataset must have recorded provenance:

```
dataset
source URL
license
commercial-use status
redistribution status
attribution requirements
```

Create:

```
DATASET_LICENSES.md
```

before incorporating data into a production training pipeline.

---

## 13. DATASET STRUCTURE

Create:

```
dataset/

    sources/

        vocals/
        drums/
        bass/
        guitar/
        piano/
        keys/
        synth/
        strings/
        brass/
        percussion/
        other/

    mixtures/

        train/
        validation/
        test/

    metadata/

        sources.json
        mixtures.json
        licenses.json
```

The dataset must distinguish:

**source recording**

from:

**synthetically constructed mixture**

---

## 14. SYNTHETIC MIXTURE GENERATION

Use isolated source recordings to generate controlled mixtures.

Example:

```
Rhodes
+
bass
+
kick
+
snare
+
vocal
+
synth
=
training mixture
```

Randomize:

- source gain
- panning
- stereo width
- EQ
- compression
- saturation
- reverb
- delay
- timing
- noise
- clipping
- mastering
- masking relationships

The target source remains known.

Therefore:

```
mixture → known target
```

provides exact training supervision.

---

## 15. REAL-WORLD VALIDATION

Synthetic mixtures are insufficient.

Create a separate real-world evaluation set containing actual mixed recordings.

Where ground truth cannot be obtained, use objective artifact diagnostics and controlled expert evaluation during development.

Do not allow the model to be declared successful merely because it performs well on synthetic mixtures.

---

## 16. ARBITRARY-SOURCE EXTRACTION

The production target is not fixed four-stem separation.

The architecture must support:

```
query:
"Rhodes"
```

rather than only:

```
stem:
other
```

The system may internally use fixed stems or hierarchical separation as an intermediate strategy.

For example:

```
MIX
 ↓
coarse separation
 ↓
OTHER
 ↓
candidate source analysis
 ↓
Rhodes extraction
```

This is acceptable internally.

The customer must still receive:

```
Rhodes
```

rather than an "other.wav" file.

---

## 17. HIERARCHICAL SEPARATION

Implement the architecture so multiple separation passes can be chained.

Example:

```
MIX
 │
 ├── vocals
 ├── drums
 ├── bass
 ├── guitar
 ├── piano
 └── other
          │
          ▼
      secondary model
          │
          ├── synth
          ├── keys
          ├── strings
          ├── brass
          ├── FX
          └── ambience
```

The exact hierarchy must be determined experimentally.

Do not hard-code the hierarchy as a claim about the final model.

---

## 18. SOURCE CANDIDATE ANALYSIS

For arbitrary queries, analyze the mixture for candidate regions using:

- spectral characteristics
- harmonic structure
- temporal behavior
- transient structure
- pitch
- spatial information
- semantic query representation

The candidate system should help determine:

«Where in the mixture is the requested source most likely present?»

This stage may reduce the amount of audio requiring expensive separation.

---

## 19. TEMPORAL SELECTION

Allow the user to optionally specify:

```
start time
end time
```

Example:

```
"that Wurlitzer"
00:32 → 00:38
```

If no region is specified, analyze the full input.

The product must work without a selection.

---

## 20. CHANNEL/SPATIAL ANALYSIS

Analyze stereo information.

Potentially exploit:

- left/right energy
- mid/side representation
- inter-channel phase
- spatial consistency

Do not assume panning alone identifies a source.

Spatial information is a feature, not a guaranteed separator.

---

## 21. EXTRACTION MODES

Provide three modes.

**Preview**

Fast model or reduced computation.

Purpose:

«determine whether the requested source is being found.»

**High Quality**

Normal final extraction.

**Maximum**

Most expensive available offline processing.

The maximum mode may perform:

```
multiple separation stages
+
artifact analysis
+
refinement
```

---

## 22. COMPUTATIONAL ARCHITECTURE

Do not force the highest-quality model below 10 GFLOPs.

Instead:

```
PREVIEW ENGINE
    ↓
low computational cost
    ↓
interactive audition
```

and:

```
HQ ENGINE
    ↓
offline
    ↓
quality prioritized
```

The preview engine should target approximately:

**<10 GFLOPs/sec**

where achievable.

The HQ engine has no arbitrary compute ceiling.

---

## 23. INFERENCE BACKEND

Use a native inference runtime such as:

**ONNX Runtime**

or another production-appropriate runtime if benchmarking demonstrates a superior choice.

Requirements:

- CPU inference
- multithreading
- optional GPU acceleration
- model versioning
- model integrity validation
- cancellation
- deterministic configuration where possible

Do not require CUDA.

---

## 24. MODEL PACKAGE

Each production model must have:

```json
{
  "name": "source-extractor",
  "version": "1.0.0",
  "model_format": "onnx",
  "sample_rates": [44100, 48000, 88200, 96000],
  "channels": 2,
  "sha256": "...",
  "license": "...",
  "license_url": "..."
}
```

The application must validate the model before execution.

---

## 25. AUDIO PIPELINE

Production pipeline:

```
INPUT MIX
    ↓
format validation
    ↓
sample-rate handling
    ↓
channel analysis
    ↓
optional region selection
    ↓
source-query encoding
    ↓
candidate analysis
    ↓
source separation
    ↓
artifact analysis
    ↓
optional refinement
    ↓
source reconstruction
    ↓
asset creation
```

---

## 26. DSP

Implement and test:

- STFT
- iSTFT
- resampling
- channel conversion
- phase handling
- overlap-add
- fades
- transient detection
- zero-crossing analysis

Default STFT may use:

```
FFT = 2048
hop = 512
Hann window
```

but model-specific configurations must be supported.

Do not assume 48 kHz.

---

## 27. ARTIFACT ANALYSIS

Implement an internal artifact analyzer.

Detect where technically possible:

- source leakage
- musical noise
- warbling
- chirping
- transient destruction
- spectral discontinuity
- phase instability
- boundary artifacts

Track internal metrics:

- target preservation
- leakage
- transient integrity
- spectral integrity
- phase integrity
- artifact score
- confidence

Metrics must be computed, not invented.

---

## 28. REFINEMENT

If the extracted source fails internal quality criteria:

```
EXTRACT
   ↓
ANALYZE
   ↓
acceptable?
 ┌─┴─┐
YES  NO
 │    ↓
DONE REFINE
      │
      ▼
    ANALYZE
```

Maximum:

**3 refinement passes**

Do not create an infinite processing loop.

---

## 29. SOURCE RESULT

The fundamental internal object is:

```cpp
struct SourceResult
{
    AudioBuffer audio;

    std::string requestedDescription;

    SourceMetadata metadata;

    QualityMetrics quality;

    ExtractionDiagnostics diagnostics;

    bool successful;

    std::string errorMessage;
};
```

Everything downstream consumes `SourceResult`.

---

## 30. ONE-SHOT CREATION

Implement:

```
SourceResult
      ↓
OneShotCreator
      ↓
SampleAsset
```

Automatically determine:

- attack
- body
- decay
- useful tail
- start boundary
- end boundary

Use:

- zero-crossing analysis
- transient detection
- short fades

Do not destroy the source's transient character.

---

## 31. PHRASE CREATION

Allow the user to convert an extracted source into a phrase.

Determine:

- BPM
- beat boundaries
- bar boundaries
- phrase length
- key where sufficiently confident

Unknown information must remain unknown.

Do not fabricate key or tempo.

---

## 32. LOOP CREATION

Find candidate loop points using:

- waveform continuity
- spectral continuity
- phase continuity
- beat alignment
- transient compatibility

Rank candidates internally.

Allow manual adjustment.

Support:

- loop start
- loop end
- crossfade

---

## 33. PITCH ANALYSIS

For monophonic material:

- fundamental frequency
- pitch trajectory
- confidence

For polyphonic material:

- candidate fundamentals
- confidence

Never force an unreliable single pitch.

---

## 34. TEMPO ANALYSIS

Estimate tempo when sufficient rhythmic information exists.

Expose:

- BPM
- confidence

If uncertain:

```
Tempo uncertain
```

Do not invent a BPM.

---

## 35. KEY ANALYSIS

Estimate key where technically justified.

Expose:

```
A minor
C major
```

etc.

or:

```
Key uncertain
```

Never fabricate a key.

---

## 36. INSTRUMENTIZATION

The user can take a clean extracted source and select:

«MAKE INSTRUMENT»

Implement:

```
IInstrumentBuilder
```

Input:

```
SourceResult / SampleAsset[]
```

Output:

```
Instrument
```

---

## 37. ONE-SHOT INSTRUMENT

For one pitched sample:

1. Detect root note.
2. Determine safe pitch range.
3. Map sample.
4. Support MIDI playback.
5. Add envelope.
6. Add velocity response.

Do not automatically pitch a single sample over an unrealistic keyboard range.

---

## 38. MULTISAMPLE INSTRUMENT

If multiple notes are available:

```
C3
E3
G3
C4
E4
G4
```

create automatic mappings.

Each sample has:

- root note
- low note
- high note
- velocity range

Use the nearest appropriate source sample.

---

## 39. PHRASE INSTRUMENT

Allow a phrase to be mapped chromatically.

Pitch and time manipulation must be separate.

The phrase must remain synchronized to the host tempo when used in a DAW.

---

## 40. INSTRUMENT ENGINE

Initial controls:

```
Attack
Decay
Sustain
Release

Filter cutoff
Filter resonance

Pitch
Fine tune

Sample start
Loop
Loop crossfade

Velocity
Stereo
```

Support MIDI:

- note-on
- note-off
- velocity

---

## 41. EXPORT

Export:

- WAV 16-bit
- WAV 24-bit
- WAV 32-bit float

Export:

- clean source
- one-shot
- phrase
- loop

Export MIDI where valid musical information has been identified.

---

## 42. PROJECT FORMAT

Create:

```
.zsource
```

Store:

- source audio reference or embedded source audio
- query
- selection
- processing settings
- model version
- extraction diagnostics
- sample metadata
- instrument mapping

Projects must reopen reproducibly.

---

## 43. PLUGIN SAFETY

The plugin must never execute neural inference on the DAW real-time audio thread.

Architecture:

```
DAW AUDIO THREAD
      │
      ▼
capture/request
      │
      ▼
thread-safe queue
      │
      ▼
worker threads
      │
      ▼
model inference
      │
      ▼
result
      │
      ▼
UI / non-real-time playback
```

No:

- blocking mutexes on audio thread
- model loading on audio thread
- large allocations on audio thread
- synchronous neural inference in "processBlock"

---

## 44. OFFLINE PROCESSING

Processing must visibly occur outside the real-time path.

Display:

```
Analyzing audio...
Finding source...
Separating...
Refining...
Finalizing...
```

The progress indicator must represent real processing stages.

Do not fake progress.

---

## 45. CANCELLATION

Every long-running operation must support cancellation.

Check cancellation:

- between chunks
- between model stages
- between refinement passes
- before export

After cancellation:

- workers stop
- memory is released
- UI returns to idle state
- DAW remains functional

---

## 46. MEMORY MANAGEMENT

Before starting extraction:

Estimate:

- input memory
- model memory
- working memory
- output memory

If insufficient:

```
Required: X GB
Available: Y GB
```

Offer a lower-compute mode where possible.

Never intentionally cause an out-of-memory crash.

---

## 47. USER INTERFACE

SCREEN 1 — EXTRACT

```
SOURCE

[ waveform ]

Region
[ optional ]

What do you want to extract?

[ Rhodes                         ]

Quality

[ Preview ▼ ]

            EXTRACT
```

This is the central interface.

No reference-audio field.

No unnecessary model terminology.

---

## 48. PROCESSING SCREEN

```
EXTRACTING RHODES

Analyzing source...
██████████░░░░░░

Finding target...
████████████░░░░

Separating...
██████████████░░

Refining...
████████████████

[ CANCEL ]
```

---

## 49. RESULT SCREEN

```
EXTRACTED SOURCE

Rhodes

[ waveform ]

[ PLAY ]

[ SAVE WAV ]

[ ONE-SHOT ]

[ PHRASE ]

[ LOOP ]

[ INSTRUMENT ]
```

---

## 50. INSTRUMENT SCREEN

Display:

- sample map
- keyboard
- waveform

Controls:

```
Attack
Decay
Sustain
Release

Filter
Pitch
Fine Tune
Sample Start
Loop
Stereo
```

---

## 51. DESIGN LANGUAGE

Follow Zarlino Audio design:

- near-black
- steel secondary surfaces
- white typography
- blue accent
- Inter
- IBM Plex Mono
- 4 px radius
- 8 px spacing
- no gradients
- no unnecessary visual effects

The product should look like a professional audio instrument.

Do not create an "AI magic" aesthetic.

---

## 52. REPOSITORY

Create:

```
zarlino-source/

├── CMakeLists.txt
├── README.md
├── BUILD.md
├── TESTING.md
├── MODEL_PROVENANCE.md
├── DATASET_LICENSES.md
│
├── apps/
│   └── standalone/
│
├── plugin/
│
├── core/
│   ├── audio/
│   ├── extraction/
│   ├── analysis/
│   ├── sampling/
│   ├── instruments/
│   └── project/
│
├── dsp/
│
├── inference/
│   ├── ISeparator.*
│   ├── ModelRuntime.*
│   ├── ModelManager.*
│   └── implementations/
│
├── models/
│
├── ui/
│
├── tests/
│
├── benchmarks/
│
└── ml/
    ├── datasets/
    ├── training/
    ├── evaluation/
    └── export/
```

---

## 53. MODEL PROVENANCE

For every model used, record:

- model name
- repository
- checkpoint
- version
- license
- weights license
- training-data information
- commercial-use status
- redistribution status
- modifications

Do not bundle a model into a commercial product until its redistribution rights have been verified.

---

## 54. DEVELOPMENT PHASE 0 — RESEARCH AUDIT

Before writing the production separator:

1. Search available open-weight models.
2. Record candidate models.
3. Verify licenses.
4. Download permitted checkpoints.
5. Determine model input/output requirements.
6. Run inference.
7. Build benchmark fixtures.
8. Compare results.

Deliver:

```
MODEL_BENCHMARK.md
```

Do not train anything yet.

---

## 55. DEVELOPMENT PHASE 1 — APPLICATION FOUNDATION

Implement:

- JUCE
- CMake
- VST3
- standalone
- plugin state
- audio input
- file loading
- waveform
- selection
- background worker infrastructure
- UI shell

Build must succeed.

---

## 56. DEVELOPMENT PHASE 2 — DSP

Implement:

- STFT
- iSTFT
- resampling
- channel processing
- waveform rendering
- spectrogram
- transient analysis
- loop detection

Write automated tests.

---

## 57. DEVELOPMENT PHASE 3 — OPEN-WEIGHT INTEGRATION

Integrate the best legally usable open model(s).

The separator must perform real inference.

No:

- dummy mask
- silence
- copy input
- random output
- placeholder WAV

---

## 58. DEVELOPMENT PHASE 4 — SOURCE QUERY

Implement the actual text-query pathway.

The system must be capable of accepting:

```
Rhodes
```

and passing the semantic representation into the source-identification/extraction pipeline.

Do not reduce arbitrary text to a fixed enumeration.

---

## 59. DEVELOPMENT PHASE 5 — ARBITRARY SOURCE RESEARCH

Evaluate:

- fixed-stem models
- hierarchical separation
- semantic source localization
- text-conditioned separation
- query-conditioned models

Choose the smallest architecture that can satisfy the product quality requirement.

Do not train a giant model merely because it is technically possible.

---

## 60. DEVELOPMENT PHASE 6 — SPECIALIZED FINE-TUNING

Only if benchmark evidence requires it:

```
existing open model
+
licensed training data
+
Zarlino-specific mixtures
↓
fine-tune
↓
evaluate
↓
export
```

The first experiment should be small enough to run on a T4-class GPU.

Use:

- mixed precision
- gradient accumulation
- checkpoints
- validation
- experiment tracking

---

## 61. DEVELOPMENT PHASE 7 — QUALITY REFINEMENT

Implement:

- artifact analysis
- refinement
- source cleanup
- one-shot creation
- phrase creation
- loop creation
- pitch analysis
- tempo analysis
- key analysis

---

## 62. DEVELOPMENT PHASE 8 — INSTRUMENT

Implement:

- sampler
- MIDI
- root detection
- mapping
- multisampling
- ADSR
- filter
- pitch
- phrase playback
- preset saving

---

## 63. DEVELOPMENT PHASE 9 — PROJECTS

Implement:

```
.zsource
```

save/load.

---

## 64. DEVELOPMENT PHASE 10 — QA

Run:

**Functional**

- [ ] import audio
- [ ] play audio
- [ ] query source
- [ ] extract source
- [ ] export WAV
- [ ] create one-shot
- [ ] create phrase
- [ ] create loop
- [ ] create instrument
- [ ] MIDI playback
- [ ] save project
- [ ] load project

**Extraction**

- [ ] vocal
- [ ] kick
- [ ] snare
- [ ] percussion
- [ ] bass
- [ ] guitar
- [ ] piano
- [ ] Rhodes
- [ ] Wurlitzer
- [ ] synth
- [ ] strings
- [ ] brass

**Engineering**

- [ ] cancellation
- [ ] memory failure
- [ ] model failure
- [ ] corrupt audio
- [ ] unsupported format
- [ ] unsupported sample rate
- [ ] DAW loading
- [ ] DAW playback
- [ ] audio-thread safety

---

## 65. CUSTOMER ACCEPTANCE TEST

The product must pass these real workflows.

**Test A — Rhodes**

Input: mixed song. Query: "Rhodes". Expected: usable Rhodes extraction. The user must not provide a reference recording.

**Test B — Wurlitzer**

Input: mixed song containing Wurlitzer. Query: "Wurlitzer". Expected: usable Wurlitzer extraction.

**Test C — Guitar**

Query: "electric guitar". Expected: usable electric-guitar extraction.

**Test D — Percussion**

Query: "shaker". Expected: shaker extraction.

**Test E — Sampling**

Extract: "kick". Create: one-shot. Export WAV.

**Test F — Instrument**

Extract multiple notes from a source. Create: multisample instrument. Play it via MIDI.

**Test G — Phrase**

Extract a musical phrase. Create a loop. Change host tempo. Verify synchronization.

---

## 66. CRITICAL AI-AGENT RULES

You are responsible for implementation, not merely planning.

Do not stop at scaffolding.

Do not claim a feature is complete because its UI exists.

Do not create placeholder ML behavior.

Do not fabricate evaluation results.

Do not fabricate model quality.

Do not fabricate licensing information.

Do not silently substitute a fixed four-stem system for arbitrary source extraction.

Do not silently reintroduce reference-audio requirements.

If an exact capability cannot yet be achieved:

1. State the limitation.
2. Implement the strongest real solution available.
3. Measure it.
4. Document the gap.
5. Continue implementing all independent functionality.

Do not hide research uncertainty behind polished UI.

---

## 67. DEFINITION OF DONE

The product is DONE when the following are true:

- [ ] Standalone builds
- [ ] VST3 builds
- [ ] Plugin loads in a DAW
- [ ] Mixed audio can be imported
- [ ] User can type an arbitrary source description
- [ ] No reference recording is required
- [ ] Source extraction performs real inference
- [ ] Extraction works offline
- [ ] Preview mode works
- [ ] HQ mode works
- [ ] Artifact analysis works
- [ ] WAV export works
- [ ] One-shot creation works
- [ ] Phrase creation works
- [ ] Loop creation works
- [ ] Pitch analysis works
- [ ] Tempo analysis works
- [ ] Key detection reports uncertainty correctly
- [ ] Instrument creation works
- [ ] MIDI playback works
- [ ] Multisample mapping works
- [ ] Project save/load works
- [ ] Cancellation works
- [ ] Memory failures are handled
- [ ] DAW real-time thread is protected
- [ ] Model provenance is documented
- [ ] Dataset licenses are documented
- [ ] Regression tests pass
- [ ] No release-blocking crashes remain

When all defined acceptance gates pass:

**SHIP.**

Do not add new requirements after the defined standard has been met.

---

## 68. PRODUCT PRINCIPLE

The product is not:

«"A better stem splitter."»

It is:

«A local system for turning a sound described by the user into an extracted, reusable musical object.»

The architecture therefore separates:

```
SOURCE IDENTIFICATION
        ↓
SOURCE EXTRACTION
        ↓
SOURCE CLEANUP
        ↓
SOURCE INTERPRETATION
        ↓
SOURCE INSTRUMENTIZATION
```

The separator is infrastructure.

The customer-facing value is:

```
"What's that sound?"
        ↓
"Extract it."
        ↓
"Now I can use it."
```

Build that system.
