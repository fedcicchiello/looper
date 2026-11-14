#pragma once

#include "Types.h"

namespace looper
{

// What the audio engine must do. The engine executes these at an exact sample.
enum class Command
{
    None,
    StartRecording,     // begin the first take
    CloseLoop,          // close the take and start playing it from the top
    CloseLoopStopped,   // close the take but do not play it (bypass while recording)
    StartPlayback,      // play the loop from the top
    StartOverdub,       // begin summing the input into the loop
    EndOverdub,         // stop summing, keep what was added
    Stop,               // fade out and stop, keep the loop (any overdub is kept)
    StopDiscardOverdub, // fade out and stop; throw away an overdub that is not yet committed
    Clear               // fade out and forget the loop (or discard the take)
};

struct Transition
{
    State next;
    Command command;
};

// Pure functions: the whole control model of SPEC sections 4.3 - 4.5 lives here.

// `count` is the tap's position in its multi-tap sequence.
// `recordedFrames` is how long the current take is (only meaningful while Recording).
Transition onTap (State state, int count, int64_t recordedFrames, int64_t minLoop);

// A hold clears everything, from any state that has something to clear.
Transition onHold (State state);

// The host bypassed the plugin: behave like a stop, keep the loop.
Transition onBypass (State state, int64_t recordedFrames, int64_t minLoop);

// The engine closed the take by itself because it reached the maximum length.
Transition onRecordingFull (State state);

} // namespace looper
