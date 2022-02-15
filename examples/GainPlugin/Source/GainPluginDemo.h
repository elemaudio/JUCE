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
                                           .withOutput ("Output", AudioChannelSet::stereo()),
                          getEditorWebViewConfiguration())
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
    
    AudioProcessorParameter* getBypassParameter() const override { return bypass; }

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
    
    //==============================================================================
    void webViewReceivedMessage(String const& message) override
    {
        auto args = StringArray::fromTokens(message, "@", {});
        auto msg = args[0];
        
        if (msg == "param") {
            auto param = args[1];
            auto value = args[2].getFloatValue();
            
            if      (param == "gain")   gain->setValueNotifyingHost(value);
            else if (param == "bypass") bypass->setValueNotifyingHost(value);
        } else if (msg == "update") {
            triggerAsyncUpdate();
        }
    }

private:
    //==============================================================================
    void handleAsyncUpdate () override {
        {
            MemoryOutputStream mo;
            mo << "bypass@" << (static_cast<bool>(*bypass) ? 1 : 0);
            sendMessageToWebView(mo.toString());
        }
        
        {
            MemoryOutputStream mo;
            mo << "gain@"   << static_cast<float>(*gain) << "\n";
            sendMessageToWebView(mo.toString());
        }
    }
    
    void parameterValueChanged (int /*parameterIndex*/, float /*newValue*/) override {
        triggerAsyncUpdate();
    }
    
    void parameterGestureChanged (int, bool) override {}
    
    //==============================================================================
    static WebViewConfiguration getEditorWebViewConfiguration()
    {
        static WebViewConfiguration config = [] ()
        {
            static constexpr char htmlPage[] = R"END(
                <html style="background-color:#33475b">
                    <body>
                        <center>
                <script>
                    function juceBridgeOnMessage(message) {
                        var args = message.split("@");
                        var paramId = args[0];
                        var value = Number(args[1]);
                        
                        if (paramId == "gain")        { document.getElementById("gain").value = value * 100.; }
                        else if (paramId == "bypass") { document.getElementById("bypass").checked = value; }
                    }
                    
                    window.onload = function () {
                        juceBridge.postMessage("update");
                    }
                </script>
                <input type="checkbox" id="bypass" name="bypass" onchange="juceBridge.postMessage('param@bypass@' + (this.checked ? '1' : '0'))"/>
                <label for="bypass">Bypass</label><br/>
                <input type="range" id="gain" value = "0" name="gain" min="0" max="100" oninput="juceBridge.postMessage('param@gain@' + (this.value / 100.))"/>
                <label for="range">Gain</label><br/>
                <button name = "button" value = "Resize" type = "button" onclick="juceBridge.resizeTo(800, 400)">Resize!</button>
                </center>
                </body>
                </html>
                )END";
            MemoryBlock htmlPageData(htmlPage, sizeof(htmlPage));
            
            WebViewConfiguration result;
            
            result.url = URL(htmlPageData, "text/html");
            result.size = Rectangle<int>(0, 0, 200, 100);
            
            return result;
        } ();
        
        return config;
    }
    
    //==============================================================================
    AudioParameterBool* bypass;
    AudioParameterFloat* gain;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GainProcessor)
};
