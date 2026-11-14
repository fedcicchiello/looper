# Looper — Specification & Design

Product name: **Looper**. Its simplicity is the point.

Status: **v1.2**. All requirements are agreed and ready for implementation.

## 1. Vision

The simplest possible software looper for guitarists: **one loop, one button**.
Controlled entirely by a single MIDI footswitch, with a gesture vocabulary modelled on the TC Electronic Ditto Looper.

### Explicit non-goals

- No tempo, BPM, quantize, metronome, click, or host sync.
- No level knobs (loop level, dry level, input/output gain, feedback/decay).
- No multiple tracks, no undo/redo, no reverse/half-speed or other effects.
- No persistence of recorded audio (no save, no export).
- No audible confirmations of any kind (no beeps, clicks, or cue sounds).

## 2. Platform & format

| Item | Decision |
|---|---|
| Formats | **VST3** plugin (main target) and a **Standalone** app |
| OS | Windows, macOS, Linux (desktop only) |
| Not supported | AU, AAX, mobile |
| Framework | JUCE (C++) under its GPLv3 licence, built with CMake |
| Licence | Open source (GPLv3), to be published at some later point |
| Reported latency | 0 samples |

### 2.1 Standalone app

- Both formats are built from the same code using JUCE's standalone wrapper.
- The standalone build adds JUCE's standard **Audio/MIDI settings** dialog: audio device, input and output channels, sample rate, buffer size, and MIDI input devices. These are the only extra settings it has.
- Dry signal always passes through in the standalone app too. If your audio interface also has direct monitoring switched on, you will hear your guitar twice; switch one of the two off.
- On macOS the app asks for microphone permission the first time it runs, which is required to receive audio input.

## 3. Audio

### 3.1 Channel layouts

| Input | Output | Loop buffer | Behaviour |
|---|---|---|---|
| Mono | Mono | 1 ch | Straight through |
| Mono | Stereo | 1 ch | Dry and loop duplicated to L and R |
| Stereo | Stereo | 2 ch | Channels kept separate |

Stereo input with mono output is **not supported**; there is no meaningful use case for it. The plugin refuses that layout, so the host will not offer it.

### 3.2 Signal flow

```
input ──┬───────────────────────────────▶ (+) ──▶ output
        │                                  ▲
        └──▶ [record / overdub] ──▶ LOOP ──┘ playback
```

- **Dry signal always passes through** at unity gain. There is no option to turn it off.
- Loop playback is summed at unity gain.
- Overdubs are **additive**: new material is summed onto the existing loop at unity, with no decay of older layers.
- All processing is 32-bit float. Material over 0 dBFS is kept intact inside the loop; there is no limiter.

### 3.3 Loop memory

- Maximum loop length: **5 minutes**.
- The buffer is allocated in `prepareToPlay` and sized for the current sample rate and input channel count. For example, stereo at 48 kHz needs about 115 MB.
- The audio thread never allocates, locks, or performs I/O.
- If the host changes the sample rate or the channel layout, the plugin is re-prepared and **the loop is cleared**.
- Recorded audio is never saved, neither in the host project nor on disk.

### 3.4 Click-free transitions

Every discontinuity gets a short fade of length `T_fade` (default **5 ms**, user-editable, see §4.6):

- the loop seam: a crossfade between the end and the start of the loop when the loop is closed;
- start and stop of playback;
- entering and leaving overdub, applied as a fade on the overdub write gain;
- clearing.

## 4. Control model

### 4.1 States

| State | You hear | Loop memory |
|---|---|---|
| **Empty** | dry | nothing |
| **Recording** | dry | first take being written; defines the loop length |
| **Playing** | dry + loop | read-only |
| **Overdubbing** | dry + loop | input summed into the loop |
| **Stopped** | dry | loop kept, play position reset to the start |

### 4.2 Gestures

All gestures are detected on a single button.

| Gesture | Definition |
|---|---|
| **Tap** | Switch goes down. It **always acts immediately on press**, never on release. |
| **Double-tap** | A second tap within `T_multi` of the previous tap. |
| **Hold** | Switch kept down for `T_hold` (momentary mode only). |
| **Double-tap & hold** | The second tap of a double-tap, held for `T_hold` (momentary mode only). |
| **Triple-tap** | A third tap within `T_multi` of the second (latch mode only). |

### 4.3 Transitions — momentary mode (default)

```
            tap               tap                tap
  EMPTY ──────────▶ RECORDING ──────▶ PLAYING ◀─────────▶ OVERDUBBING
    ▲                                  │  ▲                    │
    │                       double-tap │  │ tap      double-tap│
    │                                  ▼  │                    │
    │                                STOPPED ◀─────────────────┘
    │                                  │
    └──── double-tap & hold, or hold (any non-empty state): stop + clear
```

| Current state | Tap | Double-tap | Double-tap & hold | Hold |
|---|---|---|---|---|
| Empty | → Recording | — | — | — |
| Recording | close loop → Playing | → Stopped (loop kept) | stop + clear → Empty | clear → Empty |
| Playing | → Overdubbing | → Stopped (loop unchanged) | stop + clear → Empty (silent) | clear → Empty |
| Overdubbing | → Playing | → Stopped (overdub kept) | stop + clear → Empty (silent) | clear → Empty |
| Stopped | → Playing (from start) | → Stopped | stop + clear → Empty | clear → Empty |

