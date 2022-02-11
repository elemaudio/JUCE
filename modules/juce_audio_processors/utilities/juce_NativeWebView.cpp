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
namespace
{
//==============================================================================
// JUCE bridge JavaScript injection
constexpr char javascriptInjection[] =
R"END(
var juceBridge = {
    postMessage: function (param) {
        juceBridgeInternalMessage("message:" + param);
    },

    resizeTo: function (width, height) {
        juceBridgeInternalMessage("resize:" + width.toString() + "," + height.toString());
    }
};
)END";

//==============================================================================
::juce::String toEscapedJsLiteral(::juce::String const& str)
{
    ::juce::MemoryOutputStream out;
    auto lastWasHexEscapeCode = false;
    auto* utf8 = str.toUTF8().getAddress();
    auto numBytesToRead = static_cast<int>(::juce::CharPointer_UTF8::getBytesRequiredFor (str.getCharPointer()));

    for (int i = 0; i < numBytesToRead || numBytesToRead < 0; ++i)
    {
        auto c = (unsigned char) utf8[i];

        switch (c)
        {

            case '\t':  out << "\\t";  lastWasHexEscapeCode = false; break;
            case '\r':  out << "\\r";  lastWasHexEscapeCode = false; break;
            case '\n':  out << "\\n";  lastWasHexEscapeCode = false; break;
            case '\\':  out << "\\\\"; lastWasHexEscapeCode = false; break;
            case '\"':  out << "\\\""; lastWasHexEscapeCode = false; break;
            case 0:
                if (numBytesToRead < 0)
                    return out.toString();

                out << "\\0";
                lastWasHexEscapeCode = true;
                break;

            default:
                if (c >= 32 && c < 127 && ! (lastWasHexEscapeCode  // (have to avoid following a hex escape sequence with a valid hex digit)
                                                && ::juce::CharacterFunctions::getHexDigitValue (c) >= 0))
                {
                    out << (char) c;
                    lastWasHexEscapeCode = false;
                }
                else
                {
                    out << (c < 16 ? "\\x0" : "\\x") << ::juce::String::toHexString ((int) c);
                    lastWasHexEscapeCode = true;
                }

                break;
        }
    }

    return out.toString();
}
}

namespace juce
{

//==============================================================================
NativeWebView::NativeWebView(void* parentNativeWindow,
                             WebViewConfiguration const& webViewConfig,
                             std::function<void (NativeWebView&, int, int)> && resizeCallback)
    : config (webViewConfig),
      resize (std::move (resizeCallback)),
      nativeImpl(Impl::create(parentNativeWindow,
                              webViewConfig.size,
                              webViewConfig.url,
                              javascriptInjection,
                              {
                                 [this] () { finishLoading(); },
                                 [this] (String const& msg) { messageReceived (msg); }
                              }))
{}

NativeWebView::~NativeWebView() {
    if (config.onDestroy)
        config.onDestroy();
}

void NativeWebView::setBounds(Rectangle<int> const& rc)
{
    nativeImpl->setBounds (rc);
}

Rectangle<int> NativeWebView::getBounds()
{
    return nativeImpl->getBounds();
}

void NativeWebView::finishLoading()
{
    if (config.onLoad)
    {
      config.onLoad([this] (String const& msg)
      {
         nativeImpl->executeJS("juceBridgeOnMessage", msg);
      });
    }
}

void NativeWebView::messageReceived(String const& msg)
{
    auto deliminator = msg.indexOfChar(':');

    if (deliminator == -1)
        return;

    auto cmd = msg.substring(0, deliminator);
    auto arg = msg.substring(deliminator + 1);

    if (cmd == "message") {
        if (config.onMessageReceived)
            config.onMessageReceived(arg);
    } else if (cmd == "resize") {
        auto size = ::juce::StringArray::fromTokens(arg, ",", {});

        if (size.size() < 2)
            return;

        auto width  = size[0].getIntValue();
        auto height = size[1].getIntValue();

        if (resize)
            resize (*this, width, height);
    }
}

#if JUCE_MAC || JUCE_IOS
void* NativeWebView::transferOwnershipToNativeView(std::unique_ptr<NativeWebView> && nativeWebView)
{
    return [Impl::transferOwnershipToNativeView(std::move(nativeWebView)).release() autorelease];
}

NativeWebView* NativeWebView::getWebViewObjFromNSView(void* nativeView)
{
    return Impl::getWebViewObjFromNSView(static_cast<NSObject*>(nativeView));
}
#endif

//==============================================================================
void NativeWebView::Impl::executeJS(String const& functionName, String const& param)
{
    MemoryOutputStream mo;

    mo << functionName << "(" << "\"" << toEscapedJsLiteral(param) << "\");";
    evalJS(mo.toString());
}

NativeWebView::Impl* NativeWebView::Impl::getImpl(NativeWebView* _this)
{
    return _this->nativeImpl.get();
}
}
