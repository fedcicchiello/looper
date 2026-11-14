#pragma once

#include "Types.h"

#include <optional>

namespace looper
{

// Turns raw switch presses/releases into gestures (SPEC section 4.2).
//
// A tap is reported the instant the switch goes down. Its `count` says how many
// taps of the current multi-tap sequence it is. A hold is reported when the switch
// has stayed down for `hold` samples. All times are absolute sample positions.
//
// Usage contract: before delivering a press() or release() at time t, call poll(t)
// until it returns nothing, so that a hold due at or before t fires first.
//
// Sequence rules:
//  - Momentary mode: counts 1, 2, then the next tap starts a new sequence at 1.
//  - Latch mode: counts 1, 2, 3, then the next tap starts a new sequence at 1.
//  - Presses closer than `debounce` to the previous accepted press are ignored.
class GestureDetector
{
public:
    void configure (const TimingSamples& timings, SwitchMode mode);
    void reset();

    std::optional<GestureEvent> press (int64_t time);
    void release (int64_t time);
    std::optional<GestureEvent> poll (int64_t time);

    // Time at which poll() will next return a hold, or kNever.
    int64_t nextDeadline() const;

    bool isDown() const { return down_; }

private:
    TimingSamples timings_;
    SwitchMode mode_ = SwitchMode::Momentary;

    bool havePress_ = false;
    int64_t lastPress_ = 0;
    int lastCount_ = 0;

    bool down_ = false;
    bool holdArmed_ = false;
    int64_t holdDue_ = 0;
};

} // namespace looper
