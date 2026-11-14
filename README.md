# Looper

The simplest possible guitar looper: one loop, one button, controlled by a single MIDI footswitch.
Ships as a VST3 plugin and a standalone app for Windows, macOS and Linux. See [SPEC.md](SPEC.md).

## Build

Requires CMake 3.22+ and a C++17 compiler. JUCE is downloaded automatically.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

Artefacts are in `build/src/plugin/LooperPlugin_artefacts/Release/` (`VST3/` and `Standalone/`).

Core logic only (no JUCE download), with unit tests:

```bash
cmake -S . -B build/core -DLOOPER_BUILD_PLUGIN=OFF
cmake --build build/core && ./build/core/tests/looper_tests
```

## Layout

- `src/core`: plain C++ looper (gesture detector, state machine, loop engine). No JUCE dependency.
- `src/plugin`: JUCE processor and editor.
- `tests`: unit tests for the core.

## Licence

GPLv3 (JUCE is used under its GPLv3 licence).
