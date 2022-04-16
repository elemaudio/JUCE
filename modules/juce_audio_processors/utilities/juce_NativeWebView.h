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
namespace juce
{
class NativeWebView
{
public:
    //==============================================================================
    NativeWebView(WebViewConfiguration const& webViewConfig,
                  std::function<void (String const&)> && messageReceived);
    ~NativeWebView();

    //==============================================================================
    void setBounds(Rectangle<int> const&);
    Rectangle<int> getBounds();
    
    //==============================================================================
    void sendMessage(String const&);
    
    //==============================================================================
    void attachToParent(void* nativeParent);
    void detachFromParent();
    bool isAttached() const noexcept;
    
   #if JUCE_MAC || JUCE_IOS
    void* getNativeView();
   #endif

    class Impl;
private:
    //==============================================================================
    friend class Impl;

    void internalMessageReceived(String const&);

    //==============================================================================
    WebViewConfiguration config;
    std::function<void(String const&)> receiveMessageCallback;
    std::unique_ptr<Impl> nativeImpl;
    bool attached = false;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NativeWebView)
};
}
