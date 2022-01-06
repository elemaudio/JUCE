/*
  ==============================================================================

   Copyright (c) 2021 - Fabian Renn-Giles, fabian.renn@gmail.com

  ==============================================================================
*/
#include "juce_mac_AudioProcessorWebView.h"

namespace
{

using namespace juce;

//==============================================================================
// JUCE bridge JavaScript injection
constexpr char javascriptInjection[] =
R"END(
var juceBridge = {
    postMessage: function (param) {
        return webkit.messageHandlers.juceBridge.postMessage(param);
    }
};
)END";

//==============================================================================
class ScriptMessageHandler
{
public:
    ScriptMessageHandler(NSObject<WKScriptMessageHandlerWithReply>* objcClassInstance)
        : objcInstance (objcClassInstance)
    {}
    
    static std::unique_ptr<NSObject<WKScriptMessageHandlerWithReply>, NSObjectDeleter> createInstance()
    {
        static Class cls;
        return std::unique_ptr<NSObject<WKScriptMessageHandlerWithReply>, NSObjectDeleter> ([cls.createInstance() init]);
    }
    
    std::function<void (WKScriptMessage*, void (^)(id, NSString *))> messageCallback;
    
private:
    void didReceiveScriptMessage(WKUserContentController*, WKScriptMessage* msg, void (^returnBlock)(id, NSString *))
    {
        if (messageCallback) {
            messageCallback(msg, returnBlock);
        } else {
            returnBlock(nullptr, @"Message unhandled");
        }
    }
    
    //==============================================================================
    NSObject<WKScriptMessageHandlerWithReply>* objcInstance;

public:
    //==============================================================================
    struct Class  : public ObjCClass<NSObject<WKScriptMessageHandlerWithReply>>
    {
        Class() : ObjCClass<NSObject<WKScriptMessageHandlerWithReply>> ("WKScriptMessageHandlerWithReply")
        {
            addIvar<ScriptMessageHandler*> ("cppObject");
            
            //==============================================================================
            addProtocol(@protocol(WKScriptMessageHandlerWithReply));
            
            //==============================================================================
            addMethod (@selector (init),    init);
            addMethod (@selector (dealloc), dealloc);
            
            //==============================================================================
            addMethod (@selector (userContentController:didReceiveScriptMessage:replyHandler:), _didReceiveScriptMessage);

            registerClass();
        }

        //==============================================================================
        static ScriptMessageHandler* _this (id self)                     { return getIvar<ScriptMessageHandler*> (self, "cppObject"); }
        static void setThis (id self, ScriptMessageHandler* cpp)         { object_setInstanceVariable  (self, "cppObject", cpp); }

        //==============================================================================
        static id init (id _self, SEL)
        {
            NSObject<WKScriptMessageHandlerWithReply>* self = _self;
            
            self = ObjCMsgSendSuper<NSObject, NSObject<WKScriptMessageHandlerWithReply>*> (self, @selector (init));

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
        static void _didReceiveScriptMessage(id self, SEL, WKUserContentController* controller, WKScriptMessage* msg, void (^returnBlock)(id, NSString*)) { _this(self)->didReceiveScriptMessage(controller, msg, returnBlock); }
    };
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScriptMessageHandler)
};

//==============================================================================
class WebkitView
{
public:
    WebkitView(WKWebView* objcClassInstance, WebViewConfiguration const& userConfig,
               std::unique_ptr<NSObject<WKScriptMessageHandlerWithReply>, NSObjectDeleter> && scriptMessageHandler)
        : objcInstance(objcClassInstance), juceConfig(userConfig), messageHandler (std::move (scriptMessageHandler))
    {
        ScriptMessageHandler::Class::_this(messageHandler.get())->messageCallback
            = [this] (WKScriptMessage* msg, void (^returnBlock)(id, NSString *))
              {
                  didReceiveScriptMessage (msg, returnBlock);
              };
    }
    
    ~WebkitView()
    {
        ScriptMessageHandler::Class::_this(messageHandler.get())->messageCallback = {};
        [[[objcInstance configuration] userContentController] removeScriptMessageHandlerForName:@"juceBridge"];
    }
    
    static std::unique_ptr<WKWebView, NSObjectDeleter> createInstance(WebViewConfiguration const& config)
    {
        static Class cls;
        WebViewConfiguration configCopy (config);
        
        JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wobjc-method-access")
        return std::unique_ptr<WKWebView, NSObjectDeleter> ([cls.createInstance() initWithWebViewConfiguration:&configCopy]);
        JUCE_END_IGNORE_WARNINGS_GCC_LIKE
    }
    
    void load()
    {
        ObjCMsgSendSuper<NSView, void> (objcInstance, @selector (viewDidMoveToSuperview));
        
        std::unique_ptr<NSURL, NSObjectDeleter> url ([[NSURL alloc] initWithString:juceStringToNS(juceConfig.url.toString(true))]);
        std::unique_ptr<NSURLRequest, NSObjectDeleter> req ([[NSURLRequest alloc] initWithURL:url.get()]);
        
        [objcInstance loadRequest:req.get()];
        
        if (juceConfig.onLoad)
            juceConfig.onLoad([this] (String const& script) { return executeScript (script); });
    }
    
private:
    //==============================================================================
    struct ScriptCompletion
    {
        ScriptCompletion(WebkitView& p) : parent(p) {}
        
        WebkitView& parent;
        std::promise<var> result;
        
