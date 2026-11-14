# Looper

The simplest possible guitar looper: one loop, one button, controlled by a single MIDI footswitch.
Ships as a VST3 plugin and a standalone app for Windows, macOS and Linux. See [USAGE.md](USAGE.md) for how to use it and [SPEC.md](SPEC.md) for the design.

## Building

Only macOS has been built and tested so far. The Windows and Linux steps follow the JUCE requirements and are untested.

### 1. Prerequisites

You need all of these on every platform:

- **Git** and an internet connection. The first configure downloads JUCE (GPLv3) from GitHub.
- **CMake 3.22 or newer**.
- **A C++17 compiler**, as described below.

| Platform | Compiler and packages |
|---|---|
| macOS | Xcode Command Line Tools: `xcode-select --install`. Full Xcode is not needed. |
| Windows | Visual Studio 2022 (or its Build Tools) with the "Desktop development with C++" workload. |
| Linux | GCC or Clang, plus the JUCE development packages. On Debian/Ubuntu: `sudo apt install build-essential cmake git libasound2-dev libjack-jackd2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libgl1-mesa-dev` |

On macOS, if the compiler can't find standard headers such as `<cmath>`, the Command Line Tools install is damaged; reinstall it (`sudo rm -rf /Library/Developer/CommandLineTools && xcode-select --install`).

### 2. Configure and build

From the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

This builds both the VST3 plugin and the standalone app, and the unit tests. The first configure takes a few minutes because it downloads JUCE.

On Windows, Visual Studio generators are multi-config, so choose the configuration when building instead of at configure time:

```bash
cmake -S . -B build
cmake --build build --config Release
```

To build only one of them:

```bash
cmake --build build --target LooperPlugin_VST3
cmake --build build --target LooperPlugin_Standalone
```

### 3. Find the results

Everything ends up in `build/src/plugin/LooperPlugin_artefacts/Release/`:

| Platform | VST3 plugin | Standalone app |
|---|---|---|
| macOS | `VST3/Looper.vst3` | `Standalone/Looper.app` |
| Windows | `VST3/Looper.vst3` | `Standalone/Looper.exe` |
| Linux | `VST3/Looper.vst3` | `Standalone/Looper` |

### 4. Run the standalone app

```bash
open build/src/plugin/LooperPlugin_artefacts/Release/Standalone/Looper.app    # macOS
build/src/plugin/LooperPlugin_artefacts/Release/Standalone/Looper             # Linux
```

On Windows, double-click `Looper.exe`. On first launch, macOS asks for microphone access; allow it.

If macOS says the app is not signed or won't open it, sign it ad hoc on your own machine:

```bash
codesign --force --deep -s - build/src/plugin/LooperPlugin_artefacts/Release/Standalone/Looper.app
```

### 5. Install the VST3 plugin

The build does not install anything. Copy `Looper.vst3` to your system's VST3 folder, then rescan plugins in your host.

| Platform | Folder (current user) | Folder (all users) |
|---|---|---|
| macOS | `~/Library/Audio/Plug-Ins/VST3/` | `/Library/Audio/Plug-Ins/VST3/` |
| Windows | `%LOCALAPPDATA%\Programs\Common\VST3\` | `C:\Program Files\Common Files\VST3\` |
| Linux | `~/.vst3/` | `/usr/lib/vst3/` |

For example, on macOS:

```bash
cp -R build/src/plugin/LooperPlugin_artefacts/Release/VST3/Looper.vst3 ~/Library/Audio/Plug-Ins/VST3/
```

Install again after every rebuild. If your host has the plugin loaded, close it or restart the host first.

Builds you make yourself run without any developer account. Handing a build to someone else needs signing; see [SPEC.md](SPEC.md) section 12.

### Tests

```bash
ctest --test-dir build
```

The core logic has its own unit tests. You can build and run them without downloading JUCE:

```bash
cmake -S . -B build/core -DLOOPER_BUILD_PLUGIN=OFF
cmake --build build/core
./build/core/tests/looper_tests
```

## Layout

- `src/core`: plain C++ looper (gesture detector, state machine, loop engine). No JUCE dependency.
- `src/plugin`: JUCE processor and editor.
- `tests`: unit tests for the core.

## Licence

GPLv3 (JUCE is used under its GPLv3 licence).
