/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2020 - Raw Material Software Limited

   JUCE is an open source library subject to commercial or open-source
   licensing.

   By using JUCE, you agree to the terms of both the JUCE 6 End-User License
   Agreement and JUCE Privacy Policy (both effective as of the 16th June 2020).

   End User License Agreement: www.juce.com/juce-6-licence
   Privacy Policy: www.juce.com/juce-privacy-policy

   Or: You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/
#pragma once

class StandaloneAudio   : private Timer
{
public:
    StandaloneAudio(AudioProcessor& audioProcessor, ApplicationProperties& applicationProperties)
        : deviceManager(std::make_shared<AudioDeviceManager>()),
          appProperties(applicationProperties),
          settingsView(getAudioSettingsWebViewConfiguration(), {}, [this] (String const& msg) { receivedMessage(msg); })
    {
        processorPlayer.setProcessor(&audioProcessor);
        deviceManager->addAudioCallback(&processorPlayer);

        auto hasAudioInput = (audioProcessor.getChannelCountOfBus(true, 0) != 0);
        auto hasAudioOutput = (audioProcessor.getChannelCountOfBus(false, 0) != 0);

        if (hasAudioOutput || hasAudioInput) {
            std::shared_ptr<XmlElement> savedState (appProperties.getUserSettings()->getXmlValue ("audioDeviceState").release());
            RuntimePermissions::request (RuntimePermissions::recordAudio,
                                        [deviceManager = deviceManager, savedState, hasAudioOutput, hasAudioInput] (bool granted) mutable
                                        {
                                            // bit of a hack as RuntimePermissions::request does not allow us to move in a lambda
                                            std::weak_ptr<AudioDeviceManager> weak(deviceManager);
                                            deviceManager = nullptr;

                                            if (auto dm = weak.lock())
                                                dm->initialise (granted && hasAudioInput ? 256 : 0, hasAudioOutput ? 256 : 0, savedState.get(), true);
                                        });
        }
    }

    NativeWebView& getSettingsView() {
        return settingsView;
    }

private:
    //==============================================================================
    static WebViewConfiguration getAudioSettingsWebViewConfiguration()
    {
        static WebViewConfiguration config = [] ()
        {
            static constexpr char htmlPage[] = R"END(
                <html style="background-color:#33475b">
                    <body>
                <script>
                    function juceBridgeOnMessage(message) {
                        var settings = JSON.parse(message);
                        var directions = ['inputs', 'outputs'];
                        for (const direction of directions) {
                            var deviceSection = document.getElementById(direction);
                            var devicesSelector = deviceSection.querySelector('select');
                            if (direction in settings) {
                                var types = settings[direction];

                                for (const [key, value] of Object.entries(types)) {
                                    var optionGroup = document.createElement("optgroup");
                                    optionGroup.label = key;

                                    for (const device of value) {
                                        var option = document.createElement("option");
                                        option.textContent = device['name'];
                                        if (device['selected']) {
                                            option.setAttribute('selected', true);
                                        }
                                        optionGroup.appendChild(option);
                                    }

                                    devicesSelector.replaceChildren(optionGroup);
                                }
                                deviceSection.style.display = "block";
                            } else {
                                deviceSection.style.display = "none";
                            }
                        }
                    }

                    function sendObjectToNativeCode(obj) {
                        juceBridge.postMessage(JSON.stringify(obj));
                    }

                    function playTestTone() {
                        sendObjectToNativeCode({'message': 'playTestTone', 'params': {}});
                    }

                    function deviceChanged(isInput, option) {
                        var deviceName = option.value;
                        var typeName = option.parentElement.label;
                        sendObjectToNativeCode({'message': 'deviceChanged', 'params': {'isInput': isInput, 'typeName': typeName, 'deviceName': deviceName}});
                    }
                    
                    window.onload = function () {
                        document.getElementById("inputs").style.display = "none";
                        document.getElementById("outputs").style.display = "none";

                        sendObjectToNativeCode({'message': 'onLoad', 'params': {}});
                    }
                </script>
                <p>Michael, please make this look good!</p>
                <div id="inputs">
                    <label for="input_device">Input Device:</label><br/>
                    <select name="input_device" id="input_device" onchange="deviceChanged(true, this.options[this.selectedIndex])">
                    </select>
                </div>
                <div id="outputs">
                    <label for="output_device">Output Device:</label><br/>
                    <select name="output_device" id="output_device" onchange="deviceChanged(false, this.options[this.selectedIndex])">
                    </select>
                    <button name = "button" value = "testtone" type = "button" onclick="playTestTone()">Play Tone</button>
                </div>
                </body>
                </html>
            )END";
            MemoryBlock htmlPageData(htmlPage, sizeof(htmlPage));
            
            WebViewConfiguration result;
            
            result.url = URL(htmlPageData, "text/html");
            result.size = Rectangle<int>(0, 0, 400, 300);
            
            return result;
        } ();
        
