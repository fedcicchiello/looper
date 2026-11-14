#include "PluginEditor.h"

using looper::State;
using looper::SwitchMode;
using looper::Timings;

namespace
{
const juce::Colour kBackground { 0xff15171a };
const juce::Colour kText { 0xffe5e7eb };
const juce::Colour kDim { 0xff9ca3af };

juce::Colour colourFor (State s)
{
    switch (s)
    {
        case State::Empty:       return juce::Colour (0xff4b5563);
        case State::Recording:   return juce::Colour (0xffdc2626);
        case State::Playing:     return juce::Colour (0xff16a34a);
        case State::Overdubbing: return juce::Colour (0xfff97316);
        case State::Stopped:     return juce::Colour (0xff166534).withMultipliedBrightness (0.9f);
    }
    return juce::Colours::grey;
}

const char* labelFor (State s)
{
    switch (s)
    {
        case State::Empty:       return "EMPTY";
        case State::Recording:   return "REC";
        case State::Playing:     return "PLAY";
        case State::Overdubbing: return "OVERDUB";
        case State::Stopped:     return "STOPPED";
    }
    return "";
}

juce::String formatTime (double seconds)
{
    const int minutes = static_cast<int> (seconds) / 60;
    return juce::String (minutes) + ":" + juce::String (seconds - minutes * 60, 1).paddedLeft ('0', 4);
}
} // namespace

// ---------------------------------------------------------------------------------------------
// Settings panel: MIDI Learn, switch type, the three editable timings, and (standalone) devices.
// ---------------------------------------------------------------------------------------------
struct LooperEditor::SettingsPanel : public juce::Component
{
    explicit SettingsPanel (LooperProcessor& p) : processor (p)
    {
        auto label = [this] (juce::Label& l, const juce::String& text) {
            l.setText (text, juce::dontSendNotification);
            l.setColour (juce::Label::textColourId, kText);
            addAndMakeVisible (l);
        };

        label (mappingLabel, "");
        label (modeLabel, "Switch type");
        label (multiLabel, "Double-tap window");
        label (holdLabel, "Hold time");
        label (fadeLabel, "Fade");

        addAndMakeVisible (learnButton);
        learnButton.onClick = [this] {
            if (processor.isLearning())
                processor.cancelLearn();
            else
                processor.startLearn();
            refresh();
        };

        addAndMakeVisible (anyChannel);
        anyChannel.setColour (juce::ToggleButton::textColourId, kText);
        anyChannel.setToggleState (processor.anyChannel(), juce::dontSendNotification);
        anyChannel.onClick = [this] { processor.setAnyChannel (anyChannel.getToggleState()); };

        addAndMakeVisible (modeBox);
        modeBox.addItem ("Momentary", 1);
        modeBox.addItem ("Latch", 2);
        modeBox.setSelectedId (processor.switchMode() == SwitchMode::Latch ? 2 : 1, juce::dontSendNotification);
        modeBox.onChange = [this] {
            processor.setSwitchMode (modeBox.getSelectedId() == 2 ? SwitchMode::Latch : SwitchMode::Momentary);
            refresh();
        };

        setupSlider (multiSlider, Timings::kMultiTapMin, Timings::kMultiTapMax);
        setupSlider (holdSlider, Timings::kHoldMin, Timings::kHoldMax);
        setupSlider (fadeSlider, Timings::kFadeMin, Timings::kFadeMax);
        loadSliders();

        auto apply = [this] {
            processor.setTimings ({ multiSlider.getValue(), holdSlider.getValue(), fadeSlider.getValue() });
        };
        multiSlider.onValueChange = apply;
        holdSlider.onValueChange = apply;
        fadeSlider.onValueChange = apply;

        addAndMakeVisible (resetButton);
        resetButton.onClick = [this] {
            processor.setTimings (Timings {});
            loadSliders();
        };

#if JucePlugin_Build_Standalone
        if (juce::StandalonePluginHolder::getInstance() != nullptr)
        {
            addAndMakeVisible (deviceButton);
            deviceButton.onClick = [] {
                if (auto* holder = juce::StandalonePluginHolder::getInstance())
                    holder->showAudioSettingsDialog();
            };
        }
#endif

        refresh();
    }

