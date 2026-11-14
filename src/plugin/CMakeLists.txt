include(FetchContent)

# JUCE is used under its GPLv3 licence (this project is open source).
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.8
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(LooperPlugin
    PRODUCT_NAME "Looper"
    COMPANY_NAME "Looper"
    BUNDLE_ID com.looper.Looper
    PLUGIN_MANUFACTURER_CODE Lpmf
    PLUGIN_CODE Lpr1
    FORMATS VST3 Standalone
    VST3_CATEGORIES Fx
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Looper needs your audio input to record the loop.")

target_sources(LooperPlugin PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
)

target_compile_definitions(LooperPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(LooperPlugin
    PRIVATE
        looper_core
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
