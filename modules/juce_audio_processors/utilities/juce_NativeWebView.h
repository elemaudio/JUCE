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
    NativeWebView(void* parentNativeWindow,
                  WebViewConfiguration const& webViewConfig,
                  std::function<void (NativeWebView&, int, int)> && resizeCallback);
    ~NativeWebView();

    //==============================================================================
    void setBounds(Rectangle<int> const&);
    Rectangle<int> getBounds();

   #if JUCE_MAC || JUCE_IOS
    // transfers the ownership of the nativeWebView to the underlying native NSView itself
    // (i.e. when the NSView is deleted, the instance of NativeWebView is also deleted).
    // This function returns an autoreleased NSView pointer.
    static void* transferOwnershipToNativeView(std::unique_ptr<NativeWebView> && nativeWebView);

    // if (and only if) the ownership was transferred, then the NativeWebView pointer
    // can be acquired from the native view itself.
    static NativeWebView* getWebViewObjFromNSView(void* nativeView);
   #endif

    class Impl;
private:
    void finishLoading();
    void messageReceived(String const&);

    //==============================================================================
    friend class Impl;

    //==============================================================================
    WebViewConfiguration config;
    std::function<void (NativeWebView&, int, int)> resize;
    std::unique_ptr<Impl> nativeImpl;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NativeWebView)
};
}
