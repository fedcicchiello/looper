#include "Looper.h"

namespace looper
{

void Looper::prepare (double sampleRate, int inChannels, int outChannels, double maxLoopSeconds)
{
    sampleRate_ = sampleRate;
    inChannels_ = std::clamp (inChannels, 1, LoopEngine::kMaxChannels);
    outChannels_ = std::clamp (outChannels, 1, LoopEngine::kMaxChannels);

    engine_.prepare (inChannels_, sampleRate, static_cast<int64_t> (std::llround (maxLoopSeconds * sampleRate)));
    configure (Timings {}, SwitchMode::Momentary);

    detector_.reset();
    state_ = State::Empty;
    now_ = 0;
    cursor_ = 0;
    wasBypassed_ = false;
    publishStatus();
}

void Looper::configure (const Timings& timings, SwitchMode mode)
{
    timings_ = TimingSamples::make (timings, sampleRate_);
    engine_.setTimings (timings_);
    detector_.configure (timings_, mode);
}

Looper::Status Looper::status() const
{
    Status s;
    s.state = static_cast<State> (pubState_.load (std::memory_order_relaxed));
    s.loopLength = pubLoopLength_.load (std::memory_order_relaxed);
    s.position = pubPosition_.load (std::memory_order_relaxed);
    s.recordedFrames = pubRecorded_.load (std::memory_order_relaxed);
    s.sampleRate = sampleRate_;
    return s;
}

void Looper::publishStatus()
{
    pubState_.store (static_cast<int> (state_), std::memory_order_relaxed);
    pubLoopLength_.store (engine_.loopLength(), std::memory_order_relaxed);
    pubPosition_.store (engine_.position(), std::memory_order_relaxed);
    pubRecorded_.store (engine_.recordedFrames(), std::memory_order_relaxed);
}

void Looper::apply (const Transition& t)
{
    engine_.execute (t.command);
    state_ = t.next;
}

void Looper::handle (const GestureEvent& e)
{
    if (e.type == GestureEvent::Type::Tap)
        apply (onTap (state_, e.count, engine_.recordedFrames(), timings_.minLoop));
    else
        apply (onHold (state_));
}

void Looper::deliverHolds (int64_t time)
{
    while (auto e = detector_.poll (time))
        handle (*e);
}

void Looper::runUntil (int target)
{
    while (cursor_ < target)
    {
        int segmentEnd = target;

        const int64_t deadline = detector_.nextDeadline();
        if (deadline != kNever)
        {
            const int64_t due = deadline - now_;
            if (due <= cursor_)
            {
                deliverHolds (now_ + cursor_);
                continue;
            }
            if (due < segmentEnd)
                segmentEnd = static_cast<int> (due);
        }

        const float* inPtr[LoopEngine::kMaxChannels];
        float* outPtr[LoopEngine::kMaxChannels];
        for (int c = 0; c < inChannels_; ++c)
            inPtr[c] = in_[c] + cursor_;
        for (int c = 0; c < outChannels_; ++c)
            outPtr[c] = out_[c] + cursor_;

        const int done = engine_.process (inPtr, inChannels_, outPtr, outChannels_, segmentEnd - cursor_);
        cursor_ += done;

        if (engine_.consumeAutoClosed())
            apply (onRecordingFull (state_));
    }
}

void Looper::process (const float* const* in, float* const* out, int numFrames,
                      const SwitchEvent* events, int numEvents, bool bypassed)
{
    in_ = in;
    out_ = out;
    cursor_ = 0;

    if (bypassed && ! wasBypassed_)
    {
        apply (onBypass (state_, engine_.recordedFrames(), timings_.minLoop));
        detector_.reset();
    }
    wasBypassed_ = bypassed;

    if (! bypassed)
    {
        for (int i = 0; i < numEvents; ++i)
        {
            const SwitchEvent& ev = events[i];
            runUntil (std::clamp (ev.offset, 0, numFrames));

            const int64_t t = now_ + cursor_;
            deliverHolds (t);

            if (ev.pressed)
            {
                if (auto g = detector_.press (t))
                    handle (*g);
            }
            else
            {
                detector_.release (t);
            }
        }
    }

    runUntil (numFrames);

    now_ += numFrames;
    publishStatus();
}

} // namespace looper
