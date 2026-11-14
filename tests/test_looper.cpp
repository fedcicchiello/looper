#include "TestUtil.h"

#include "Looper.h"

#include <cstring>
#include <functional>

using namespace looper;

namespace
{
constexpr double kSr = 48000.0;
constexpr double kPi = 3.14159265358979323846;
int64_t ms (double v) { return static_cast<int64_t> (std::llround (v * kSr / 1000.0)); }

// Drives a Looper like a host would: fixed-size blocks, switch events at exact sample times.
struct Sim
{
    Looper looper;
    int inCh, outCh, block;
    int64_t t = 0;
    bool bypassed = false;
    std::vector<std::pair<int64_t, bool>> pending;
    std::function<float (int64_t, int)> input = [] (int64_t, int) { return 0.0f; };
    std::vector<std::vector<float>> out; // captured output per channel, index == absolute time

    Sim (int in = 1, int outc = 1, int blockSize = 64, double maxLoopSeconds = 300.0,
         SwitchMode mode = SwitchMode::Momentary, Timings timings = {})
        : inCh (in), outCh (outc), block (blockSize), out (static_cast<size_t> (outc))
    {
        looper.prepare (kSr, in, outc, maxLoopSeconds);
        looper.configure (timings, mode);
    }

    void press() { pending.push_back ({ t, true }); }
    void release() { pending.push_back ({ t, false }); }

    void runFrames (int64_t n)
    {
        std::vector<std::vector<float>> inBuf (static_cast<size_t> (inCh)), outBuf (static_cast<size_t> (outCh));
        while (n > 0)
        {
            const int len = static_cast<int> (std::min<int64_t> (block, n));
            std::vector<Looper::SwitchEvent> events;
            for (auto it = pending.begin(); it != pending.end();)
            {
                if (it->first < t + len)
                {
                    events.push_back ({ static_cast<int> (it->first - t), it->second });
                    it = pending.erase (it);
                }
                else
                    ++it;
            }

            std::vector<const float*> inP;
            std::vector<float*> outP;
            for (int c = 0; c < inCh; ++c)
            {
                inBuf[static_cast<size_t> (c)].resize (static_cast<size_t> (len));
                for (int i = 0; i < len; ++i)
                    inBuf[static_cast<size_t> (c)][static_cast<size_t> (i)] = input (t + i, c);
                inP.push_back (inBuf[static_cast<size_t> (c)].data());
            }
            for (int c = 0; c < outCh; ++c)
            {
                outBuf[static_cast<size_t> (c)].assign (static_cast<size_t> (len), 0.0f);
                outP.push_back (outBuf[static_cast<size_t> (c)].data());
            }

            looper.process (inP.data(), outP.data(), len, events.data(), static_cast<int> (events.size()), bypassed);

            for (int c = 0; c < outCh; ++c)
                out[static_cast<size_t> (c)].insert (out[static_cast<size_t> (c)].end(), outBuf[static_cast<size_t> (c)].begin(), outBuf[static_cast<size_t> (c)].end());

            t += len;
            n -= len;
        }
    }

    void run (double milliseconds) { runFrames (ms (milliseconds)); }

    void tap (double holdMs = 60.0)
    {
        press();
        run (holdMs);
        release();
    }

    State state() const { return looper.status().state; }
    int64_t loopLength() const { return looper.status().loopLength; }
};

float sine (int64_t t, double freq = 441.3, double amp = 0.5)
{
    return static_cast<float> (amp * std::sin (2.0 * kPi * freq * static_cast<double> (t) / kSr));
}

float noise (int64_t t)
{
    uint32_t x = static_cast<uint32_t> (t) * 2654435761u;
    x ^= x >> 15;
    x *= 2246822519u;
    x ^= x >> 13;
    return (static_cast<float> (x & 0xffff) / 32768.0f - 1.0f) * 0.3f;
}

struct RecordedLoop
{
    int64_t start = 0, close = 0, length = 0;
};

// Records `seconds` of a sine, closes the loop, then silences the input. Leaves the sim playing.
RecordedLoop recordSine (Sim& s, double seconds = 1.0)
{
    s.input = [] (int64_t t, int) { return sine (t); };
    RecordedLoop r;
    r.start = s.t;
    s.tap();
    s.run (seconds * 1000.0 - 60.0);
    r.close = s.t;
    s.tap();
    s.run (40);
    s.input = [] (int64_t, int) { return 0.0f; };
    r.length = s.loopLength();
    return r;
}
} // namespace

TEST(looper_record_then_play_reproduces_the_take)
{
    Sim s;
    const auto r = recordSine (s);
    CHECK (s.state() == State::Playing);
    CHECK_EQ (r.length, r.close - r.start);
    CHECK_EQ (r.length, ms (1000));

    s.run (3000);

    // Second pass of the loop: input is silent, so the output is the loop alone.
    const auto& o = s.out[0];
    int bad = 0;
    for (int64_t n = r.close + r.length + 300; n < r.close + 2 * r.length - 300; ++n)
    {
        const int64_t k = (n - r.close) % r.length;
        if (std::fabs (o[static_cast<size_t> (n)] - sine (r.start + k)) > 1e-6)
            ++bad;
    }
    CHECK_EQ (bad, 0);
}

