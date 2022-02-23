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
// Windows bootstrap code
constexpr char windowsJSInjection[] =R"END(
function juceBridgeInternalMessage(msg) {
    window.external.notify(msg);
}
)END";

class WinWebView : public NativeWebView::Impl
{
public:
    //==============================================================================
    WinWebView(Rectangle<int> const& initialBounds,
               URL const& url,
               bool /*wantsKeyboard*/,
               String const& jsBootstrap,
               std::function<void(String const&)>&& messageReceivedCallback)
        : messageReceived(std::move(messageReceivedCallback)),
        windowClass(registerWindowClass()),
        parentWhenDetached(CreateWindowEx(0, 
                                          (LPCTSTR)(pointer_sized_uint)windowClass,
                                          L"", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                          nullptr, nullptr, (HMODULE)Process::getCurrentModuleInstanceHandle(), nullptr))
    {
        init_apartment(winrt::apartment_type::single_threaded);
        auto process = winrt::Windows::Web::UI::Interop::WebViewControlProcess();

        winrt::Windows::Foundation::Rect rc;

        rc.X = static_cast<float>(initialBounds.getX());
        rc.Y = static_cast<float>(initialBounds.getY());
        rc.Width = static_cast<float>(initialBounds.getWidth());
        rc.Height = static_cast<float>(initialBounds.getHeight());

        auto asyncOp = process.CreateWebViewControlAsync(reinterpret_cast<int64_t>(parentWhenDetached), rc);
        if (asyncOp.Status() != winrt::Windows::Foundation::AsyncStatus::Completed) {
            ::HANDLE h(::CreateEvent(nullptr, false, false, nullptr));
            asyncOp.Completed([h](auto, auto) { ::SetEvent(h); });

            DWORD i;
            CoWaitForMultipleHandles(COWAIT_DISPATCH_WINDOW_MESSAGES |
                COWAIT_DISPATCH_CALLS |
                COWAIT_INPUTAVAILABLE,
                INFINITE, 1, &h, &i);
        }
        mWebView = asyncOp.GetResults();
        mWebView.Settings().IsScriptNotifyAllowed(true);

        mWebView.ScriptNotify([this](auto const&, auto const& args) {
            if (messageReceived)
                messageReceived(winrt::to_string(args.Value()));
        });

        String jsInjection;
        {
            MemoryOutputStream mo;

            mo << windowsJSInjection << "\n" << jsBootstrap;
            jsInjection = mo.toString();
        }

        mWebView.NavigationStarting([this, jsInjection](auto const&, auto const&) {
            mWebView.AddInitializeScript(winrt::to_hstring(jsInjection.toRawUTF8()));
        });

        mWebView.IsVisible(true);

        if (url.isLocalFile()) {
            // Windows does not support navigating to local files :-(
            // I need to read in the file myself. Note that local relative resources
            // will not load.

            // TODO: implement workaround described here: https://github.com/CommunityToolkit/WindowsCommunityToolkit/issues/2297
            auto htmlContents = url.getLocalFile().loadFileAsString();
            mWebView.NavigateToString(winrt::to_hstring(htmlContents.toRawUTF8()));
        } else if (url.isDataScheme()) {
            auto data = url.getURLEncodedData();
            String htmlContent(static_cast<char*>(data.getData()), data.getSize());
            mWebView.NavigateToString(winrt::to_hstring(htmlContent.toRawUTF8()));
        } else {
            winrt::Windows::Foundation::Uri uri(winrt::to_hstring(url.toString(true).toRawUTF8()));
            mWebView.Navigate(uri);
        }
    }

    ~WinWebView()
    {
        ::DestroyWindow(parentWhenDetached);
        ::UnregisterClass((LPCTSTR)(pointer_sized_uint)windowClass, nullptr);
    }

    void setBounds(Rectangle<int> const& newBounds) override {
        winrt::Windows::Foundation::Rect rc;

        rc.X = static_cast<float>(newBounds.getX());
        rc.Y = static_cast<float>(newBounds.getY());
        rc.Width = static_cast<float>(newBounds.getWidth());
        rc.Height = static_cast<float>(newBounds.getHeight());
        mWebView.Bounds(rc);
    }

    Rectangle<int> getBounds() override {
        auto bounds = mWebView.Bounds();
        return Rectangle<int>(static_cast<int>(bounds.X),     static_cast<int>(bounds.Y),
                              static_cast<int>(bounds.Width), static_cast<int>(bounds.Height));
    }

    void attachToParent(void* nativeWindowPtr) override
    {
        if (currentParent != nullptr)
        {
            jassertfalse;
            return;
        }

        auto nativeWindow = reinterpret_cast<::HWND>(nativeWindowPtr);

        if (auto webKitNativeWindow = ::FindWindowEx(parentWhenDetached, nullptr, nullptr, nullptr))
        {
            ::SetParent(webKitNativeWindow, nativeWindow);
            currentParent = nativeWindow;
        }
    }

    void detachFromParent() override
    {
        if (currentParent == nullptr)
        {
            jassertfalse;
            return;
        }

        if (auto webKitNativeWindow = ::FindWindowEx(currentParent, nullptr, nullptr, nullptr))
        {
            ::SetParent(webKitNativeWindow, parentWhenDetached);
            currentParent = nullptr;
        }
    }

    void executeJS(String const& functionName, String const& param) override
    {
        mWebView.InvokeScriptAsync(winrt::to_hstring(functionName.toRawUTF8()),
                                   winrt::single_threaded_vector<winrt::hstring>({ winrt::to_hstring(param.toRawUTF8()) }));
    }

    void evalJS(String const& javascript) override
    {
        executeJS("eval", javascript);
    }
private:
    ::ATOM registerWindowClass()
    {
        String className;

        {
            MemoryOutputStream mo;

            mo << "webKitWndHolder_" << reinterpret_cast<std::int64_t>(this);
            className = mo.toString();
        }

        WNDCLASSEX wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = ::DefWindowProc;
        wc.cbWndExtra = 4;
        wc.hInstance = (HMODULE)Process::getCurrentModuleInstanceHandle();
        wc.lpszClassName = className.toWideCharPointer();

        return ::RegisterClassEx(&wc);
    }

    //==============================================================================
    std::function<void(String const&)> messageReceived;
    ::ATOM windowClass;
    ::HWND parentWhenDetached;
    ::HWND currentParent = nullptr;
    winrt::Windows::Web::UI::Interop::WebViewControl mWebView = {nullptr};

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WinWebView)
};

//==============================================================================
std::unique_ptr<NativeWebView::Impl> NativeWebView::Impl::create(Rectangle<int> const& initialBounds,
                                                                 URL const& url,
                                                                 bool wantsKeybaordFocus,
                                                                 String const& jsBootstrap,
                                                                 std::function<void(String const&)> && messageReceived) {
    return std::make_unique<WinWebView> (initialBounds, url, wantsKeybaordFocus, jsBootstrap, std::move(messageReceived));
}
}
