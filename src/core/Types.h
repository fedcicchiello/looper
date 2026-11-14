#pragma once

// Plain C++ types shared by the looper core. No JUCE dependency.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace looper
{

constexpr int64_t kNever = std::numeric_limits<int64_t>::max();

enum class SwitchMode
{
    Momentary, // press and release are separate messages; supports holds
    Latch      // one message per press; every message is a tap
};

enum class State
{
    Empty,
    Recording,
    Playing,
    Overdubbing,
    Stopped
};

// User-editable timings (SPEC section 4.6). Values are in milliseconds.
struct Timings
{
    static constexpr double kMultiTapMin = 150.0, kMultiTapMax = 600.0, kMultiTapDefault = 300.0;
    static constexpr double kHoldMin = 400.0, kHoldMax = 2000.0, kHoldDefault = 1000.0;
    static constexpr double kFadeMin = 1.0, kFadeMax = 50.0, kFadeDefault = 5.0;

    double multiTapMs = kMultiTapDefault;
    double holdMs = kHoldDefault;
    double fadeMs = kFadeDefault;

    Timings clamped() const
    {
        return { std::clamp(multiTapMs, kMultiTapMin, kMultiTapMax),
                 std::clamp(holdMs, kHoldMin, kHoldMax),
                 std::clamp(fadeMs, kFadeMin, kFadeMax) };
    }
};

// Fixed (non-editable) timings.
constexpr double kDebounceMs = 30.0;
constexpr double kMinLoopBaseMs = 500.0;
constexpr double kMinLoopMarginMs = 100.0; // minLoop is at least multiTap + this
constexpr double kMaxLoopSecondsDefault = 300.0;

// Timings converted to sample counts for a given sample rate.
struct TimingSamples
{
    int64_t multiTap = 0;
    int64_t hold = 0;
    int64_t fade = 0;
    int64_t minLoop = 0; // derived: max(500 ms, multiTap + 100 ms)
    int64_t debounce = 0;

    static TimingSamples make (const Timings& requested, double sampleRate)
    {
        const Timings t = requested.clamped();
        auto toSamples = [sampleRate] (double ms) {
            return std::max<int64_t> (1, static_cast<int64_t> (std::llround (ms * sampleRate / 1000.0)));
        };

        TimingSamples s;
        s.multiTap = toSamples (t.multiTapMs);
        s.hold = toSamples (t.holdMs);
        s.fade = toSamples (t.fadeMs);
        s.minLoop = toSamples (std::max (kMinLoopBaseMs, t.multiTapMs + kMinLoopMarginMs));
        s.debounce = toSamples (kDebounceMs);
        return s;
    }
};

// What the gesture detector reports to the state machine.
struct GestureEvent
{
    enum class Type { Tap, Hold };

    Type type = Type::Tap;
    int count = 1;      // position of the tap in its multi-tap sequence (1, 2, 3); for Hold, the tap it belongs to
    int64_t time = 0;   // absolute sample time at which the gesture takes effect
};

} // namespace looper
