#include "TestUtil.h"

#include "StateMachine.h"

using namespace looper;

namespace
{
const int64_t kMinLoop = 24000;
Transition tap (State s, int count, int64_t rec = 0) { return onTap (s, count, rec, kMinLoop); }
} // namespace

TEST(state_single_taps_cycle)
{
    auto t = tap (State::Empty, 1);
    CHECK (t.next == State::Recording && t.command == Command::StartRecording);
    t = tap (State::Recording, 1, kMinLoop);
    CHECK (t.next == State::Playing && t.command == Command::CloseLoop);
    t = tap (State::Playing, 1);
    CHECK (t.next == State::Overdubbing && t.command == Command::StartOverdub);
    t = tap (State::Overdubbing, 1);
    CHECK (t.next == State::Playing && t.command == Command::EndOverdub);
    t = tap (State::Stopped, 1);
    CHECK (t.next == State::Playing && t.command == Command::StartPlayback);
}

TEST(state_tap_during_minimum_loop_is_ignored)
{
    auto t = tap (State::Recording, 1, kMinLoop - 1);
    CHECK (t.next == State::Recording && t.command == Command::None);
    t = tap (State::Recording, 2, 100);
    CHECK (t.next == State::Recording && t.command == Command::None);
}

TEST(state_double_tap_stops)
{
    auto t = tap (State::Playing, 2); // first tap started playback or ended an overdub
    CHECK (t.next == State::Stopped && t.command == Command::Stop);
    t = tap (State::Overdubbing, 2); // first tap started an overdub: throw it away
    CHECK (t.next == State::Stopped && t.command == Command::StopDiscardOverdub);
}

TEST(state_double_tap_has_no_effect_where_there_is_nothing_to_stop)
{
    CHECK (tap (State::Empty, 2).command == Command::None);
    CHECK (tap (State::Stopped, 2).command == Command::None);
}

TEST(state_hold_clears_from_every_state_with_content)
{
    for (State s : { State::Recording, State::Playing, State::Overdubbing, State::Stopped })
    {
        auto t = onHold (s);
        CHECK (t.next == State::Empty && t.command == Command::Clear);
    }
    CHECK (onHold (State::Empty).command == Command::None);
}

TEST(state_triple_tap_clears)
{
    auto t = tap (State::Stopped, 3);
    CHECK (t.next == State::Empty && t.command == Command::Clear);
    t = tap (State::Recording, 3);
    CHECK (t.next == State::Empty && t.command == Command::Clear);
    CHECK (tap (State::Empty, 3).command == Command::None);
}

TEST(state_bypass_behaves_like_stop)
{
    auto t = onBypass (State::Playing, 0, kMinLoop);
    CHECK (t.next == State::Stopped && t.command == Command::Stop);
    t = onBypass (State::Overdubbing, 0, kMinLoop);
    CHECK (t.next == State::Stopped && t.command == Command::Stop);
    t = onBypass (State::Recording, kMinLoop, kMinLoop);
    CHECK (t.next == State::Stopped && t.command == Command::CloseLoopStopped);
    t = onBypass (State::Recording, kMinLoop - 1, kMinLoop);
    CHECK (t.next == State::Empty && t.command == Command::Clear);
    CHECK (onBypass (State::Stopped, 0, kMinLoop).command == Command::None);
    CHECK (onBypass (State::Empty, 0, kMinLoop).command == Command::None);
}

TEST(state_recording_full_moves_to_playing)
{
    CHECK (onRecordingFull (State::Recording).next == State::Playing);
    CHECK (onRecordingFull (State::Stopped).next == State::Stopped);
}
