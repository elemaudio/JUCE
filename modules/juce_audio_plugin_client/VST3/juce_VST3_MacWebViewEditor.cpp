/*
  ==============================================================================

   Copyright (c) 2021 - Fabian Renn-Giles, fabian.renn@gmail.com

  ==============================================================================
*/

#include <juce_core/system/juce_TargetPlatform.h>

#if JUCE_MAC
#define JUCE_VST3HEADERS_INCLUDE_HEADERS_ONLY 1

#include <juce_core/system/juce_CompilerWarnings.h>
#include <juce_audio_processors/format_types/juce_VST3Headers.h>

#include "../utility/juce_CheckSettingMacros.h"
#include "../utility/juce_IncludeSystemHeaders.h"
#include "../utility/juce_IncludeModuleHeaders.h"

#include <juce_core/native/juce_mac_ObjCHelpers.h>
#include <juce_gui_extra/native/juce_mac_AudioProcessorWebView.h>


using namespace juce;
using namespace Steinberg;

namespace
{


//==============================================================================
class MacWebViewEditor  : public Vst::EditorView
{
public:
    MacWebViewEditor (WebViewConfiguration && webConfig, Vst::EditController* ec, AudioProcessor& p, ViewRect& webViewBounds)
        : Vst::EditorView (ec, &webViewBounds),
          pluginInstance (p),
          webViewConfig (std::move(webConfig))
    {}

    REFCOUNT_METHODS (Vst::EditorView)

    //==============================================================================
    tresult PLUGIN_API isPlatformTypeSupported (FIDString type) override
    {
        if (type != nullptr && (! webViewConfig.url.isEmpty()))
        {
            if (strcmp (type, kPlatformTypeNSView) == 0)
                return kResultTrue;
        }

        return kResultFalse;
    }

    tresult PLUGIN_API attached (void* parent, FIDString type) override
    {
        if (parent == nullptr || isPlatformTypeSupported (type) == kResultFalse)
            return kResultFalse;

        auto parentView = static_cast<NSView*>(parent);
        webView = createWebViewController (webViewConfig,
                                           [this] (int width, int height)
                                           {
                                                if (plugFrame != nullptr) {
                                                    ViewRect rc (0, 0, width, height);
                                                    plugFrame->resizeView (this, &rc);
                                                }
                                           });
        
        [webView.get() setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
        auto parentBounds = [parentView bounds];
        [webView.get() setBounds:parentBounds];
        [parentView addSubview:webView.get()];
        

        return kResultTrue;
    }

    tresult PLUGIN_API removed() override
    {
        if (webView != nullptr) {
            [webView.get() removeFromSuperviewWithoutNeedingDisplay];
            webView = nullptr;
        }

        return CPluginView::removed();
    }

    tresult PLUGIN_API onSize (ViewRect* newSize) override
    {
        // do nothing: we resize with our parent view
        return kResultTrue;
    }

    tresult PLUGIN_API getSize (ViewRect* size) override
    {
        if (size != nullptr)
        {
            if (webView != nullptr) {
                auto bounds = [webView.get() bounds];
                *size = ViewRect (0, 0, bounds.size.width, bounds.size.height);
            } else {
                *size = ViewRect (0, 0, webViewConfig.size.getWidth(), webViewConfig.size.getHeight());
            }
            
            return kResultTrue;
        }

        return kResultFalse;
    }

    tresult PLUGIN_API canResize() override
    {
        return kResultTrue;
    }

    tresult PLUGIN_API checkSizeConstraint (ViewRect* rectToCheck) override
    {
        return kResultTrue;
    }

private:

    //==============================================================================
    ScopedJuceInitialiser_GUI libraryInitialiser;
    AudioProcessor& pluginInstance;
    WebViewConfiguration webViewConfig;
    std::unique_ptr<NSView, NSObjectDeleter> webView;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MacWebViewEditor)
};
}

//==============================================================================
namespace juce
{
Vst::EditorView* createVST3WebView(WebViewConfiguration && webConfig, Vst::EditController* ec, AudioProcessor& p, ViewRect& webViewBounds)
{
    return new MacWebViewEditor (std::move (webConfig), ec, p, webViewBounds);
}
}
#endif // JUCE_MAC