TEST(looper_seam_is_continuous)
{
    // 441.3 Hz over 1 s is not a whole number of cycles, so a naive wrap would jump.
    Sim s;
    const auto r = recordSine (s);
    s.run (3000);

    const auto& o = s.out[0];
    const double maxStep = 0.5 * 2.0 * kPi * 441.3 / kSr; // largest sample-to-sample change of the sine
    double worst = 0.0;
    for (int64_t n = r.close + 2 * r.length - 50; n < r.close + 3 * r.length + 50; ++n)
        worst = std::max (worst, static_cast<double> (std::fabs (o[static_cast<size_t> (n)] - o[static_cast<size_t> (n - 1)])));
    CHECK (worst <= maxStep * 1.3);
}

TEST(looper_dry_always_passes_through)
{
    Sim s;
    s.input = [] (int64_t t, int) { return sine (t); };
    s.run (200);
    for (int64_t n = 0; n < s.t; ++n)
        CHECK_NEAR (s.out[0][static_cast<size_t> (n)], sine (n), 1e-7);
}

TEST(looper_cancelled_overdub_leaves_loop_bit_identical)
{
    Sim s;
    recordSine (s);
    s.run (3000);

    const auto& e = s.looper.engine();
    std::vector<float> before (e.loopData (0), e.loopData (0) + e.loopLength());

    s.input = [] (int64_t t, int) { return noise (t); };
    s.tap (40);     // starts an overdub
    s.run (100);
    s.tap();        // second tap inside the window: stop, discard the overdub
    s.run (200);

    CHECK (s.state() == State::Stopped);
    CHECK (! e.sideBufferPending());
    CHECK (std::memcmp (before.data(), e.loopData (0), before.size() * sizeof (float)) == 0);
}

TEST(looper_overdub_is_added_on_top_at_unity)
{
    Sim s;
    recordSine (s);
    s.run (3000);

    const auto& e = s.looper.engine();
    const int64_t len = e.loopLength();
    std::vector<float> before (e.loopData (0), e.loopData (0) + len);

    s.input = [] (int64_t, int) { return 0.1f; };
    const int64_t startPos = e.position();
    s.tap (40);
    s.run (460);       // overdub for ~500 ms
    s.tap (40);        // end the overdub (more than 300 ms after the first tap)
    s.run (200);
    CHECK (s.state() == State::Playing);

    int bad = 0;
    for (int64_t i = 600; i < ms (500) - 600; ++i)
    {
        const int64_t k = (startPos + i) % len;
        if (std::fabs (e.loopData (0)[k] - (before[static_cast<size_t> (k)] + 0.1f)) > 1e-6)
            ++bad;
    }
    CHECK_EQ (bad, 0);

    // Well outside the overdubbed span the loop is untouched.
    const int64_t far = (startPos + ms (800)) % len;
    CHECK (e.loopData (0)[far] == before[static_cast<size_t> (far)]);
}

TEST(looper_hold_fires_at_the_exact_sample)
{
    Sim s (1, 1, 1); // one-frame blocks
    recordSine (s);
    s.runFrames (ms (500)); // let the multi-tap window of the closing tap expire
    const int64_t t0 = s.t;
    s.press();      // first tap: starts an overdub, then held
    s.runFrames (ms (1000));
    CHECK (s.t == t0 + ms (1000));
    CHECK (s.state() == State::Overdubbing); // not yet: it fires at sample t0 + 1000 ms
    s.runFrames (1);
    CHECK (s.state() == State::Empty);
    s.run (20); // the loop fades out before it is forgotten
    CHECK_EQ (s.loopLength(), 0);
}

TEST(looper_double_tap_and_hold_clears_silently)
{
    Sim s;
    recordSine (s);
    s.run (500);

    s.press();
    s.run (50);
    s.release();
    s.run (100);
    const int64_t secondPress = s.t;
    s.press();      // second tap: stops immediately, the hold then clears
    s.run (1200);
    s.release();
    s.run (200);

    CHECK (s.state() == State::Empty);
    CHECK_EQ (s.loopLength(), 0);

    // From just after the stop fade until well past the clear, the output is exactly silent.
    for (int64_t n = secondPress + ms (5) + 2; n < s.t; ++n)
        CHECK (s.out[0][static_cast<size_t> (n)] == 0.0f);
}

TEST(looper_double_tap_stops_and_keeps_the_loop)
{
    Sim s;
    const auto r = recordSine (s);
    s.run (500);
    s.tap();
    s.run (100);
    s.tap();
    s.run (300);
    CHECK (s.state() == State::Stopped);
    CHECK_EQ (s.loopLength(), r.length);
    CHECK_EQ (s.looper.engine().position(), 0);

    s.tap();        // plays again from the top
    s.run (100);
    CHECK (s.state() == State::Playing);
}

