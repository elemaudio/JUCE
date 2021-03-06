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

using namespace juce;

//==============================================================================
// Mac specific bootstrap code
constexpr char macJSInjection[] = R"END(
function juceBridgeInternalMessage(msg) {
    webkit.messageHandlers.juceBridge.postMessage(msg);
}
)END";

//==============================================================================
class ScriptMessageHandler
{
public:
    ScriptMessageHandler(NSObject<WKScriptMessageHandler>* objcClassInstance)
        : objcInstance (objcClassInstance)
    {}

    static std::unique_ptr<NSObject<WKScriptMessageHandler>, NSObjectDeleter> createInstance()
    {
        static Class cls;
        return std::unique_ptr<NSObject<WKScriptMessageHandler>, NSObjectDeleter> ([cls.createInstance() init]);
    }

    std::function<void (WKScriptMessage*)> messageCallback;

private:
    void didReceiveScriptMessage(WKUserContentController*, WKScriptMessage* msg)
    {
        if (messageCallback)
            messageCallback(msg);
    }

    //==============================================================================
    NSObject<WKScriptMessageHandler>* objcInstance;

public:
    //==============================================================================
    struct Class  : public ObjCClass<NSObject<WKScriptMessageHandler>>
    {
        Class() : ObjCClass<NSObject<WKScriptMessageHandler>> ("WKScriptMessageHandler")
        {
            addIvar<ScriptMessageHandler*> ("cppObject");

            //==============================================================================
            addProtocol(@protocol(WKScriptMessageHandler));

            //==============================================================================
            addMethod (@selector (init),    init);
            addMethod (@selector (dealloc), dealloc);

            //==============================================================================
            addMethod (@selector (userContentController:didReceiveScriptMessage:), _didReceiveScriptMessage);

            registerClass();
        }

        //==============================================================================
        static ScriptMessageHandler* _this (id self)                     { return getIvar<ScriptMessageHandler*> (self, "cppObject"); }
        static void setThis (id self, ScriptMessageHandler* cpp)         { object_setInstanceVariable  (self, "cppObject", cpp); }

        //==============================================================================
        static id init (id _self, SEL)
        {
            NSObject<WKScriptMessageHandler>* self = _self;

            self = ObjCMsgSendSuper<NSObject, NSObject<WKScriptMessageHandler>*> (self, @selector (init));

            ScriptMessageHandler* juceHandler = new ScriptMessageHandler (self);
            setThis (self, juceHandler);

            return self;
        }

        static void dealloc (id self, SEL)
        {
            delete _this (self);
            setThis (self, nullptr);
        }

        //==============================================================================
        static void _didReceiveScriptMessage(id self, SEL, WKUserContentController* controller, WKScriptMessage* msg) { _this(self)->didReceiveScriptMessage(controller, msg); }
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScriptMessageHandler)
};

//==============================================================================
class WebkitView
{
public:
    WebkitView(WKWebView* objcClassInstance,
               bool keyboardFocus,
               std::unique_ptr<NSObject<WKScriptMessageHandler>, NSObjectDeleter> && scriptMessageHandler,
               std::function<void (String const&)> && messageReceivedCb)
        : objcInstance(objcClassInstance), wantsKeyboardFocus(keyboardFocus),
          messageHandler (std::move (scriptMessageHandler)),
          messageReceived (std::move (messageReceivedCb))
    {
        ScriptMessageHandler::Class::_this(messageHandler.get())->messageCallback
            = [this] (WKScriptMessage* msg)
              {
                  didReceiveScriptMessage (msg);
              };
    }

    ~WebkitView()
    {
        ScriptMessageHandler::Class::_this(messageHandler.get())->messageCallback = {};
        [[[objcInstance configuration] userContentController] removeScriptMessageHandlerForName:@"juceBridge"];
    }

    static std::unique_ptr<WKWebView, NSObjectDeleter> createInstance(Rectangle<int> const& initialBounds,
                                                                      URL const& url,
                                                                      bool keyboardFocus,
                                                                      String const& jsBootstrap,
                                                                      std::function<void (String const&)> && messageReceivedCb)
    {
        InitializationParams params(initialBounds, url, keyboardFocus, jsBootstrap, std::move(messageReceivedCb));

        JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wobjc-method-access")
        return std::unique_ptr<WKWebView, NSObjectDeleter> ([Class::get().createInstance() initWithInitializationParams:&params]);
        JUCE_END_IGNORE_WARNINGS_GCC_LIKE
    }

