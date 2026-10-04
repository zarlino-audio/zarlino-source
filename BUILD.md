# Building Zarlino Source

## Requirements

| Component | Version used | Notes |
|---|---|---|
| CMake | 3.22+ | `C:\Users\khrit\AppData\Local\Python\pythoncore-3.14-64\Scripts\cmake.exe` in this workspace |
| JUCE | 8.0.13 | Reused from `../JUCE` when building inside the VST workspace |
| Compiler | MSVC (Visual Studio 18) | `vcvars64.bat`; Ninja generator |
| ONNX Runtime | optional at build time | Loaded dynamically at runtime; not required to compile |

## Windows (the supported development flow)

```
tools\build.cmd ZarlinoSource_All ZarlinoSource_Tests
```

The script initialises the MSVC environment, configures `build\` (Ninja,
Release) if needed, and builds the requested targets.

### Manual configure/build

```
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target ZarlinoSource_All ZarlinoSource_Tests --parallel
```

### Targets

| Target | Produces |
|---|---|
| `ZarlinoSource_All` | Aggregate: VST3 bundle + standalone application |
| `ZarlinoSource_VST3` | `ZarlinoSource_artefacts/Release/VST3/Zarlino Source.vst3` |
| `ZarlinoSource_Standalone` | `ZarlinoSource_artefacts/Release/Standalone/Zarlino Source.exe` |
| `ZarlinoSource` | Shared-code static library used by the format targets (no binaries by itself) |
| `ZarlinoSource_Tests` | Console test runner (see `TESTING.md`) |

Artefacts land under `build/` (JUCE `*_artefacts` folders).

### Standalone-only build

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DZARLINO_SOURCE_BUILD_TESTS=OFF
```

## Linux / macOS (not the primary target yet)

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target ZarlinoSource_All ZarlinoSource_Tests --parallel
```

JUCE is expected in `../JUCE` or supplied via `-DZARLINO_SOURCE_JUCE_DIR=`.

## ONNX Runtime

The inference runtime is loaded **dynamically**:

- Windows: `onnxruntime.dll` (or `onnxruntime_*.dll` variants) is searched
  beside the executable, in `runtime/` next to the executable, and via the
  `ONNXRUNTIME_DLL_DIR` environment variable.
- If no runtime is present the application still starts; extraction reports a
  clear "inference runtime unavailable" error instead of producing fake audio.

Place the ONNX Runtime redistributable files under `runtime/` for development
builds or next to the installed binaries for release builds.

## Model packages

Production extraction requires a model package in `models/`:

```
models/
└── <package-name>/
    ├── manifest.json     # name, version, sha256, license, io spec
    └── model.onnx        # checkpoint
```

See `models/README.md` and `MODEL_PROVENANCE.md`. The application validates the
manifest and the checkpoint SHA-256 before execution and refuses to run models
whose license fields are not filled in.