        return config;
    }

    void deviceChanged(bool isInput, String const& typeName, String const& deviceName) {
        stopTimer();

        auto currentSetup = deviceManager->getAudioDeviceSetup();
        AudioDeviceManager::AudioDeviceSetup newSetup;

        newSetup.inputDeviceName = currentSetup.inputDeviceName;
        newSetup.outputDeviceName = currentSetup.outputDeviceName;

        // find the type
        auto* currentDeviceType = deviceManager->getCurrentDeviceTypeObject();
        AudioIODeviceType* newDeviceType = nullptr;
        auto& deviceTypes = deviceManager->getAvailableDeviceTypes();

        for (auto& deviceType : deviceTypes) {
            if (deviceType->getTypeName() == typeName) {
                newDeviceType = deviceType;
                break;
            }
        }

        if (newDeviceType == nullptr) {
            jassertfalse;
            return;
        }

        if (newDeviceType != currentDeviceType) {
            newDeviceType->scanForDevices();

            // change the other direction to the default device of this type
            auto otherDirection = ! isInput;
            auto defaultDeviceIdxForOtherDirection = newDeviceType->getDefaultDeviceIndex(otherDirection);
            auto defaultDeviceForOtherDirection = newDeviceType->getDeviceNames(otherDirection)[defaultDeviceIdxForOtherDirection];

            jassert(defaultDeviceForOtherDirection.isNotEmpty());
            (otherDirection ? newSetup.inputDeviceName : newSetup.outputDeviceName) = defaultDeviceForOtherDirection;

            deviceManager->closeAudioDevice();
            deviceManager->setCurrentAudioDeviceType(newDeviceType->getTypeName(), true);
        }

        (isInput ? newSetup.inputDeviceName : newSetup.outputDeviceName) = deviceName;
        deviceManager->setAudioDeviceSetup(newSetup, true);

        updateAudioSettingsView();
        startTimer(500);
    }

    void receivedMessage(String const& jsonString) {
        auto json = JSON::parse(jsonString);

        if (! json.isObject()) {
            jassertfalse;
            return;
        }

        auto messageValue = json["message"];
        if (! messageValue.isString()) {
            jassertfalse;
            return;
        }

        auto params = json["params"];
        auto message = messageValue.toString();
        
        if (message == "onLoad") {
            updateAudioSettingsView();
            startTimer(500);
        } else if (message == "playTestTone") {
            deviceManager->playTestSound();
        } else if (message == "deviceChanged") {
            auto isInput = static_cast<bool>(params["isInput"]);
            auto typeName = params["typeName"].toString();
            auto deviceName = params["deviceName"].toString();
           
            deviceChanged(isInput, typeName, deviceName);
        } else {
            // unknown message
            jassertfalse;
        }
    }

    void updateAudioSettingsView() {
        if (processorPlayer.getCurrentProcessor() == nullptr)
            return;
        
        auto hasAudioInput  = (processorPlayer.getCurrentProcessor()->getChannelCountOfBus(true,  0) != 0);
        auto hasAudioOutput = (processorPlayer.getCurrentProcessor()->getChannelCountOfBus(false, 0) != 0);

        auto currentSetup = deviceManager->getAudioDeviceSetup();

        DynamicObject::Ptr root = new DynamicObject;

        for (auto i = (hasAudioInput ? 0 : 1); i < (hasAudioOutput ? 2 : 1); ++i) {
            auto isInput = (i == 0);

            DynamicObject::Ptr deviceTypes = new DynamicObject;

            auto& types = deviceManager->getAvailableDeviceTypes();

            for (auto& type : types) {
                auto currentTypeSelected = (deviceManager->getCurrentDeviceTypeObject() == type);

                type->scanForDevices();
                auto names = type->getDeviceNames(isInput);

                Array<var> devices;

                for (auto& name: names) {
                    auto isSelected = currentTypeSelected && name == (isInput ? currentSetup.inputDeviceName : currentSetup.outputDeviceName);
                    DynamicObject::Ptr device = new DynamicObject;

                    device->setProperty("name", name);
                    device->setProperty("selected", var(isSelected));
                    devices.insert(-1, var(device));
                }

                if (devices.size() > 0)
                    deviceTypes->setProperty(type->getTypeName(), var(devices));
            }

            root->setProperty(isInput ? "inputs" : "outputs", var(deviceTypes));
        }

        auto jsonString = JSON::toString(var(root), true);

        if (jsonString == previousDeviceStatus)
            return;

        previousDeviceStatus = jsonString;
        settingsView.sendMessage(jsonString);
    }

    //==============================================================================
    void timerCallback() override {
        updateAudioSettingsView();
    }

    //==============================================================================
    AudioProcessorPlayer processorPlayer;
    ApplicationProperties& appProperties;
    std::shared_ptr<AudioDeviceManager> deviceManager;
    NativeWebView settingsView;
    String previousDeviceStatus;
};