    void load(URL const& url)
    {
        std::unique_ptr<NSURL, NSObjectDeleter> nsUrl ([[NSURL alloc] initWithString:juceStringToNS(url.toString(true))]);
        std::unique_ptr<NSURLRequest, NSObjectDeleter> req ([[NSURLRequest alloc] initWithURL:nsUrl.get()]);

        [objcInstance loadRequest:req.get()];
    }

    void setBounds(Rectangle<int> const& newBounds) {
        [objcInstance setFrameSize:CGSizeMake(newBounds.getWidth(), newBounds.getHeight())];
    }

    Rectangle<int> getBounds() {
        auto bounds = [objcInstance frame];
        return Rectangle<int>(static_cast<int>(bounds.origin.x), static_cast<int>(bounds.origin.y),
                              static_cast<int>(bounds.size.width), static_cast<int>(bounds.size.height));
    }

    void evalJS(String const& javascript)
    {
        std::unique_ptr<NSString, NSObjectDeleter> nsJS ([[NSString alloc] initWithUTF8String:javascript.toRawUTF8()]);
        [objcInstance evaluateJavaScript:nsJS.get() completionHandler:nullptr];
    }

    static WebkitView* cppObject(WKWebView* obj) {
        return Class::_this(obj);
    }

private:
    //==============================================================================
    void didReceiveScriptMessage(WKScriptMessage* msg)
    {
        if (! [[msg body] isKindOfClass: [NSString class]]) {
            return;
        }

        auto arg = nsStringToJuce((NSString*)[msg body]);

        if (messageReceived)
            messageReceived(arg);
    }

    //==============================================================================
    bool acceptsFirstResponder()
    {
        return wantsKeyboardFocus;
    }
    
    void keyDown(NSEvent* event)
    {
        if (pluginHostType.isAbletonLive()) {
            // Don't let Ableton Live process the ESC key event as it will close the
            // WekKitView while the ESC key is being processed. Grab this key event
            // before it gets to the WebKitView and do the same thing that Ableton
            // does when hitting ESC (i.e. closing the plug-in window).
            if([event keyCode] == 53) {
                [[objcInstance window] performClose:objcInstance];
                return;
            }
        }
        
        if (wantsKeyboardFocus) {
            // pass on the event
            ObjCMsgSendSuper<WKWebView, void> (objcInstance, @selector (keyDown:), event);
        } else {
            auto* nextResponder = [objcInstance nextResponder];
            
            if (nextResponder != nullptr)
                [nextResponder keyDown:event];
        }
    }

    //==============================================================================
    WKWebView* objcInstance;
    bool wantsKeyboardFocus;
    std::unique_ptr<NSObject<WKScriptMessageHandler>, NSObjectDeleter> messageHandler;
    std::function<void (String const&)> messageReceived;
    PluginHostType pluginHostType;

    //==============================================================================
    struct InitializationParams {
        InitializationParams(Rectangle<int> const& bounds, URL const& u, bool k, String const& bootstrap, std::function<void (String const&)> && cb)
            : initialBounds(bounds), url(u), keyboardFocus(k), jsBootstrap(bootstrap), messageReceivedCb(std::move(cb))
        {}

        Rectangle<int> const& initialBounds;
        URL const& url;
        bool keyboardFocus;
        String const& jsBootstrap;
        std::function<void (String const&)> messageReceivedCb;
    };

    //==============================================================================
    struct Class  : public ObjCClass<WKWebView>
    {
        Class() : ObjCClass<WKWebView> ("WKWebView_")
        {
            addIvar<WebkitView*> ("cppObject");

            //==============================================================================
            JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wundeclared-selector")
            addMethod (@selector (initWithInitializationParams:),   initWithInitializationParams);
            JUCE_END_IGNORE_WARNINGS_GCC_LIKE
            addMethod (@selector (dealloc),                                        dealloc);

            //==============================================================================
            addMethod (@selector (acceptsFirstResponder),                          _acceptsFirstResponder);
            addMethod (@selector (keyDown:),                          _keyDown);

            registerClass();
        }

