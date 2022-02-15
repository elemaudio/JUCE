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
class NativeWebView::Impl
{
public:
    //==============================================================================
    struct Callbacks {
        std::function<void ()> finishLoading;
        std::function<void (String const&)> messageReceived;
    };

    //==============================================================================
    static std::unique_ptr<Impl> create(Rectangle<int> const& initialBounds,
                                        URL const& url,
                                        String const& jsBootstrap,
                                        Callbacks && callbacks);
    virtual ~Impl() = default;

    //==============================================================================
    virtual void setBounds(Rectangle<int> const&) = 0;
    virtual Rectangle<int> getBounds() = 0;
    virtual void executeJS(String const& functionName, String const& param);
    virtual void evalJS(String const& javascript) = 0;
    virtual void attachToParent(void*) = 0;
    virtual void detachFromParent() = 0;
    
   #if JUCE_MAC || JUCE_IOS
    virtual void* getNativeView() = 0;
   #endif

protected:
    Impl() = default;

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Impl)
};
}