    void setupSlider (juce::Slider& s, double lo, double hi)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setRange (lo, hi, 1.0);
        s.setTextValueSuffix (" ms");
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 22);
        addAndMakeVisible (s);
    }

    void loadSliders()
    {
        const auto t = processor.timings();
        multiSlider.setValue (t.multiTapMs, juce::dontSendNotification);
        holdSlider.setValue (t.holdMs, juce::dontSendNotification);
        fadeSlider.setValue (t.fadeMs, juce::dontSendNotification);
    }

    // Called from the editor's timer too, so the label follows a learn that finished.
    void refresh()
    {
        const bool learning = processor.isLearning();
        learnButton.setButtonText (learning ? "Cancel" : "Learn");
        mappingLabel.setText (learning ? "Press your footswitch..."
                                       : "MIDI: " + processor.getMapping().describe (processor.anyChannel()),
                              juce::dontSendNotification);
        holdSlider.setEnabled (processor.switchMode() == SwitchMode::Momentary);
        holdLabel.setEnabled (holdSlider.isEnabled());
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kBackground.withAlpha (0.97f));
        g.setColour (kDim);
        g.drawRect (getLocalBounds(), 1);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16);
        const int rowH = 30, gap = 8;

        auto row = [&] { auto b = r.removeFromTop (rowH); r.removeFromTop (gap); return b; };

        auto top = row();
        learnButton.setBounds (top.removeFromRight (90));
        mappingLabel.setBounds (top);
        anyChannel.setBounds (row());

        auto m = row();
        modeLabel.setBounds (m.removeFromLeft (140));
        modeBox.setBounds (m);

        auto layoutSlider = [&] (juce::Label& l, juce::Slider& s) {
            auto b = row();
            l.setBounds (b.removeFromLeft (140));
            s.setBounds (b);
        };
        layoutSlider (multiLabel, multiSlider);
        layoutSlider (holdLabel, holdSlider);
        layoutSlider (fadeLabel, fadeSlider);

        resetButton.setBounds (row().removeFromLeft (150));
        if (deviceButton.isVisible())
            deviceButton.setBounds (row().removeFromLeft (200));
    }

    LooperProcessor& processor;
    juce::Label mappingLabel, modeLabel, multiLabel, holdLabel, fadeLabel;
    juce::TextButton learnButton { "Learn" }, resetButton { "Reset to defaults" }, deviceButton { "Audio/MIDI settings..." };
    juce::ToggleButton anyChannel { "Any MIDI channel" };
    juce::ComboBox modeBox;
    juce::Slider multiSlider, holdSlider, fadeSlider;
};

// ---------------------------------------------------------------------------------------------

LooperEditor::LooperEditor (LooperProcessor& p) : AudioProcessorEditor (&p), processor_ (p)
{
    addAndMakeVisible (settingsButton_);
    settingsButton_.setClickingTogglesState (true);

    settings_ = std::make_unique<SettingsPanel> (p);
    addChildComponent (*settings_);
    settingsButton_.onClick = [this] { settings_->setVisible (settingsButton_.getToggleState()); };

    setResizable (true, true);
    setResizeLimits (320, 360, 1000, 1000);
    setSize (420, 440);

    lastMidiCount_ = p.midiActivityCount();
    startTimerHz (30);
}

LooperEditor::~LooperEditor()
{
    // Never leave the loop's switch held down if the window goes away mid-press.
    if (buttonHeld_)
        processor_.pushSwitchEvent (false);
}

juce::Rectangle<float> LooperEditor::buttonArea() const
{
    auto area = getLocalBounds().toFloat().reduced (24.0f);
    area.removeFromBottom (60.0f);
    const float d = std::min (area.getWidth(), area.getHeight());
    return juce::Rectangle<float> (d, d).withCentre (area.getCentre());
}

void LooperEditor::resized()
{
    const auto b = getLocalBounds();
    settingsButton_.setBounds (b.getWidth() - 96, b.getHeight() - 40, 84, 28);
    settings_->setBounds (b.reduced (12).withTrimmedBottom (48));
}

void LooperEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto a = buttonArea();
    if (a.getCentre().getDistanceFrom (e.position) <= a.getWidth() * 0.5f)
    {
        buttonHeld_ = true;
        processor_.pushSwitchEvent (true);
    }
}

void LooperEditor::mouseUp (const juce::MouseEvent&)
{
    if (buttonHeld_)
    {
        buttonHeld_ = false;
        processor_.pushSwitchEvent (false);
    }
}

void LooperEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();

    const bool down = processor_.isSwitchDown();
    if (down && ! wasDown_)
        pressStartMs_ = now;
    wasDown_ = down;
    holdFraction_ = down ? juce::jlimit (0.0f, 1.0f, static_cast<float> ((now - pressStartMs_) / processor_.timings().holdMs)) : 0.0f;

    const auto count = processor_.midiActivityCount();
    if (count != lastMidiCount_)
    {
        lastMidiCount_ = count;
        lastMidiFlashMs_ = now;
    }

    for (int c = 0; c < 2; ++c)
    {
        const float peak = processor_.takeInputPeak (c);
        meter_[c] = std::max (peak, meter_[c] * 0.85f);
    }

    if (settings_->isVisible())
        settings_->refresh();

    repaint();
}

void LooperEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBackground);

    const auto status = processor_.status();
    const auto area = buttonArea();
    const auto colour = colourFor (status.state);
    const float ringW = std::max (6.0f, area.getWidth() * 0.035f);

    // Button body
    g.setColour (colour);
    g.fillEllipse (area.reduced (ringW * 1.6f));

    // Ring track
    const auto ringBounds = area.reduced (ringW * 0.5f);
    juce::Path track;
    track.addEllipse (ringBounds);
    g.setColour (colour.withAlpha (0.25f));
    g.strokePath (track, juce::PathStrokeType (ringW));

    // Loop position
    if ((status.state == State::Playing || status.state == State::Overdubbing) && status.loopLength > 0)
    {
        const float frac = static_cast<float> (status.position) / static_cast<float> (status.loopLength);
        juce::Path arc;
        arc.addCentredArc (ringBounds.getCentreX(), ringBounds.getCentreY(), ringBounds.getWidth() * 0.5f,
                           ringBounds.getHeight() * 0.5f, 0.0f, 0.0f, frac * juce::MathConstants<float>::twoPi, true);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.strokePath (arc, juce::PathStrokeType (ringW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Hold progress: fills the button so a clear never comes as a surprise
    if (holdFraction_ > 0.0f)
    {
        const auto inner = area.reduced (ringW * 1.6f);
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (inner.getSmallestIntegerContainer().withTrimmedTop (static_cast<int> (inner.getHeight() * (1.0f - holdFraction_))));
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.fillEllipse (inner);
    }

    // Labels
    const double seconds = (status.state == State::Recording ? status.recordedFrames : status.loopLength) / status.sampleRate;
    g.setColour (juce::Colours::white);
    g.setFont (juce::jmax (18.0f, area.getWidth() * 0.11f));
    g.drawText (labelFor (status.state), area.withTrimmedBottom (area.getHeight() * 0.1f), juce::Justification::centred);
    if (status.state != State::Empty)
    {
        g.setFont (juce::jmax (14.0f, area.getWidth() * 0.07f));
        g.drawText (formatTime (seconds), area.withTrimmedTop (area.getHeight() * 0.6f), juce::Justification::centredTop);
    }

    // Footer: hint, MIDI LED, meter
    auto footer = getLocalBounds().reduced (16).removeFromBottom (44);
    const bool mapped = processor_.getMapping().type != LooperProcessor::Mapping::Type::None;
    if (! mapped)
    {
        g.setColour (kDim);
        g.setFont (13.0f);
        g.drawText ("No MIDI switch mapped. Open Settings and click Learn.", footer.removeFromTop (18), juce::Justification::centredLeft);
    }
    else
    {
        footer.removeFromTop (18);
    }

    auto row = footer.removeFromTop (22);
    const bool flash = juce::Time::getMillisecondCounterHiRes() - lastMidiFlashMs_ < 120.0 || processor_.isSwitchDown();
    g.setColour (flash ? juce::Colour (0xff22c55e) : juce::Colour (0xff374151));
    g.fillEllipse (row.removeFromLeft (14).toFloat().withSizeKeepingCentre (12.0f, 12.0f));
    g.setColour (kDim);
    g.setFont (13.0f);
    g.drawText ("MIDI", row.removeFromLeft (44), juce::Justification::centredLeft);
    g.drawText ("IN", row.removeFromLeft (24), juce::Justification::centredLeft);

    const int channels = processor_.numInputChannels();
    const float barH = channels > 1 ? 7.0f : 12.0f;
    const float meterH = barH * static_cast<float> (channels) + 2.0f * static_cast<float> (channels - 1);
    juce::Rectangle<float> meterArea (static_cast<float> (row.getX()), static_cast<float> (row.getCentreY()) - meterH * 0.5f, 140.0f, meterH);
    for (int c = 0; c < channels; ++c)
    {
        auto bar = meterArea.removeFromTop (barH);
        meterArea.removeFromTop (2.0f);
        g.setColour (juce::Colour (0xff1f2937));
        g.fillRoundedRectangle (bar, 2.0f);
        const float level = juce::jlimit (0.0f, 1.0f, juce::Decibels::gainToDecibels (meter_[c], -60.0f) / 60.0f + 1.0f);
        g.setColour (meter_[c] > 0.98f ? juce::Colour (0xffdc2626) : juce::Colour (0xff22c55e));
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * level), 2.0f);
    }
}
