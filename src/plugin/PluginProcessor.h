#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Looper.h"

#include <array>
#include <atomic>

// The JUCE wrapper around looper::Looper. It only translates: MIDI / host parameter / GUI
// presses become Looper::SwitchEvents, and JUCE buffers become channel pointers.
class LooperProcessor : public juce::AudioProcessor
{
public:
    // A learned MIDI message that acts as the footswitch.
    struct Mapping
    {
        enum class Type { None = 0, Cc = 1, Note = 2 };

        Type type = Type::None;
        int number = 0;  // CC number or note number
        int channel = 1; // 1-16, ignored when the "any channel" option is on

        uint32_t pack() const
        {
            return static_cast<uint32_t> (type) | (static_cast<uint32_t> (number & 0x7f) << 2)
                   | (static_cast<uint32_t> ((channel - 1) & 0xf) << 9);
        }

        static Mapping unpack (uint32_t v)
        {
            Mapping m;
            m.type = static_cast<Type> (v & 0x3);
            m.number = static_cast<int> ((v >> 2) & 0x7f);
            m.channel = static_cast<int> ((v >> 9) & 0xf) + 1;
            return m;
        }

        juce::String describe (bool anyChannel) const;
    };

    LooperProcessor();

    // --- juce::AudioProcessor ---
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Looper"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- control surface used by the editor (all thread-safe) ---
    void pushSwitchEvent (bool pressed); // on-screen button
    looper::Looper::Status status() const { return looper_.status(); }

    void startLearn() { learning_ = true; }
    void cancelLearn() { learning_ = false; }
    bool isLearning() const { return learning_; }
    Mapping getMapping() const { return Mapping::unpack (mapping_.load()); }
    void clearMapping() { mapping_ = Mapping {}.pack(); }
    bool anyChannel() const { return anyChannel_; }
    void setAnyChannel (bool b) { anyChannel_ = b; }

    looper::SwitchMode switchMode() const { return switchMode_.load(); }
    void setSwitchMode (looper::SwitchMode m) { switchMode_ = m; }

    looper::Timings timings() const { return { multiTapMs_.load(), holdMs_.load(), fadeMs_.load() }; }
    void setTimings (const looper::Timings& t)
    {
        const auto c = t.clamped();
        multiTapMs_ = c.multiTapMs;
        holdMs_ = c.holdMs;
        fadeMs_ = c.fadeMs;
    }

    bool isSwitchDown() const { return switchDown_; }
    uint32_t midiActivityCount() const { return midiActivity_; }
    float takeInputPeak (int channel) { return inputPeak_[static_cast<size_t> (channel)].exchange (0.0f); }
    int numInputChannels() const { return numIn_; }

private:
    void render (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, bool bypassed);
    bool handleMidiMessage (const juce::MidiMessage& m, looper::SwitchMode mode, bool& pressed) const;

    static constexpr int kMaxEvents = 256;

    looper::Looper looper_;

    juce::AudioParameterBool* footswitchParam = nullptr;
    juce::AudioParameterBool* bypassParam = nullptr;
    bool lastFootswitch_ = false;

    juce::AbstractFifo guiFifo_ { 64 };
    std::array<bool, 64> guiFifoData_ {};

    std::atomic<uint32_t> mapping_ { 0 };
    std::atomic<bool> anyChannel_ { false };
    std::atomic<bool> learning_ { false };
    std::atomic<looper::SwitchMode> switchMode_ { looper::SwitchMode::Momentary };
    std::atomic<double> multiTapMs_ { looper::Timings::kMultiTapDefault };
    std::atomic<double> holdMs_ { looper::Timings::kHoldDefault };
    std::atomic<double> fadeMs_ { looper::Timings::kFadeDefault };

    std::atomic<bool> switchDown_ { false };
    std::atomic<uint32_t> midiActivity_ { 0 };
    std::array<std::atomic<float>, 2> inputPeak_ {};

    double preparedRate_ = 0.0;
    int numIn_ = 1, numOut_ = 1;
    int preparedIn_ = 0, preparedOut_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LooperProcessor)
};
