#pragma once

#include "StateMachine.h"
#include "Types.h"

#include <vector>

namespace looper
{

// Owns the loop memory and renders audio. Executes Commands at exact sample boundaries.
//
// Real-time rules: everything except prepare() is allocation-free and lock-free.
//
// Output = dry + loop * playGain. The dry signal always passes through at unity.
// Fades of `fade` samples are applied on every start/stop of playback and overdub.
//
// Seam: when a take is closed, the engine keeps recording `fade` more frames past the end
// (the "tail") while playback starts from the top. Once the tail is complete the first
// `fade` frames of the loop are crossfaded from the tail into the original head, so the
// wrap point from the last frame to the first is continuous.
//
// Overdub safety: for the first `multiTap + 1` frames of an overdub, the new material goes
// into a side buffer and is mixed into the loop only after that window has passed. A
// second tap inside the window discards the side buffer, leaving the loop bit-identical.
class LoopEngine
{
public:
    static constexpr int kMaxChannels = 2;

    // Not real-time safe: allocates all memory. Clears the loop.
    void prepare (int channels, double sampleRate, int64_t maxLoopFrames);

    // Real-time safe. Takes effect from now on.
    void setTimings (const TimingSamples& timings);

    void execute (Command command);

    // Renders `numFrames`. Normally returns numFrames. It returns early (having rendered the
    // frames up to and including the one that filled the buffer) when the take reaches its
    // maximum length; the engine has then closed the loop by itself and
    // consumeAutoClosed() returns true once.
    // `in` and `out` may alias. Layouts: loop channels == inChannels; outChannels 1 or 2.
    int process (const float* const* in, int inChannels, float* const* out, int outChannels, int numFrames);

    bool consumeAutoClosed();

    int64_t loopLength() const { return len_; }
    int64_t position() const { return pos_; }
    int64_t recordedFrames() const { return mode_ == Mode::Recording ? rec_ : 0; }
    int channels() const { return channels_; }
    const float* loopData (int channel) const { return buf_[static_cast<size_t> (channel)].data(); }
    bool sideBufferPending() const { return sideActive_; }

private:
    enum class Mode { Idle, Recording, Playing };

    void closeLoop (bool play);
    void commitSide();
    void blendSeam();
    void finishStop();
    void resetAll();

    int channels_ = 1;
    int64_t maxLoop_ = 0;
    int64_t fadeCapacity_ = 0;
    int64_t sideCapacity_ = 0;
    TimingSamples timings_;
    float fadeStep_ = 1.0f;

    std::vector<std::vector<float>> buf_;  // maxLoop + fadeCapacity (room for the seam tail)
    std::vector<std::vector<float>> side_; // overdub side buffer

    Mode mode_ = Mode::Idle;
    int64_t len_ = 0;
    int64_t pos_ = 0;
    int64_t rec_ = 0;

    float playGain_ = 0.0f, playTarget_ = 0.0f;
    float ovdGain_ = 0.0f, ovdTarget_ = 0.0f;
    bool clearPending_ = false;

    int64_t tailTotal_ = 0, tailWritten_ = 0;

    bool sideActive_ = false;
    int64_t sideStartPos_ = 0, sideLen_ = 0, sideWritten_ = 0;

    bool autoClosed_ = false;
};

} // namespace looper