TEST(looper_taps_in_the_minimum_loop_length_are_ignored)
{
    Sim s;
    s.tap();
    s.run (140);
    s.tap();        // 200 ms after the first: ignored
    s.run (100);
    CHECK (s.state() == State::Recording);
    s.run (300);
    s.tap();        // 600 ms in: closes the loop
    CHECK (s.state() == State::Playing);
}

TEST(looper_hold_while_recording_discards_the_take)
{
    Sim s;
    s.press();
    s.run (1100);
    CHECK (s.state() == State::Empty);
    s.release();
    s.run (100);
    CHECK_EQ (s.loopLength(), 0);
}

TEST(looper_recording_closes_itself_at_the_maximum_length)
{
    Sim s (1, 1, 64, 1.0); // 1 second maximum for the test
    s.tap();
    s.run (1500);
    CHECK (s.state() == State::Playing);
    CHECK_EQ (s.loopLength(), ms (1000));
}

TEST(looper_stereo_channels_stay_separate)
{
    Sim s (2, 2);
    s.input = [] (int64_t, int c) { return c == 0 ? 0.3f : -0.2f; };
    s.tap();
    s.run (940);
    s.tap();
    s.run (40);
    s.input = [] (int64_t, int) { return 0.0f; };
    s.run (2000);
    const int64_t n = s.t - ms (500);
    CHECK_NEAR (s.out[0][static_cast<size_t> (n)], 0.3, 1e-6);
    CHECK_NEAR (s.out[1][static_cast<size_t> (n)], -0.2, 1e-6);
}

TEST(looper_mono_input_feeds_both_stereo_outputs)
{
    Sim s (1, 2);
    s.input = [] (int64_t t, int) { return sine (t); };
    s.tap();
    s.run (940);
    s.tap();
    s.run (40);
    s.input = [] (int64_t, int) { return 0.0f; };
    s.run (2000);
    for (int64_t n = 0; n < s.t; ++n)
        CHECK (s.out[0][static_cast<size_t> (n)] == s.out[1][static_cast<size_t> (n)]);
    CHECK (std::fabs (s.out[0][static_cast<size_t> (s.t - ms (500))]) > 0.0f);
}

TEST(looper_bypass_stops_and_keeps_the_loop)
{
    Sim s;
    const auto r = recordSine (s);
    s.run (300);
    s.bypassed = true;
    s.run (100);
    CHECK (s.state() == State::Stopped);
    CHECK_EQ (s.loopLength(), r.length);

    s.tap();        // switch is ignored while bypassed
    s.run (100);
    CHECK (s.state() == State::Stopped);

    for (int64_t n = s.t - ms (50); n < s.t; ++n)
        CHECK (s.out[0][static_cast<size_t> (n)] == 0.0f);

    s.bypassed = false;
    s.run (100);
    CHECK (s.state() == State::Stopped);    // waits for a tap
    s.tap();
    s.run (50);
    CHECK (s.state() == State::Playing);
}

TEST(looper_latch_mode_triple_tap_clears)
{
    Sim s (1, 1, 64, 300.0, SwitchMode::Latch);
    s.press();                  // start recording
    s.run (700);
    CHECK (s.state() == State::Recording);
    s.press();                  // close the loop
    s.run (100);
    CHECK (s.state() == State::Playing);
    s.press();                  // 2nd tap of a sequence: stop
    s.run (100);
    CHECK (s.state() == State::Stopped);
    s.press();                  // 3rd tap: clear
    s.run (50);
    CHECK (s.state() == State::Empty);
    CHECK_EQ (s.loopLength(), 0);
}

TEST(looper_editable_timings_change_the_double_tap_window)
{
    Timings t;
    t.multiTapMs = 150;
    Sim s (1, 1, 64, 300.0, SwitchMode::Momentary, t);
    recordSine (s);
    s.run (500);
    s.tap (40);     // starts an overdub
    s.run (160);    // 200 ms between the presses: outside a 150 ms window
    s.tap (40);     // so this is a plain tap, which ends the overdub instead of stopping
    s.run (50);
    CHECK (s.state() == State::Playing);
}

TEST(looper_playback_fades_in_and_out_without_clicks)
{
    Sim s;
    s.input = [] (int64_t, int) { return 0.5f; }; // DC makes any discontinuity obvious
    s.tap();
    s.run (940);
    const int64_t close = s.t;
    s.tap();
    s.input = [] (int64_t, int) { return 0.0f; };
    s.run (1500);

    // On the stop, the loop output ramps down smoothly: no step bigger than the fade slope.
    s.tap();
    s.run (100);
    s.tap();
    s.run (200);
    const auto& o = s.out[0];
    double worst = 0.0;
    for (int64_t n = close + ms (1500) + 100; n + 1 < s.t; ++n)
        worst = std::max (worst, static_cast<double> (std::fabs (o[static_cast<size_t> (n + 1)] - o[static_cast<size_t> (n)])));
    CHECK (worst <= 0.5 / static_cast<double> (ms (5)) * 1.5);
}
