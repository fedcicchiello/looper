#include "LoopEngine.h"

#include <cstring>

namespace looper
{

void LoopEngine::prepare (int channels, double sampleRate, int64_t maxLoopFrames)
{
    channels_ = std::clamp (channels, 1, kMaxChannels);
    maxLoop_ = std::max<int64_t> (1, maxLoopFrames);
    fadeCapacity_ = static_cast<int64_t> (std::ceil (Timings::kFadeMax * sampleRate / 1000.0)) + 1;
    sideCapacity_ = static_cast<int64_t> (std::ceil (Timings::kMultiTapMax * sampleRate / 1000.0)) + 2;

    buf_.assign (static_cast<size_t> (channels_), std::vector<float> (static_cast<size_t> (maxLoop_ + fadeCapacity_), 0.0f));
    side_.assign (static_cast<size_t> (channels_), std::vector<float> (static_cast<size_t> (sideCapacity_), 0.0f));

    setTimings (TimingSamples::make (Timings {}, sampleRate));
    resetAll();
}

void LoopEngine::setTimings (const TimingSamples& timings)
{
    timings_ = timings;
    timings_.fade = std::clamp<int64_t> (timings_.fade, 1, std::max<int64_t> (1, fadeCapacity_));
    fadeStep_ = 1.0f / static_cast<float> (timings_.fade);
}

void LoopEngine::resetAll()
{
    mode_ = Mode::Idle;
    len_ = pos_ = rec_ = 0;
    playGain_ = playTarget_ = ovdGain_ = ovdTarget_ = 0.0f;
    clearPending_ = false;
    tailTotal_ = tailWritten_ = 0;
    sideActive_ = false;
    sideStartPos_ = sideLen_ = sideWritten_ = 0;
    autoClosed_ = false;
}

bool LoopEngine::consumeAutoClosed()
{
    const bool r = autoClosed_;
    autoClosed_ = false;
    return r;
}

void LoopEngine::closeLoop (bool play)
{
    len_ = std::max<int64_t> (1, rec_);
    rec_ = 0;
    pos_ = 0;

    if (play)
    {
        mode_ = Mode::Playing;
        playGain_ = 0.0f;
        playTarget_ = 1.0f;
        tailTotal_ = timings_.fade;
        tailWritten_ = 0;
    }
    else
    {
        mode_ = Mode::Idle;
        playGain_ = playTarget_ = 0.0f;
        tailTotal_ = tailWritten_ = 0;
    }
}

void LoopEngine::commitSide()
{
    if (! sideActive_)
        return;

    for (int c = 0; c < channels_; ++c)
    {
        float* dst = buf_[static_cast<size_t> (c)].data();
        const float* src = side_[static_cast<size_t> (c)].data();
        int64_t p = sideStartPos_;
        for (int64_t k = 0; k < sideWritten_; ++k)
        {
            dst[p] += src[k];
            if (++p >= len_)
                p = 0;
        }
    }
    sideActive_ = false;
    sideWritten_ = 0;
}

void LoopEngine::blendSeam()
{
    // First `fade` frames of the loop: crossfade from the tail (the audio that would have
    // followed the last frame) into the original head.
    const int64_t f = tailTotal_;
    for (int c = 0; c < channels_; ++c)
    {
        float* b = buf_[static_cast<size_t> (c)].data();
        const float* tail = b + len_;
        for (int64_t i = 0; i < f; ++i)
        {
            const float w = static_cast<float> (i) / static_cast<float> (f);
            b[i] = b[i] * w + tail[i] * (1.0f - w);
        }
    }
}

void LoopEngine::finishStop()
{
    mode_ = Mode::Idle;
    pos_ = 0;
    playGain_ = playTarget_ = 0.0f;
    ovdGain_ = ovdTarget_ = 0.0f;
    tailTotal_ = tailWritten_ = 0;
    sideActive_ = false;
    sideWritten_ = 0;
    if (clearPending_)
    {
        clearPending_ = false;
        len_ = 0;
        rec_ = 0;
    }
}

void LoopEngine::execute (Command command)
{
    switch (command)
    {
        case Command::None:
            break;

        case Command::StartRecording:
            resetAll();
            mode_ = Mode::Recording;
            break;

        case Command::CloseLoop:
            if (mode_ == Mode::Recording)
                closeLoop (true);
            break;

        case Command::CloseLoopStopped:
            if (mode_ == Mode::Recording)
                closeLoop (false);
            break;

        case Command::StartPlayback:
            if (len_ > 0)
            {
                mode_ = Mode::Playing;
                pos_ = 0;
                playTarget_ = 1.0f;
                clearPending_ = false;
                ovdTarget_ = 0.0f;
            }
            break;

        case Command::StartOverdub:
            if (mode_ == Mode::Playing && len_ > 0)
            {
                ovdTarget_ = 1.0f;
                sideActive_ = true;
                sideStartPos_ = pos_;
                sideLen_ = std::min (timings_.multiTap + 1, sideCapacity_);
                sideWritten_ = 0;
                for (auto& s : side_)
                    std::memset (s.data(), 0, static_cast<size_t> (sideLen_) * sizeof (float));
            }
            break;

        case Command::EndOverdub:
            commitSide();
            ovdTarget_ = 0.0f;
            break;

        case Command::Stop:
            if (mode_ == Mode::Playing)
            {
                commitSide();
                ovdTarget_ = 0.0f;
                playTarget_ = 0.0f;
                tailTotal_ = tailWritten_ = 0; // an unfinished seam tail is abandoned
            }
            break;

        case Command::StopDiscardOverdub:
            if (mode_ == Mode::Playing)
            {
                sideActive_ = false;
                sideWritten_ = 0;
                ovdGain_ = ovdTarget_ = 0.0f;
                playTarget_ = 0.0f;
                tailTotal_ = tailWritten_ = 0;
            }
            break;

        case Command::Clear:
            if (mode_ == Mode::Playing)
            {
                sideActive_ = false;
                sideWritten_ = 0;
                ovdGain_ = ovdTarget_ = 0.0f;
                playTarget_ = 0.0f;
                tailTotal_ = tailWritten_ = 0;
                clearPending_ = true; // forget the loop once the fade-out has finished
            }
            else
            {
                resetAll();
            }
            break;
    }
}

int LoopEngine::process (const float* const* in, int inChannels, float* const* out, int outChannels, int numFrames)
{
    const int inCh = std::max (1, inChannels);

    for (int i = 0; i < numFrames; ++i)
    {
        float dry[kMaxChannels] = { 0.0f, 0.0f };
        for (int c = 0; c < channels_; ++c)
            dry[c] = in[std::min (c, inCh - 1)][i];

        float loop[kMaxChannels] = { 0.0f, 0.0f };
        bool fullNow = false;

        if (mode_ == Mode::Recording)
        {
            for (int c = 0; c < channels_; ++c)
                buf_[static_cast<size_t> (c)][static_cast<size_t> (rec_)] = dry[c];
            if (++rec_ >= maxLoop_)
                fullNow = true;
        }
        else if (mode_ == Mode::Playing)
        {
            if (playGain_ < playTarget_)
                playGain_ = std::min (playTarget_, playGain_ + fadeStep_);
            else if (playGain_ > playTarget_)
                playGain_ = std::max (playTarget_, playGain_ - fadeStep_);

            for (int c = 0; c < channels_; ++c)
                loop[c] = buf_[static_cast<size_t> (c)][static_cast<size_t> (pos_)] * playGain_;

            if (ovdGain_ < ovdTarget_)
                ovdGain_ = std::min (ovdTarget_, ovdGain_ + fadeStep_);
            else if (ovdGain_ > ovdTarget_)
                ovdGain_ = std::max (ovdTarget_, ovdGain_ - fadeStep_);

            if (ovdGain_ > 0.0f)
            {
                for (int c = 0; c < channels_; ++c)
                {
                    if (sideActive_)
                        side_[static_cast<size_t> (c)][static_cast<size_t> (sideWritten_)] += dry[c] * ovdGain_;
                    else
                        buf_[static_cast<size_t> (c)][static_cast<size_t> (pos_)] += dry[c] * ovdGain_;
                }
            }

            if (tailWritten_ < tailTotal_)
            {
                for (int c = 0; c < channels_; ++c)
                    buf_[static_cast<size_t> (c)][static_cast<size_t> (len_ + tailWritten_)] = dry[c];
                if (++tailWritten_ == tailTotal_)
                    blendSeam();
            }

            if (++pos_ >= len_)
                pos_ = 0;

            if (sideActive_ && ++sideWritten_ >= sideLen_)
                commitSide();

            if (playTarget_ == 0.0f && playGain_ == 0.0f)
                finishStop();
        }

        // Dry passes through at unity; the loop is added on top. Computed from the saved
        // input values (dry[]), so in-place processing is safe. A mono source feeds both
        // outputs when the output is stereo.
        for (int o = 0; o < outChannels; ++o)
        {
            const int src = std::min (o, channels_ - 1);
            out[o][i] = dry[src] + loop[src];
        }

        if (fullNow)
        {
            closeLoop (true);
            autoClosed_ = true;
            return i + 1;
        }
    }
    return numFrames;
}

} // namespace looper