        void completion(id obj, NSError* error)
        {
            if (error != nullptr) {
                try {
                    // TODO: create a bespoke javascript error exception type
                    throw std::runtime_error(nsStringToJuce([error localizedDescription]).toRawUTF8());
                } catch (...) {
                    try { result.set_exception(std::current_exception()); } catch (...) {}
                }
            } else {
                result.set_value(nsObjectToVar(obj));
            }
            
            // de-allocate myself
            parent.completions.erase(std::remove_if(parent.completions.begin(), parent.completions.end(),
                                                    [this] (std::unique_ptr<ScriptCompletion>& p) { return p.get() == this; }),
                                     parent.completions.end());
        }
    };
    
    std::future<var> executeScript(String const& script)
    {
        auto& completionObj = completions.emplace_back(new ScriptCompletion (*this));
        std::unique_ptr<NSString, NSObjectDeleter> nsScript([[NSString alloc] initWithUTF8String:script.toRawUTF8()]);
        [objcInstance evaluateJavaScript:nsScript.get() completionHandler:CreateObjCBlock(completionObj.get(), &ScriptCompletion::completion)];
        return completionObj->result.get_future();
    }
    
    //==============================================================================
    void didReceiveScriptMessage(WKScriptMessage* msg, void (^returnBlock)(id, NSString *))
    {
        if (juceConfig.onMessageReceived)
        {
            auto futureResult = juceConfig.onMessageReceived(nsObjectToVar([msg body]));
            
            // if the future isn't valid then we return void
            if (futureResult.valid())
            {
                if (futureResult.wait_for (std::chrono::seconds (0)) == std::future_status::timeout)
                {
                    // TODO: deal with asynchronous results
                }
                else
                {
                    returnBlock (varToNSObject (futureResult.get()).get(), nullptr);
                }
            }
            else
            {
                // a default constructed future object was returned: return void
                returnBlock(nullptr, nullptr);
            }
        }
        else
        {
            returnBlock(nullptr, @"Message unhandled");
        }
    }
    
    //==============================================================================
    void viewDidMoveToSuperview() {}
    
    //==============================================================================
    WKWebView* objcInstance;
    WebViewConfiguration juceConfig;
    std::unique_ptr<NSObject<WKScriptMessageHandlerWithReply>, NSObjectDeleter> messageHandler;
    std::vector<std::unique_ptr<ScriptCompletion>> completions;
    
    //==============================================================================
    struct Class  : public ObjCClass<WKWebView>
    {
        Class() : ObjCClass<WKWebView> ("WKWebView_")
        {
            addIvar<WebkitView*> ("cppObject");
            
            //==============================================================================
            JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wundeclared-selector")
            addMethod (@selector (initWithWebViewConfiguration:),                  initWithWebViewConfiguration);
            JUCE_END_IGNORE_WARNINGS_GCC_LIKE
            addMethod (@selector (dealloc),                                        dealloc);
            
            //==============================================================================
            addMethod (@selector (viewDidMoveToSuperview),                         _viewDidMoveToSuperview);

            registerClass();
        }

        //==============================================================================
        static WebkitView* _this (id self)                     { return getIvar<WebkitView*> (self, "cppObject"); }
        static void setThis (id self, WebkitView* cpp)         { object_setInstanceVariable  (self, "cppObject", cpp); }

        //==============================================================================
        static id initWithWebViewConfiguration (id _self, SEL, WebViewConfiguration* config)
        {
            WKWebView* self = _self;
            
            {
                std::unique_ptr<WKWebViewConfiguration, NSObjectDeleter> wkConfig ([[WKWebViewConfiguration alloc] init]);
                std::unique_ptr<WKUserContentController, NSObjectDeleter> userController([[WKUserContentController alloc] init]);
                auto scriptMessageHandler = ScriptMessageHandler::createInstance();
                
                [userController.get() addScriptMessageHandlerWithReply:scriptMessageHandler.get() contentWorld:[WKContentWorld pageWorld] name:@"juceBridge"];
                
                std::unique_ptr<NSString, NSObjectDeleter> nsStringScript([[NSString alloc] initWithUTF8String:javascriptInjection]);
                std::unique_ptr<WKUserScript, NSObjectDeleter> userScript([[WKUserScript alloc] initWithSource:nsStringScript.get()
                                                                                                 injectionTime:WKUserScriptInjectionTimeAtDocumentStart
                                                                                              forMainFrameOnly:NO]);
                [userController.get() addUserScript:userScript.get()];
                
                [wkConfig.get() setUserContentController:userController.get()];

                auto frame = CGRectMake(0, 0, config->size.getWidth(), config->size.getHeight());
                
                self = ObjCMsgSendSuper<WKWebView, WKWebView*, CGRect, WKWebViewConfiguration*> (self, @selector (initWithFrame:configuration:), frame, wkConfig.get());

                WebkitView* juceWK = new WebkitView (self, *config, std::move(scriptMessageHandler));
                setThis (self, juceWK);
                
                juceWK->load();
            }
            
            return self;
        }

        static void dealloc (id self, SEL)
        {
            delete _this (self);
            setThis (self, nullptr);
        }
        
        static void _viewDidMoveToSuperview(id self, SEL) { _this(self)->viewDidMoveToSuperview(); }
    };
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebkitView)
};
} // anonymous namespace

namespace juce
{

std::unique_ptr<NSView, NSObjectDeleter> createWebViewController(WebViewConfiguration const& userConfig)
{
    return WebkitView::createInstance(userConfig);
}

}
