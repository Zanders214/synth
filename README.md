# ZandersWave

A Serum 2-class **wavetable synthesizer** audio plugin (VST3 / AU / Standalone), built with [JUCE](https://juce.com) (C++).

Two wavetable oscillators + sub + noise → multimode filter → amp envelope, with three envelopes,
four LFOs, four macros, a drag-to-assign modulation matrix, a ten-slot FX rack, an arpeggiator,
preset management, and polyphonic / MPE voice handling — behind a high-fidelity "Neon Plugins" UI.

> Status: under active construction, built milestone-by-milestone (M0 → M8). See
> the build plan and `TASKS` for the roadmap.

## Building

### Prerequisites
- A C++17 compiler — **MSVC** (Visual Studio 2022/2026) on Windows, **Xcode/clang** on macOS.
- **CMake ≥ 3.22** and **Ninja**.
- Git (JUCE is fetched automatically via CMake `FetchContent` — no separate SDK needed).

### Windows (Ninja + MSVC)
From a developer shell where `cmake` and `ninja` are on `PATH` and the MSVC environment is loaded
(`vcvars64.bat`):

```bat
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

If CMake/Ninja are installed via pip and not on `PATH`, prepend them for the session, e.g.:

```powershell
$py = "$env:APPDATA\Python\Python313"
$env:PATH = "$py\site-packages\cmake\data\bin;$py\Scripts;$env:PATH"
```

### macOS (universal binary)
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --parallel
```

### Artifacts
```
build/ZandersWave_artefacts/Release/
├── VST3/ZandersWave.vst3
├── Standalone/ZandersWave(.exe/.app)
└── AU/ZandersWave.component   (macOS only)
```
`COPY_PLUGIN_AFTER_BUILD` also installs the VST3/AU into the user plugin directory on each build.

## Validation
- **Standalone:** launch the Standalone app, set Audio/MIDI, and play.
- **pluginval:** `pluginval --strictness-level 5 <path-to>.vst3` (raise to `10` for release).

## Continuous integration
`.github/workflows/sonarcloud.yml` builds on Windows under the SonarSource build-wrapper and runs
**SonarCloud** C/C++ analysis (project `zandersdaw_synth`, org `zandersdaw`).

> CI setup: add a `SONAR_TOKEN` repository secret and disable *Automatic Analysis* in the SonarCloud
> project settings (C/C++ uses CI-based analysis). Until the token is present, the build job runs and
> the analysis step is skipped.

## License
The two bundled UI fonts (Space Grotesk, JetBrains Mono) are under the SIL Open Font License.
JUCE is used under its own license terms.