**Double-tap & hold is the primary way to end and clear.** It is silent: the second tap stops the loop, and the hold then clears it.

A plain hold reaches the same end state, but the press first performs its tap action. Two consequences:

- From Playing or Recording, the loop keeps sounding during the hold.
- From Stopped, the press starts playback, so a burst of up to `T_hold` is heard before the clear. This matches the Ditto and is accepted.

### 4.4 Transitions — latch mode (option)

A latch switch sends one message per press and has no release, so every message is a tap. Holds do not exist.

| Gesture | Effect |
|---|---|
| Tap | Same as momentary mode |
| Double-tap | → Stopped (same as momentary mode) |
| Triple-tap | Stop + clear → Empty (the third tap clears a loop that is already stopped, so the clear is silent) |

### 4.5 How multi-tap works without delaying the first tap

The first tap always acts immediately. A later tap that arrives within `T_multi` revises the result as follows.

| Before the first tap | First tap (immediate) | Second tap within `T_multi` |
|---|---|---|
| Playing | starts Overdubbing | overdub material is **discarded**; → Stopped; the loop is bit-identical to before |
| Overdubbing | → Playing, overdub committed | → Stopped |
| Recording | loop closed, playback starts | → Stopped, loop kept (up to `T_multi` of the loop's start is audible) |
| Stopped | playback starts | → Stopped (up to `T_multi` of the loop is audible) |

**The overdub side buffer.** For its first `T_multi`, an overdub is written to a small side buffer instead of the loop:

- If the window expires without another tap, the side buffer is mixed into the loop at its original positions, and the overdub continues writing directly into the loop.
- If a second tap arrives, the side buffer is dropped.

Because `L_min > T_multi`, playback can never reach the side-buffered region before it is committed.

### 4.6 Timing constants

Three timings can be changed in Settings. The rest are fixed.

| Constant | Default | Editable | Range | Meaning |
|---|---|---|---|---|
| `T_multi` | 300 ms | **yes** | 150–600 ms | maximum gap between taps of a double- or triple-tap |
| `T_hold` | 1000 ms | **yes** | 400–2000 ms | press duration that counts as a hold |
| `T_fade` | 5 ms | **yes** | 1–50 ms | fade length for click-free transitions |
| `L_min` | max(500 ms, `T_multi` + 100 ms) | no (derived) | — | minimum loop length; taps during the first `L_min` of recording are ignored (holds still clear) |
| `T_debounce` | 30 ms | no | — | presses closer together than this are ignored |
| `L_max` | 5 min | no | — | maximum loop length |

Rules for the editable timings:

- They are stored in the plugin state, which the host saves in its project; the standalone app keeps them in its settings file. They are **not** host-automatable parameters, so the host's parameter list stays clean.
- **Reset to defaults** restores all three.
- A change takes effect from the next gesture or transition. It never alters an action that is already in progress.
- `L_min` is derived from `T_multi` so that it always stays longer than the double-tap window. The overdub side-buffer mechanism in §4.5 depends on this.

### 4.7 Other rules

- If Recording reaches `L_max`, the loop closes automatically → Playing.
- An overdub can run across any number of loop passes.
- Stop always resets the play position to the start. Play always starts from the start.
- Host transport (play/stop) is ignored. The looper runs whenever the host calls `processBlock`.
- **Host bypass behaves like a stop.** When the host bypasses the plugin, the looper goes to Stopped with a `T_fade` fade-out, and the loop is kept. Only dry audio passes while bypassed. On un-bypass the looper stays Stopped and waits for a tap. If bypass happens while Recording, the loop is closed at that point and kept. If the loop is shorter than `L_min`, the take is discarded instead and the looper goes to Empty.
- All timing is measured in **samples** on the audio thread, so gestures are sample-accurate and independent of the GUI. Incoming MIDI events are applied at their sample offset within the block.

## 5. MIDI & control input

### 5.1 Sources

All of the sources below feed the same gesture detector.

1. **Learned MIDI message.** MIDI routed to the plugin by the host.
   - CC: value ≥ 64 = press, < 64 = release.
   - Note: Note On (velocity > 0) = press; Note Off, or Note On with velocity 0, = release.
   - In latch mode every matching message counts as a tap, whatever its value.
2. **Host parameter "Footswitch".** An automatable on/off parameter, so a host's own MIDI mapping can drive the plugin. on = press, off = release.
3. **On-screen button.** Mouse down = press, mouse up = release. For testing without a footswitch.

The spacebar is deliberately not used, because most hosts reserve it for their transport.

**There is no default MIDI mapping.** Until MIDI Learn has been done, incoming MIDI is ignored. The GUI then shows a hint: "No MIDI switch mapped — open ⚙︎ and click Learn". The on-screen button and the host parameter keep working, because they are needed for testing and for mapping inside the host.

### 5.2 MIDI Learn

1. Click **Learn**. The plugin waits for a message.
2. Press the footswitch.
3. The plugin stores the message type (CC or Note), its number, and its channel. Channel can be specific or omni.

The learned mapping and the switch type are saved in the plugin state, which lives in the host project. Audio is never saved.

## 6. User interface

One resizable window, readable from about 2 m away.

```
┌───────────────────────────────────┐
│                                   │
│            ╭─────────╮            │
│          ╱             ╲          │
│         │   PLAYING     │         │  big round button = state + click target
│         │    0:12.8     │         │  ring = loop position
│          ╲             ╱          │
│            ╰─────────╯            │
│                                   │
│  MIDI ●    IN ▮▮▮▮▯▯▯          ⚙︎  │
└───────────────────────────────────┘
```

| State | Button colour | Centre text | Ring |
|---|---|---|---|
| Empty | grey | — | none |
| Recording | red | elapsed time | none |
| Playing | green | loop length | position sweep |
| Overdubbing | orange | loop length | position sweep |
| Stopped | dim green, steady | loop length | none |

- **Hold feedback:** while the switch is down, the button fills progressively until `T_hold`, so the user can see a clear is coming. The feedback is visual only.
- **MIDI LED:** flashes on any incoming message and lights steadily while the learned switch is held.
- **Input meter:** display only. It shows a peak level for each input channel.
- **⚙︎ Settings panel** contains exactly:
  - MIDI Learn button and current mapping (e.g. "CC 80, ch 1", or "Not mapped");
  - Switch type: **Momentary** (default) / **Latch**;
  - Timing:
    - Double-tap window (ms);
    - Hold time (ms), disabled in Latch mode;
    - Fade (ms);
    - **Reset to defaults** button;
  - Standalone only: a button that opens the Audio/MIDI settings dialog.

The GUI is optional at runtime. The looper behaves identically with the editor closed.

## 7. Architecture

```
┌───────────────────────── PluginProcessor (JUCE) ─────────────────────────┐
│                                                                          │
│  MIDI / param / GUI ──▶ InputMapper ──▶ GestureDetector ──▶ LooperState  │
│   (press/release)       (learn, CC/     (tap, multi-tap,    Machine      │
│                          note, latch)    hold; sample time)    │         │
│                                                                ▼         │
│  audio in ────────────────────────────────────────────────▶ LoopEngine ──▶ audio out
│                                                     (buffer, side buffer,│
│                                                      fades, mixing)      │
└──────────────────────────────────────────────────────────────────────────┘
          ▲ lock-free FIFO (GUI presses)        │ atomics (state, position, meters)
          │                                     ▼
                         PluginEditor (GUI, message thread)
```

- **`GestureDetector`** and **`LooperStateMachine`** are plain C++ with no JUCE dependency. They take events with sample timestamps, so they can be unit-tested exhaustively.
- **`LoopEngine`** owns all audio buffers. It is driven by commands from the state machine, issued at exact sample offsets.
- **Thread model:**
  - The audio thread owns all state.
  - GUI presses reach it through a lock-free FIFO.
  - The GUI reads state, position, and meters through atomics.
  - No locks are taken on the audio thread.

## 8. Testing

- **Unit tests** for `GestureDetector` and the state machine, covering every cell of the tables in §4.3–4.5, including boundaries (exactly `T_multi`, exactly `T_hold`, taps before `L_min`).
- **Offline audio tests** for `LoopEngine`:
  - a loop is bit-identical after a cancelled overdub;
  - no discontinuities at the seam or at start/stop;
  - correct mono/stereo mapping;
  - auto-close at `L_max`.
- **Manual host tests** on each OS, for example Reaper (Win/macOS/Linux) plus one more host per OS, using a real MIDI footswitch.

## 9. Performance requirements

- No allocation, locking, or I/O on the audio thread.
- Stable down to a 32-sample buffer at 48 kHz.
- Gesture actions take effect at the sample offset of the triggering MIDI event.

## 10. Out of scope for v1, possible later

- CLAP and LV2 builds (JUCE makes these cheap to add).
- Undo of the last overdub.
- Signed and notarised release builds (see §12).

## 11. Open questions

None at the moment.

## 12. Code signing & distribution

v1 targets personal use, so **no developer accounts are needed**.

| OS | Built on your own machine | Downloaded by someone else, unsigned |
|---|---|---|
| macOS | Runs as-is. Apple Silicon builds are ad-hoc signed automatically by the toolchain. | Gatekeeper blocks it the first time. The user allows it via *System Settings → Privacy & Security → Open Anyway*, or with `xattr -dr com.apple.quarantine <path>`. |
| Windows | Runs as-is. | The standalone `.exe` may trigger a SmartScreen warning ("More info → Run anyway"). Hosts load the VST3 `.dll` without any warning. |
| Linux | Runs as-is. | Runs as-is. |

macOS notes:

- After each rebuild, an ad-hoc signed standalone app may ask for microphone permission again, because its signature has changed. This is harmless.
- Proper signing and notarisation for public releases needs the Apple Developer Program, at USD 99/year.

Windows notes:

- Avoiding SmartScreen warnings for public releases needs a code-signing certificate.

Both of these can wait until the project is published.
