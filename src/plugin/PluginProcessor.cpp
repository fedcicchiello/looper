#include "PluginProcessor.h"
#include "PluginEditor.h"

using looper::SwitchMode;

juce::String LooperProcessor::Mapping::describe (bool anyChannel) const
{
    if (type == Type::None)
        return "Not mapped";

    juce::String s = (type == Type::Cc ? "CC " : "Note ") + juce::String (number);
    s << (anyChannel ? ", any channel" : ", ch " + juce::String (channel));
    if (invert)
        s << ", inverted";
    return s;
}

juce::String LooperProcessor::lastMidiDescription() const
{
    const uint32_t v = lastMidi_.load();
    const uint32_t kind = v & 0x3;
    if (kind == 0)
        return {};

    const int number = static_cast<int> ((v >> 2) & 0x7f);
    const int value = static_cast<int> ((v >> 9) & 0x7f);
    const int channel = static_cast<int> ((v >> 16) & 0xf);
    const char* name = kind == 1 ? "CC " : kind == 2 ? "Note On " : "Note Off ";
    return juce::String (name) + juce::String (number) + " = " + juce::String (value) + "  (ch " + juce::String (channel) + ")";
}

LooperProcessor::LooperProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    addParameter (footswitchParam = new juce::AudioParameterBool ({ "footswitch", 1 }, "Footswitch", false));
    addParameter (bypassParam = new juce::AudioParameterBool ({ "bypass", 1 }, "Bypass", false));
    for (auto& p : inputPeak_)
        p = 0.0f;
}

bool LooperProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    const bool inOk = in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
    const bool outOk = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();

    // Mono or stereo in and out; stereo in with mono out is deliberately not supported.
    return inOk && outOk && ! (in == juce::AudioChannelSet::stereo() && out == juce::AudioChannelSet::mono());
}

void LooperProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    numIn_ = std::clamp (getTotalNumInputChannels(), 1, 2);
    numOut_ = std::clamp (getTotalNumOutputChannels(), 1, 2);

    // Re-preparing clears the loop, so only do it when the rate or the layout really changed.
    if (std::abs (sampleRate - preparedRate_) > 0.5 || numIn_ != preparedIn_ || numOut_ != preparedOut_)
    {
        looper_.prepare (sampleRate, numIn_, numOut_);
        preparedRate_ = sampleRate;
        preparedIn_ = numIn_;
        preparedOut_ = numOut_;
    }

    lastFootswitch_ = footswitchParam->get(); // a restored "on" value must not count as a press
}

void LooperProcessor::pushSwitchEvent (bool pressed)
{
    int start1, size1, start2, size2;
    guiFifo_.prepareToWrite (1, start1, size1, start2, size2);
    if (size1 > 0)
    {
        guiFifoData_[static_cast<size_t> (start1)] = pressed;
        guiFifo_.finishedWrite (1);
    }
}

bool LooperProcessor::handleMidiMessage (const juce::MidiMessage& m, SwitchMode mode, bool& pressed) const
{
    const Mapping map = getMapping();
    if (map.type == Mapping::Type::None)
        return false;
    if (! anyChannel_ && m.getChannel() != map.channel)
        return false;

    if (map.type == Mapping::Type::Cc)
    {
        if (! m.isController() || m.getControllerNumber() != map.number)
            return false;
        // Momentary: >= 64 is down, below is up (the other way round if the switch is inverted).
        // Latch: every message is a tap.
        pressed = (mode == SwitchMode::Latch) || ((m.getControllerValue() >= 64) != map.invert);
        return true;
    }

    // Note mapping
    if (m.getNoteNumber() != map.number)
        return false;
    if (m.isNoteOn())
    {
        pressed = true;
        return true;
    }
    if (m.isNoteOff()) // includes Note On with velocity 0
    {
        pressed = (mode == SwitchMode::Latch);
        return true;
    }
    return false;
}

void LooperProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    render (buffer, midi, bypassParam->get());
}

void LooperProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    render (buffer, midi, true);
}

