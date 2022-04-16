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
        juceBridgeInternalMessage("msg:" + param);
    }
};

var console=(function(oldCons){
    return {
        log: function(text){
            oldCons.log(text);
            juceBridgeInternalMessage("log:log+" + text);
        },
        info: function (text) {
            oldCons.info(text);
            juceBridgeInternalMessage("log:info+" + text);
        },
        warn: function (text) {
            oldCons.warn(text);
            juceBridgeInternalMessage("log:warn+" + text);
        },
        error: function (text) {
            oldCons.error(text);
            juceBridgeInternalMessage("log:error+" + text);
        }
    };
}(window.console));

//Then redefine the old console
window.console = console;
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
NativeWebView::NativeWebView(WebViewConfiguration const& webViewConfig,
                             std::function<void (String const&)> && receivedCb)
    : config (webViewConfig),
      receiveMessageCallback (std::move(receivedCb)),
      nativeImpl(Impl::create(webViewConfig.size,
                              webViewConfig.url,
                              webViewConfig.wantsKeyboardFocus,
                              javascriptInjection,
                              [this](String const& m) { internalMessageReceived(m); }))
{}

NativeWebView::~NativeWebView() = default;

void NativeWebView::setBounds(Rectangle<int> const& rc)
{
    nativeImpl->setBounds (rc);
}

Rectangle<int> NativeWebView::getBounds()
{
    return nativeImpl->getBounds();
}

void NativeWebView::sendMessage(String const& msg)
{
    nativeImpl->executeJS("juceBridgeOnMessage", msg);
}

void NativeWebView::attachToParent(void* nativeParent)
{
    if (std::exchange(attached, true))
    {
        // multiple editors for a single plug-in instance
        // not supported
        jassertfalse;
        return;
    }
    
    nativeImpl->attachToParent(nativeParent);
}

void NativeWebView::detachFromParent()
{
    if (std::exchange(attached, false))
        nativeImpl->detachFromParent();
}

bool NativeWebView::isAttached() const noexcept
{
    return attached;
}

#if JUCE_MAC || JUCE_IOS
void* NativeWebView::getNativeView()
{
    return nativeImpl->getNativeView();
}
#endif

void NativeWebView::internalMessageReceived(String const& message) {
    auto cmd = message.substring(0, message.indexOf(":"));
    auto payload = message.substring(message.indexOf(":") + 1);

    if (cmd == "msg") receiveMessageCallback(payload);
    else if (cmd == "log") {
        auto logtype = payload.substring(0, payload.indexOf("+"));
        auto logmsg = payload.substring(payload.indexOf("+") + 1);

        // ignoring logtype for now
        (void)logtype;
        DBG(logmsg);
    }
}

//==============================================================================
void NativeWebView::Impl::executeJS(String const& functionName, String const& param)
{
    MemoryOutputStream mo;

    mo << functionName << "(" << "\"" << toEscapedJsLiteral(param) << "\");";
    evalJS(mo.toString());
}
}
