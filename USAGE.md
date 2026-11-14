# Using Looper

Looper is a one-button guitar looper. You control it with a single MIDI footswitch (or by clicking the big button). There is one loop, no tempo and no level controls.

## The window

```
┌───────────────────────────────────┐
│                                   │
│            ╭─────────╮            │
│          ╱             ╲          │
│         │     PLAY      │         │   big round button
│         │     0:12.8    │         │   = state, time, loop position
│          ╲             ╱          │
│            ╰─────────╯            │
│  No MIDI switch mapped...  Last MIDI: CC 64 = 127 (ch 1)
│  ● MIDI   IN ▮▮▮▮▯▯▯     [Settings]│
└───────────────────────────────────┘
```

The window can be resized. The plugin works the same with the window closed.

### The big button

Its colour and label show what the looper is doing:

| Colour | Label | Meaning | Time shown |
|---|---|---|---|
| Grey | EMPTY | Nothing recorded. Your guitar passes through. | none |
| Red | REC | Recording the first take. | time recorded so far |
| Green | PLAY | Playing the loop. | loop length |
| Orange | OVERDUB | Playing the loop and adding what you play to it. | loop length |
| Dark green | STOPPED | Loop kept but silent. | loop length |

- **Position ring:** while playing or overdubbing, a white arc around the button shows where you are in the loop.
- **Hold fill:** while the switch is held down, the button fills from the bottom over the hold time. When it is full, the loop is cleared. This is visual only; Looper never makes a sound to confirm anything. The fill does not appear in Latch mode.
- **Clicking the button** does the same as the footswitch: mouse down is a press, mouse up is a release. This is handy for trying things out without a pedal.

### Footer

- **Hint (left):** "No MIDI switch mapped" appears until you have used MIDI Learn. Until then, MIDI is ignored.
- **Last MIDI (right):** the last CC or note message the plugin received, for example `CC 64 = 127 (ch 1)`. Use it to check what your switch actually sends.
- **MIDI light:** flashes when any MIDI message arrives and stays lit while the switch is held.
- **IN meter:** the level of your input, one bar per channel. It turns red near full scale. It is display only; there are no level controls.

## Playing with one button

Every press takes effect the moment the switch goes down.

| Gesture | What happens |
|---|---|
| **Tap** | Empty → start recording. Recording → close the loop and play it. Playing → start overdubbing. Overdubbing → back to playing. Stopped → play from the start. |
| **Double-tap** (two taps close together) | Stop. The loop is kept. Any overdub you only just started is thrown away. |
| **Double-tap and hold** (keep the second tap down) | Stop and clear, silently. This is the usual way to end a song and wipe the loop. |
| **Hold** (about 1 s) | Clear the loop. The first tap of the hold still does its normal action, so the loop keeps sounding until the hold completes. From Stopped, you will hear the loop briefly before it clears. |

Things worth knowing:

- Taps in the first half second of a recording are ignored, so the loop can't be shorter than that.
- A recording stops by itself and starts playing at 5 minutes.
- Stopping always rewinds to the start, and playing always starts from the start.
- Overdubs add at full level on top of the old material. Nothing fades, so a very loud pile of layers can clip at the output. There is no undo.
- Your dry guitar always passes through, in every state.

### Latch switches

A latch switch sends one message per press and has no release. Looper then counts each message as a tap, and holds are not available. Set **Switch type** to Latch in Settings. Tap and double-tap work as above, and a **triple-tap** stops and clears.

## Settings

Click **Settings** (bottom right) to open the panel, and click it again to close it.

### MIDI

- **Learn:** click it, then press your footswitch once. Looper remembers the message type (CC or note), its number and its channel, and the button changes to **Cancel** while it waits. The mapping appears next to the button, for example `MIDI: CC 64, ch 1, inverted`.
  - Press the switch, don't release it first. The first message is taken to be the press.
  - **Inverted** appears when the first message was a CC with a low value, meaning your switch sends a low value on press and a high one on release. Looper detects this by itself. If the mapping behaves backwards, Learn again.
- **Any MIDI channel:** ignore the channel and respond on all of them.
- **Switch type:** **Momentary** (default) for a switch that sends a press and a release. **Latch** for one that sends a single message per press.

### Timing

These tune how gestures are recognised. The defaults are a good starting point.

| Setting | Default | Range | What it does |
|---|---|---|---|
| Double-tap window | 300 ms | 150–600 ms | The longest gap between two taps that still counts as a double-tap. |
| Hold time | 1000 ms | 400–2000 ms | How long you hold to clear. Not available in Latch mode. |
| Fade | 5 ms | 1–50 ms | The short fade used on starts, stops and the loop's wrap point, which prevents clicks. |

**Reset to defaults** restores all three.

### Audio/MIDI settings (standalone app only)

The standalone app has an extra **Audio/MIDI settings...** button for choosing your audio device, input and output channels, sample rate and buffer size, and for enabling MIDI inputs, including Bluetooth MIDI. Inside a host, the host does all of this.

If your audio interface also has direct monitoring switched on, you will hear your guitar twice, once directly and once through Looper. Switch one of them off.

## In a plugin host

- **Channels:** mono in/mono out, mono in/stereo out, and stereo in/stereo out. Stereo in with mono out is not supported.
- **MIDI:** the host must send MIDI to the plugin. Looper only reacts to messages it receives, so check your host's MIDI routing if the "Last MIDI" line stays empty.
- **Host parameters:** *Footswitch* is an on/off parameter you can map or automate in the host instead of using MIDI Learn. *Bypass* is the host's bypass.
- **Bypass:** bypassing acts like a stop. The loop fades out and is kept, only your dry guitar passes, and it waits for your tap after you un-bypass.
- **What is saved:** your MIDI mapping, switch type and timings are saved with the host project. The recorded loop is never saved and is lost when the plugin is unloaded.
- **Sample rate or channel changes** in the host clear the loop.

## Troubleshooting

| Problem | Likely cause |
|---|---|
| Loop clears about 1 s after you press | Looper thinks the switch is still held down, so the release isn't being recognised. Click Learn and press the switch again, and check the **Last MIDI** line for what it sends on press and release. |
| Nothing happens when you press the switch | The mapping says "Not mapped", the host isn't routing MIDI to the plugin, or the channel differs. Watch the **Last MIDI** line and the MIDI light. |
| Double-tap is hard to hit, or taps merge | Raise or lower the **Double-tap window**. |
| You hear your guitar twice | Direct monitoring on the interface and Looper are both passing the dry signal. Turn one off. |
| Two taps stop the loop when you wanted to overdub, or the reverse | Taps closer than the double-tap window count as a double-tap. Wait slightly longer between a closing tap and the next one. |
