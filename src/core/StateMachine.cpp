#include "StateMachine.h"

namespace looper
{

Transition onTap (State state, int count, int64_t recordedFrames, int64_t minLoop)
{
    if (count >= 3) // latch mode only: triple-tap clears
        return state == State::Empty ? Transition { State::Empty, Command::None }
                                     : Transition { State::Empty, Command::Clear };

    if (count == 2)
    {
        switch (state)
        {
            case State::Overdubbing: return { State::Stopped, Command::StopDiscardOverdub };
            case State::Playing:     return { State::Stopped, Command::Stop };
            // Taps during the minimum loop length are ignored; Empty and Stopped have no
            // first tap to revise.
            default:                 return { state, Command::None };
        }
    }

    switch (state)
    {
        case State::Empty:
            return { State::Recording, Command::StartRecording };

        case State::Recording:
            if (recordedFrames < minLoop)
                return { State::Recording, Command::None };
            return { State::Playing, Command::CloseLoop };

        case State::Playing:
            return { State::Overdubbing, Command::StartOverdub };

        case State::Overdubbing:
            return { State::Playing, Command::EndOverdub };

        case State::Stopped:
            return { State::Playing, Command::StartPlayback };
    }
    return { state, Command::None };
}

Transition onHold (State state)
{
    if (state == State::Empty)
        return { State::Empty, Command::None };
    return { State::Empty, Command::Clear };
}

Transition onBypass (State state, int64_t recordedFrames, int64_t minLoop)
{
    switch (state)
    {
        case State::Recording:
            if (recordedFrames < minLoop)
                return { State::Empty, Command::Clear };
            return { State::Stopped, Command::CloseLoopStopped };

        case State::Playing:
        case State::Overdubbing:
            return { State::Stopped, Command::Stop };

        default:
            return { state, Command::None };
    }
}

Transition onRecordingFull (State state)
{
    if (state == State::Recording)
        return { State::Playing, Command::None }; // the engine has already closed the loop
    return { state, Command::None };
}

} // namespace looper
