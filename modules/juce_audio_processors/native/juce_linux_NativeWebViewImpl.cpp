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
// Linux specific bootstrap code
constexpr char linuxJSInjection[] = R"END(
function juceBridgeInternalMessage(msg) {
    webkit.messageHandlers.juceBridge.postMessage(msg);
}
)END";

class LinuxWebView : public NativeWebView::Impl
{
public:
    //==============================================================================
    LinuxWebView(Rectangle<int> const& initialBounds,
                 URL const& url,
                 String const& jsBootstrap,
                 Callbacks && jsCallbacks)
      : fixed(gtk_fixed_new(), gtk_widget_destroy),
        wkView(webkit_web_view_new(), gtk_widget_destroy),
        jsCancellable(g_cancellable_new(), [] (GCancellable* obj) { g_cancellable_cancel (obj); g_object_unref (obj); }),
        currentBounds(initialBounds),
        callbacks(std::move(jsCallbacks))
    {
        gtk_fixed_put(reinterpret_cast<GtkFixed*>(fixed.get()), wkView.get(), 0, 0);
        setBounds(currentBounds);

        MemoryOutputStream mo;

        mo << linuxJSInjection << "\n" << jsBootstrap;
        jsInjection = mo.toString();

        auto* wview = reinterpret_cast<WebKitWebView*>(wkView.get());

        g_signal_connect(wview, "load-changed", (GCallback) staticLoadChanged, this);
        g_signal_connect(wkView.get(), "size-allocate", (GCallback) staticSizeAllocateCallback, this);

        auto* manager = webkit_web_view_get_user_content_manager(wview);
        g_signal_connect(manager, "script-message-received::juceBridge", (GCallback) staticScriptMessageReceived, this);
        webkit_user_content_manager_register_script_message_handler(manager, "juceBridge");

        if (url.isDataScheme()) {
            String mimeType;
            auto htmlData = url.getURLEncodedData(mimeType);
            std::unique_ptr<GBytes, void (*)(GBytes*)> bytes(g_bytes_new(htmlData.getData(), htmlData.getSize()), g_bytes_unref);
            webkit_web_view_load_bytes(wview, bytes.get(), mimeType.toRawUTF8(), nullptr, nullptr);
        } else {
            webkit_web_view_load_uri (wview, url.toString(true).toRawUTF8());
        }
    }

    ~LinuxWebView() = default;

    //==============================================================================
    void setBounds(Rectangle<int> const& rc) override
    {
        gtk_widget_set_size_request(wkView.get(), rc.getWidth(), rc.getHeight());
        currentBounds = rc;
    }

    Rectangle<int> getBounds() override
    {
        return currentBounds;
    }

    void attachToParent(void* nativePtr) override {
        if (plug != nullptr) {
            jassertfalse;
            return;
        }

        auto x11parent = reinterpret_cast<::Window>(nativePtr);

        plug = std::unique_ptr<GtkWidget, void (*)(GtkWidget*)>(gtk_plug_new(x11parent), gtk_widget_destroy);
        gtk_container_add (reinterpret_cast<GtkContainer*>(plug.get()), fixed.get());
        gtk_widget_show_all (plug.get());
    }

    void detachFromParent() override {
        if (plug == nullptr) {
            jassertfalse;
            return;
        }

        gtk_container_remove(reinterpret_cast<GtkContainer*>(plug.get()), fixed.get());
        plug = nullptr;
    }

    void evalJS(String const& javascript) override
    {
        auto* wview = reinterpret_cast<WebKitWebView*>(wkView.get());
        webkit_web_view_run_javascript(wview, javascript.toRawUTF8(), nullptr, nullptr, nullptr);
    }
private:
    void scriptMessageReceived(WebKitUserContentManager*, WebKitJavascriptResult* wkResult) {
        if (! callbacks.messageReceived)
          return;

        auto* jsResult = webkit_javascript_result_get_js_value(wkResult);
        if (! jsc_value_is_string (jsResult)) {
            jassertfalse; // malformed user-message message
            return;
        }

        callbacks.messageReceived (jsc_value_to_string(jsResult));
    }

    static void staticScriptMessageReceived(WebKitUserContentManager* manager, WebKitJavascriptResult* msg, gpointer userData) {
        if (auto* _this = static_cast<LinuxWebView*>(userData))
            _this->scriptMessageReceived(manager, msg);
    }
    //==============================================================================
    void bootstrap(GAsyncResult*) {
        jsInjection.clear();

        if (callbacks.finishLoading)
            callbacks.finishLoading();
    }

    static void staticBootstrap (GObject*, GAsyncResult* res, gpointer userData)
    {
        if (auto* _this = static_cast<LinuxWebView*>(userData))
            _this->bootstrap(res);
    }

    //==============================================================================
    void loadChanged (WebKitLoadEvent event) {
        auto* wview = reinterpret_cast<WebKitWebView*>(wkView.get());
        if (event == WEBKIT_LOAD_FINISHED) {
            webkit_web_view_run_javascript(wview, jsInjection.toRawUTF8(), jsCancellable.get(), staticBootstrap, this);
        }
    }

    static void staticLoadChanged (WebKitWebView*, WebKitLoadEvent event, gpointer userData) {
        if (auto* _this = static_cast<LinuxWebView*>(userData))
            _this->loadChanged(event);
    }

    //==============================================================================
    void sizeAllocateCallback(GtkWidget*, GtkAllocation* allocation) {
        if (allocation != nullptr) {
            currentBounds = Rectangle<int>(0, 0, allocation->width, allocation->height);
        }
    }

    static void staticSizeAllocateCallback(GtkWidget *widget, GtkAllocation *allocation, gpointer userData) {
        if (auto* _this = static_cast<LinuxWebView*>(userData))
            _this->sizeAllocateCallback(widget, allocation);
    }

    //==============================================================================
    std::unique_ptr<GtkWidget, void (*)(GtkWidget*)> fixed;
    std::unique_ptr<GtkWidget, void (*)(GtkWidget*)> wkView;
    std::unique_ptr<GCancellable, void (*)(GCancellable*)> jsCancellable;
    String jsInjection;
    Rectangle<int> currentBounds;
    Callbacks callbacks;
    std::unique_ptr<GtkWidget, void (*)(GtkWidget*)> plug = {nullptr, gtk_widget_destroy};

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LinuxWebView)
};

std::unique_ptr<NativeWebView::Impl> NativeWebView::Impl::create(Rectangle<int> const& initialBounds,
                                                                 URL const& url,
                                                                 String const& jsBootstrap,
                                                                 NativeWebView::Impl::Callbacks && callbacks) {
    return std::make_unique<LinuxWebView> (initialBounds, url, jsBootstrap, std::move(callbacks));
}
}
