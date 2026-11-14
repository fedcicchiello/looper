#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#if JucePlugin_Build_Standalone
 // Gives access to the standalone app's audio/MIDI settings dialog.
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

#include "PluginProcessor.h"

// One resizable window: a big round button that shows the state and acts as the footswitch,
// a MIDI activity LED, an input meter, and a settings panel.
class LooperEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LooperEditor (LooperProcessor&);
    ~LooperEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    struct SettingsPanel;

    void timerCallback() override;
    juce::Rectangle<float> buttonArea() const;

    LooperProcessor& processor_;

    juce::TextButton settingsButton_ { "Settings" };
    std::unique_ptr<SettingsPanel> settings_;

    bool buttonHeld_ = false;
    bool wasDown_ = false;
    double pressStartMs_ = 0.0;
    float holdFraction_ = 0.0f;

    uint32_t lastMidiCount_ = 0;
    double lastMidiFlashMs_ = 0.0;
    float meter_[2] = { 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LooperEditor)
};
