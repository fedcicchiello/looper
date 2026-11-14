#include "GestureDetector.h"

namespace looper
{

void GestureDetector::configure (const TimingSamples& timings, SwitchMode mode)
{
    const bool modeChanged = mode != mode_;
    timings_ = timings;
    mode_ = mode;
    if (modeChanged)
        reset();
}

void GestureDetector::reset()
{
    havePress_ = false;
    lastPress_ = 0;
    lastCount_ = 0;
    down_ = false;
    holdArmed_ = false;
    holdDue_ = 0;
}

std::optional<GestureEvent> GestureDetector::press (int64_t time)
{
    // A repeated "pressed" message while the switch is already down is not a new tap.
    if (mode_ == SwitchMode::Momentary && down_)
        return std::nullopt;

    if (havePress_ && time - lastPress_ < timings_.debounce)
        return std::nullopt;

    int count = (havePress_ && time - lastPress_ <= timings_.multiTap) ? lastCount_ + 1 : 1;
    const int maxCount = (mode_ == SwitchMode::Latch) ? 3 : 2;
    if (count > maxCount)
        count = 1;

    havePress_ = true;
    lastPress_ = time;
    lastCount_ = count;

    if (mode_ == SwitchMode::Momentary)
    {
        down_ = true;
        holdArmed_ = true;
        holdDue_ = time + timings_.hold;
    }

    return GestureEvent { GestureEvent::Type::Tap, count, time };
}

void GestureDetector::release (int64_t /*time*/)
{
    down_ = false;
    holdArmed_ = false;
}

std::optional<GestureEvent> GestureDetector::poll (int64_t time)
{
    if (holdArmed_ && down_ && time >= holdDue_)
    {
        holdArmed_ = false;
        return GestureEvent { GestureEvent::Type::Hold, lastCount_, holdDue_ };
    }
    return std::nullopt;
}

int64_t GestureDetector::nextDeadline() const
{
    return (holdArmed_ && down_) ? holdDue_ : kNever;
}

} // namespace looper