void LooperProcessor::render (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, bool bypassed)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() < std::max (numIn_, numOut_))
        return;

    float* const* channels = buffer.getArrayOfWritePointers();

    // Input meter
    for (int c = 0; c < numIn_; ++c)
    {
        const float peak = buffer.getMagnitude (c, 0, numSamples);
        auto& slot = inputPeak_[static_cast<size_t> (c)];
        if (peak > slot.load (std::memory_order_relaxed))
            slot.store (peak, std::memory_order_relaxed);
    }

    const SwitchMode mode = switchMode_.load();
    looper_.configure (timings(), mode);

    // Collect switch events, sorted by offset: host parameter and GUI first (offset 0), then MIDI.
    looper::Looper::SwitchEvent events[kMaxEvents];
    int count = 0;
    auto add = [&] (int offset, bool pressed) {
        if (count < kMaxEvents)
            events[count++] = { offset, pressed };
        if (mode == SwitchMode::Momentary)
            switchDown_ = pressed;
    };

    const bool fs = footswitchParam->get();
    if (fs != lastFootswitch_)
    {
        lastFootswitch_ = fs;
        add (0, fs);
    }

    {
        int start1, size1, start2, size2;
        guiFifo_.prepareToRead (guiFifo_.getNumReady(), start1, size1, start2, size2);
        for (int i = 0; i < size1; ++i)
            add (0, guiFifoData_[static_cast<size_t> (start1 + i)]);
        for (int i = 0; i < size2; ++i)
            add (0, guiFifoData_[static_cast<size_t> (start2 + i)]);
        guiFifo_.finishedRead (size1 + size2);
    }

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        midiActivity_.fetch_add (1, std::memory_order_relaxed);

        {
            uint32_t kind = 0, number = 0, value = 0;
            if (msg.isController())
                kind = 1, number = static_cast<uint32_t> (msg.getControllerNumber()), value = static_cast<uint32_t> (msg.getControllerValue());
            else if (msg.isNoteOn())
                kind = 2, number = static_cast<uint32_t> (msg.getNoteNumber()), value = static_cast<uint32_t> (msg.getVelocity());
            else if (msg.isNoteOff())
                kind = 3, number = static_cast<uint32_t> (msg.getNoteNumber()), value = 0;
            if (kind != 0)
                lastMidi_ = kind | ((number & 0x7f) << 2) | ((value & 0x7f) << 9) | (static_cast<uint32_t> (msg.getChannel() & 0xf) << 16);
        }

        if (learning_.load())
        {
            Mapping learned;
            if (msg.isController())
            {
                // The message that arrives while learning is the press. A low value means the
                // switch is wired inverted (low on press, high on release).
                learned = { Mapping::Type::Cc, msg.getControllerNumber(), msg.getChannel(), msg.getControllerValue() < 64 };
            }
            else if (msg.isNoteOn())
                learned = { Mapping::Type::Note, msg.getNoteNumber(), msg.getChannel() };

            if (learned.type != Mapping::Type::None)
            {
                mapping_ = learned.pack();
                learning_ = false;
            }
            continue; // the learning message itself is not a footswitch press
        }

        bool pressed = false;
        if (! bypassed && handleMidiMessage (msg, mode, pressed))
            add (std::clamp (meta.samplePosition, 0, numSamples - 1), pressed);
    }

    if (mode == SwitchMode::Latch)
        switchDown_ = false;

    const float* inPtr[2] = { channels[0], numIn_ > 1 ? channels[1] : nullptr };
    float* outPtr[2] = { channels[0], numOut_ > 1 ? channels[1] : nullptr };
    looper_.process (inPtr, outPtr, numSamples, events, count, bypassed);
}

juce::AudioProcessorEditor* LooperProcessor::createEditor()
{
    return new LooperEditor (*this);
}

void LooperProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Only settings are saved. The recorded audio never is.
    juce::XmlElement xml ("LooperState");
    xml.setAttribute ("version", 1);

    const Mapping m = getMapping();
    xml.setAttribute ("mapType", static_cast<int> (m.type));
    xml.setAttribute ("mapNumber", m.number);
    xml.setAttribute ("mapChannel", m.channel);
    xml.setAttribute ("mapInvert", m.invert);
    xml.setAttribute ("anyChannel", anyChannel_.load());
    xml.setAttribute ("latch", switchMode_.load() == SwitchMode::Latch);
    xml.setAttribute ("multiTapMs", multiTapMs_.load());
    xml.setAttribute ("holdMs", holdMs_.load());
    xml.setAttribute ("fadeMs", fadeMs_.load());

    copyXmlToBinary (xml, destData);
}

void LooperProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName ("LooperState"))
        return;

    Mapping m;
    m.type = static_cast<Mapping::Type> (std::clamp (xml->getIntAttribute ("mapType", 0), 0, 2));
    m.number = xml->getIntAttribute ("mapNumber", 0);
    m.channel = std::clamp (xml->getIntAttribute ("mapChannel", 1), 1, 16);
    m.invert = xml->getBoolAttribute ("mapInvert", false);
    mapping_ = m.pack();

    anyChannel_ = xml->getBoolAttribute ("anyChannel", false);
    switchMode_ = xml->getBoolAttribute ("latch", false) ? SwitchMode::Latch : SwitchMode::Momentary;
    setTimings ({ xml->getDoubleAttribute ("multiTapMs", looper::Timings::kMultiTapDefault),
                  xml->getDoubleAttribute ("holdMs", looper::Timings::kHoldDefault),
                  xml->getDoubleAttribute ("fadeMs", looper::Timings::kFadeDefault) });
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LooperProcessor();
}