        //==============================================================================
        static WebkitView* _this (id self)                     { return getIvar<WebkitView*> (self, "cppObject"); }
        static void setThis (id self, WebkitView* cpp)         { object_setInstanceVariable  (self, "cppObject", cpp); }

        //==============================================================================
        static id initWithInitializationParams (id _self, SEL,
                                                InitializationParams* paramsPtr)
        {
            WKWebView* self = _self;
            InitializationParams params(std::move(*paramsPtr));

            {
                std::unique_ptr<WKWebViewConfiguration, NSObjectDeleter> wkConfig ([[WKWebViewConfiguration alloc] init]);
                std::unique_ptr<WKUserContentController, NSObjectDeleter> userController([[WKUserContentController alloc] init]);
                auto scriptMessageHandler = ScriptMessageHandler::createInstance();


                [userController.get() addScriptMessageHandler:scriptMessageHandler.get() name:@"juceBridge"];

                MemoryOutputStream mo;
                mo << macJSInjection << "\n" << params.jsBootstrap;

                std::unique_ptr<NSString, NSObjectDeleter> nsStringScript([[NSString alloc] initWithUTF8String:mo.toString().toRawUTF8()]);
                std::unique_ptr<WKUserScript, NSObjectDeleter> userScript([[WKUserScript alloc] initWithSource:nsStringScript.get()
                                                                                                 injectionTime:WKUserScriptInjectionTimeAtDocumentStart
                                                                                              forMainFrameOnly:NO]);
                [userController.get() addUserScript:userScript.get()];

                [wkConfig.get() setUserContentController:userController.get()];

                auto frame = CGRectMake(0, 0, params.initialBounds.getWidth(), params.initialBounds.getHeight());

                self = ObjCMsgSendSuper<WKWebView, WKWebView*, CGRect, WKWebViewConfiguration*> (self, @selector (initWithFrame:configuration:), frame, wkConfig.get());

                WebkitView* juceWK = new WebkitView (self, params.keyboardFocus, std::move (scriptMessageHandler), std::move (params.messageReceivedCb));
                setThis (self, juceWK);

                juceWK->load(params.url);

            }

            return self;
        }

        static void dealloc (id self, SEL)
        {
            delete _this (self);
            setThis (self, nullptr);
        }

        static Class& get()
        {
            static Class cls;
            return cls;
        }

        static BOOL _acceptsFirstResponder(id self, SEL) { return _this(self)->acceptsFirstResponder() ? YES : NO; }
        static void _keyDown(id self, SEL, NSEvent* event) { return _this(self)->keyDown(event); }
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebkitView)
};

//==============================================================================
class MacWebView  : public NativeWebView::Impl
{
public:
    MacWebView(Rectangle<int> const& initialBounds,
               URL const& url,
               bool wantsKeyboardFocus,
               String const& jsBootstrap,
               std::function<void (String const&)> && messageReceived)
        : webView(WebkitView::createInstance(initialBounds, url, wantsKeyboardFocus, jsBootstrap, std::move(messageReceived)))
    {}

    void setBounds(Rectangle<int> const& newBounds) override {
        WebkitView::cppObject(webView.get())->setBounds(newBounds);
    }

    Rectangle<int> getBounds() override {
        return WebkitView::cppObject(webView.get())->getBounds();
    }

    void evalJS(String const& javascript) override {
        WebkitView::cppObject(webView.get())->evalJS(javascript);
    }
    
    void attachToParent(void* nativeView) override {
        [static_cast<NSView*>(nativeView) addSubview:webView.get()];
    }
    
    void detachFromParent() override {
        [webView.get() removeFromSuperview];
    }
    
    void* getNativeView() override {
        return webView.get();
    }
private:
    std::unique_ptr<WKWebView, NSObjectDeleter> webView;
};
} // anonymous namespace

std::unique_ptr<NativeWebView::Impl> NativeWebView::Impl::create(Rectangle<int> const& initialBounds,
                                                                 URL const& url,
                                                                 bool wantsKeyboardFocus,
                                                                 String const& jsBootstrap,
                                                                 std::function<void (String const&)> && messageReceived) {
    return std::make_unique<MacWebView>(initialBounds, url, wantsKeyboardFocus, jsBootstrap, std::move(messageReceived));
}
