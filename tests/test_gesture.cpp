#include "TestUtil.h"

#include "GestureDetector.h"

using namespace looper;

namespace
{
const double kSr = 48000.0;
int64_t ms (double v) { return static_cast<int64_t> (std::llround (v * kSr / 1000.0)); }

GestureDetector make (SwitchMode mode = SwitchMode::Momentary, Timings t = {})
{
    GestureDetector d;
    d.configure (TimingSamples::make (t, kSr), mode);
    return d;
}
} // namespace

TEST(gesture_single_tap_is_reported_on_press)
{
    auto d = make();
    auto e = d.press (1000);
    CHECK (e.has_value());
    CHECK (e->type == GestureEvent::Type::Tap);
    CHECK_EQ (e->count, 1);
    CHECK_EQ (e->time, 1000);
}

TEST(gesture_double_tap_window_boundaries)
{
    auto d = make();
    d.press (0);
    d.release (ms (50));
    // Exactly at the window: still a double-tap.
    auto e = d.press (ms (300));
    CHECK (e.has_value());
    CHECK_EQ (e->count, 2);

    // One sample past the window: a new sequence.
    auto d2 = make();
    d2.press (0);
    d2.release (ms (50));
    auto e2 = d2.press (ms (300) + 1);
    CHECK (e2.has_value());
    CHECK_EQ (e2->count, 1);
}

TEST(gesture_momentary_sequence_restarts_after_two)
{
    auto d = make();
    CHECK_EQ (d.press (0)->count, 1);
    d.release (ms (40));
    CHECK_EQ (d.press (ms (100))->count, 2);
    d.release (ms (140));
    CHECK_EQ (d.press (ms (200))->count, 1);
}

TEST(gesture_latch_counts_to_three_then_restarts)
{
    auto d = make (SwitchMode::Latch);
    CHECK_EQ (d.press (0)->count, 1);
    CHECK_EQ (d.press (ms (100))->count, 2);
    CHECK_EQ (d.press (ms (200))->count, 3);
    CHECK_EQ (d.press (ms (300))->count, 1);
}

TEST(gesture_latch_never_holds)
{
    auto d = make (SwitchMode::Latch);
    d.press (0);
    CHECK_EQ (d.nextDeadline(), kNever);
    CHECK (! d.poll (ms (5000)).has_value());
}

TEST(gesture_hold_fires_at_exact_time)
{
    auto d = make();
    d.press (ms (10));
    CHECK_EQ (d.nextDeadline(), ms (10) + ms (1000));
    CHECK (! d.poll (ms (10) + ms (1000) - 1).has_value());
    auto e = d.poll (ms (10) + ms (1000));
    CHECK (e.has_value());
    CHECK (e->type == GestureEvent::Type::Hold);
    CHECK_EQ (e->count, 1);
    CHECK_EQ (e->time, ms (10) + ms (1000));
    CHECK (! d.poll (ms (5000)).has_value()); // fires once
    CHECK_EQ (d.nextDeadline(), kNever);
}

TEST(gesture_release_cancels_hold)
{
    auto d = make();
    d.press (0);
    d.release (ms (999));
    CHECK (! d.poll (ms (2000)).has_value());
}

TEST(gesture_double_tap_and_hold_reports_count_two)
{
    auto d = make();
    d.press (0);
    d.release (ms (50));
    CHECK_EQ (d.press (ms (150))->count, 2);
    auto e = d.poll (ms (150) + ms (1000));
    CHECK (e.has_value());
    CHECK (e->type == GestureEvent::Type::Hold);
    CHECK_EQ (e->count, 2);
}

TEST(gesture_press_while_down_is_ignored)
{
    auto d = make();
    CHECK (d.press (0).has_value());
    CHECK (! d.press (ms (100)).has_value()); // repeated "pressed" message
}

TEST(gesture_debounce_ignores_very_close_presses)
{
    auto d = make();
    CHECK (d.press (0).has_value());
    d.release (ms (5));
    CHECK (! d.press (ms (20)).has_value()); // closer than 30 ms
    CHECK (d.press (ms (30)).has_value());
}

TEST(gesture_custom_timings_are_used)
{
    Timings t;
    t.multiTapMs = 150;
    t.holdMs = 400;
    auto d = make (SwitchMode::Momentary, t);
    d.press (0);
    d.release (ms (40));
    CHECK_EQ (d.press (ms (150))->count, 2);
    CHECK_EQ (d.nextDeadline(), ms (150) + ms (400));
}
