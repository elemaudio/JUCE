/*
  ==============================================================================

   This file is part of the JUCE examples.
   Copyright (c) 2020 - Raw Material Software Limited

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES,
   WHETHER EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR
   PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

 name:             GainPlugin
 version:          1.0.0
 vendor:           JUCE
 website:          http://juce.com
 description:      Gain audio plugin.

 dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats,
                   juce_audio_plugin_client, juce_audio_processors,
                   juce_audio_utils, juce_core, juce_data_structures,
                   juce_events, juce_graphics, juce_gui_basics, juce_gui_extra
 exporters:        xcode_mac, vs2019

 moduleFlags:      JUCE_STRICT_REFCOUNTEDPOINTER=1

 type:             AudioProcessor
 mainClass:        GainProcessor

 useLocalCopy:     1

 END_JUCE_PIP_METADATA

*******************************************************************************/

#pragma once


//==============================================================================
class GainProcessor  : public AudioProcessor, private AudioProcessorParameter::Listener, private AsyncUpdater
{
public:

    //==============================================================================
    GainProcessor()
        : AudioProcessor (BusesProperties().withInput  ("Input",  AudioChannelSet::stereo())
                                           .withOutput ("Output", AudioChannelSet::stereo()))
    {
        addParameter (bypass = new AudioParameterBool ("bypass", "Bypass", false));
        addParameter (gain = new AudioParameterFloat ("gain", "Gain", 0.0f, 1.0f, 0.5f));
        
        bypass->addListener(this);
        gain->addListener(this);
    }

    //==============================================================================
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}

    void processBlock (AudioBuffer<float>& buffer, MidiBuffer&) override
    {
        auto gainFactor = *bypass ? 1.f : *gain;
        buffer.applyGain (gainFactor);
    }

    void processBlock (AudioBuffer<double>& buffer, MidiBuffer&) override
    {
        auto gainFactor = *bypass ? 1.f : *gain;
        buffer.applyGain (gainFactor);
    }

    //==============================================================================
    AudioProcessorEditor* createEditor() override          { return nullptr; }
    bool hasEditor() const override                        { return false;   }
    WebViewConfiguration getEditorWebViewConfiguration() override
    {
        WebViewConfiguration config;
        
        config.url = URL("file:///Users/fr810/Development/JUCE/examples/GainPlugin/plugin.html");
        config.size = Rectangle<int>(0, 0, 200, 100);
        config.onMessageReceived
            = [this] (var const& v) -> std::future<var>
            {
                if (v["message"] == "param") {
                    if      (v["param"] == "gain")   gain->setValueNotifyingHost(v["value"]);
                    else if (v["param"] == "bypass") bypass->setValueNotifyingHost(v["value"]);
                } else if (v["message"] == "update") {
                    triggerAsyncUpdate();
                }
                
                std::promise<var> p;
                p.set_value({});
                return p.get_future();
            };
        
        config.onLoad
            = [this] (WebViewConfiguration::ExecuteJavascript && jScript)
            {
                javaScriptExecutor = std::move(jScript);
            };
        
        config.onDestroy
            = [this] ()
            {
                javaScriptExecutor = {};
            };
        
        return config;
    }

    //==============================================================================
    const String getName() const override                  { return "Gain PlugIn"; }
    bool acceptsMidi() const override                      { return false; }
    bool producesMidi() const override                     { return false; }
    double getTailLengthSeconds() const override           { return 0; }

    //==============================================================================
    int getNumPrograms() override                          { return 1; }
    int getCurrentProgram() override                       { return 0; }
    void setCurrentProgram (int) override                  {}
    const String getProgramName (int) override             { return "None"; }
    void changeProgramName (int, const String&) override   {}

    //==============================================================================
    void getStateInformation (MemoryBlock& destData) override
    {
        MemoryOutputStream (destData, true).writeFloat (*gain);
    }

    void setStateInformation (const void* data, int sizeInBytes) override
    {
        gain->setValueNotifyingHost (MemoryInputStream (data, static_cast<size_t> (sizeInBytes), false).readFloat());
    }

    //==============================================================================
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        const auto& mainInLayout  = layouts.getChannelSet (true,  0);
        const auto& mainOutLayout = layouts.getChannelSet (false, 0);

        return (mainInLayout == mainOutLayout && (! mainInLayout.isDisabled()));
    }

private:
    //==============================================================================
    void handleAsyncUpdate () override {
        if (javaScriptExecutor) {
            MemoryOutputStream mo;
            
            mo << "updateParam(\"bypass\", " << static_cast<bool>(*bypass) << ");\n";
            mo << "updateParam(\"gain\", " << static_cast<float>(*gain) << ");\n";
            
            javaScriptExecutor(mo.toString());
        }
    }
    
    void parameterValueChanged (int parameterIndex, float newValue) override {
        triggerAsyncUpdate();
    }
    
    void parameterGestureChanged (int, bool) override {}
    
    //==============================================================================
    AudioParameterBool* bypass;
    AudioParameterFloat* gain;
    
    WebViewConfiguration::ExecuteJavascript javaScriptExecutor;
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GainProcessor)
};
