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
class NativeWebView  : private Timer
{
public:
    //==============================================================================
    NativeWebView(WebViewConfiguration const& webViewConfig,
                  std::function<void ()> && loadFinished,
                  std::function<void (String const&)> && messageReceived);
    ~NativeWebView() override;

    //==============================================================================
    void setBounds(Rectangle<int> const&);
    Rectangle<int> getBounds();
    void setResizeRequestCallback(std::weak_ptr<std::function<void (NativeWebView&, int, int)>> && cb);
    std::shared_ptr<std::function<void (NativeWebView&, int, int)>> defaultSizeRequestHandler;
    
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
    void finishLoading();
    void defaultSizeHandler(NativeWebView&, int, int);
    void messageReceived(String const&);
    void timerCallback() override;
    void checkNativeImpl();

    //==============================================================================
    friend class Impl;

    //==============================================================================
    WebViewConfiguration config;
    std::function<void ()> finished;
    std::function<void (String const&)> msgReceived;
    std::weak_ptr<std::function<void (NativeWebView&, int, int)>> resize;
    std::unique_ptr<Impl> nativeImpl;
    bool attached = false;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NativeWebView)
};
}
