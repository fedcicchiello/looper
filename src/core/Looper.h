#pragma once

#include "GestureDetector.h"
#include "LoopEngine.h"
#include "StateMachine.h"
#include "Types.h"

#include <atomic>

namespace looper
{

// The whole looper without any plugin framework: switch events in, audio in, audio out.
//
//   switch events -> GestureDetector -> StateMachine -> LoopEngine -> audio
//
// process() splits each block at every switch event and at every hold deadline, so gestures
// act at the exact sample.
class Looper
{
public:
    struct SwitchEvent
    {
        int offset;   // frame offset inside the block; events must be sorted by offset
        bool pressed; // true = switch went down, false = switch came up
    };

    struct Status
    {
        State state = State::Empty;
        int64_t loopLength = 0;     // frames, 0 when there is no loop
        int64_t position = 0;       // play position in frames
        int64_t recordedFrames = 0; // length of the take in progress
        double sampleRate = 48000.0;
    };

    // Not real-time safe (allocates).
    void prepare (double sampleRate, int inChannels, int outChannels, double maxLoopSeconds = kMaxLoopSecondsDefault);

    // Real-time safe.
    void configure (const Timings& timings, SwitchMode mode);

    // `in` and `out` may alias. `events` must be sorted by offset. While bypassed, switch events
    // are ignored and only the dry signal (plus the end of a fade-out) is output.
    void process (const float* const* in, float* const* out, int numFrames,
                  const SwitchEvent* events, int numEvents, bool bypassed = false);

    // Thread-safe snapshot, refreshed at the end of every process() call.
    Status status() const;

    State state() const { return state_; }
    const LoopEngine& engine() const { return engine_; }

private:
    void runUntil (int target);
    void deliverHolds (int64_t time);
    void handle (const GestureEvent& event);
    void apply (const Transition& t);
    void publishStatus();

    GestureDetector detector_;
    LoopEngine engine_;
    TimingSamples timings_;
    double sampleRate_ = 48000.0;
    int inChannels_ = 1;
    int outChannels_ = 1;

    State state_ = State::Empty;
    int64_t now_ = 0; // absolute sample count at the start of the current block
    int cursor_ = 0;  // frames of the current block already rendered
    bool wasBypassed_ = false;

    const float* const* in_ = nullptr;
    float* const* out_ = nullptr;

    std::atomic<int> pubState_ { static_cast<int> (State::Empty) };
    std::atomic<int64_t> pubLoopLength_ { 0 }, pubPosition_ { 0 }, pubRecorded_ { 0 };
};

} // namespace looper
