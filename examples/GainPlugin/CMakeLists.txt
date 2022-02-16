cmake_minimum_required(VERSION 3.15)
project(GainPlugin VERSION 0.1)

find_package(juce CONFIG REQUIRED)

juce_set_vst2_sdk_path(/Users/fr810/Development/SDKs/VST3_SDK)
juce_add_plugin(GainPlugin 
    FORMATS AU AUv3 VST VST3 Standalone
    BUNDLE_ID com.JUCE.GainPlugin
    COMPANY_NAME JUCE
    PLUGIN_MANUFACTURER_CODE Manu
    PLUGIN_CODE Yc8b
    APP_SANDBOX_ENABLED On
    APP_SANDBOX_OPTIONS com.apple.security.network.client com.apple.security.network.server)
    
target_sources(GainPlugin PRIVATE Source/Main.cpp Source/GainPluginDemo.h)

target_link_libraries(GainPlugin PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics

    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)