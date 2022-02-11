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

#include <juce_core/system/juce_TargetPlatform.h>
#include "../utility/juce_CheckSettingMacros.h"

#include "../utility/juce_IncludeSystemHeaders.h"
#include "../utility/juce_IncludeModuleHeaders.h"

#include <memory>
#include <juce_core/native/juce_mac_ObjCHelpers.h>
#include <AppKit/AppKit.h>

using namespace juce;

//==============================================================================
class StandalonePlugInApp
{
public:
    using ObjCClassType = NSObject<NSApplicationDelegate>;

    StandalonePlugInApp(ObjCClassType* _self)
       : objCInstance(_self), app([NSApplication sharedApplication]),
         audioProcessor(::createPluginFilter()),
         webConfig(audioProcessor->getEditorWebViewConfiguration())
    {
        [app setDelegate:objCInstance];
        setupMenuBar();

        if (! webConfig.url.isEmpty()) {
            setupWindow();
        } else {
            // no UI?
            jassertfalse;
        }
    }

    void quit() {
        [app stop:objCInstance];
    }

    static std::unique_ptr<ObjCClassType, NSObjectDeleter> createInstance()
    {
        static Class cls;
        return std::unique_ptr<ObjCClassType, NSObjectDeleter> ([cls.createInstance() init]);
    }
private:
    void setupWindow()
    {
        auto contentRect = NSMakeRect(0, 0, webConfig.size.getWidth(), webConfig.size.getHeight());
        mainWindow.reset([[NSWindow alloc] initWithContentRect:contentRect
                                                     styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
                                                       backing:NSBackingStoreBuffered
                                                         defer:YES]);

        nativeWebView.reset(new NativeWebView([mainWindow.get() contentView], webConfig,
                                              [this] (NativeWebView& nv, int w, int h)
                                              {
                                                  auto oldFrame = [mainWindow.get() frame];
                                                  auto deltaX = w - oldFrame.size.width;
                                                  auto deltaY = w - oldFrame.size.width;
                                                  [mainWindow.get() setFrame:NSMakeRect(oldFrame.origin.x - (deltaX / 2),
                                                                                        oldFrame.origin.y - (deltaY / 2),
                                                                                        w, h)
                                                                     display:YES];
                                              }));
        [[[[mainWindow.get() contentView] subviews] objectAtIndex:0] setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
        [mainWindow.get() center];
        [mainWindow.get() makeKeyAndOrderFront:objCInstance];
    }

    void setupMenuBar()
    {
        std::unique_ptr<NSMenu, NSObjectDeleter> mainMenu ([[NSMenu alloc] init]);
        std::unique_ptr<NSMenuItem, NSObjectDeleter> appMenuItem([[NSMenuItem alloc] init]);
        [mainMenu.get() addItem:appMenuItem.get()];
        [app setMainMenu:mainMenu.get()];

        std::unique_ptr<NSMenu, NSObjectDeleter> appMenu ([[NSMenu alloc] init]);

        JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wundeclared-selector")
        std::unique_ptr<NSMenuItem, NSObjectDeleter> quitItem([[NSMenuItem alloc] initWithTitle:@"Quit"
                                                                                         action:@selector(quit)
                                                                                  keyEquivalent:@"q"]);
        JUCE_END_IGNORE_WARNINGS_GCC_LIKE
        [appMenu.get() addItem:quitItem.get()];
        [appMenuItem.get() setSubmenu:appMenu.get()];
    }

    //==============================================================================
    ObjCClassType* objCInstance;
    NSApplication* app;
    ScopedJuceInitialiser_GUI libraryInitialiser;
    std::unique_ptr<AudioProcessor> audioProcessor;
    WebViewConfiguration webConfig;
    std::unique_ptr<NativeWebView> nativeWebView;
    std::unique_ptr<NSWindow, NSObjectDeleter> mainWindow;

    //==============================================================================
    struct Class  : public ObjCClass<ObjCClassType>
    {
        Class() : ObjCClass<ObjCClassType> ("StandalonePlugInApp")
        {
            addIvar<StandalonePlugInApp*> ("cppObject");

            //==============================================================================
            addProtocol(@protocol(NSMenuDelegate));

            //==============================================================================
            addMethod (@selector (init),    init);
            addMethod (@selector (dealloc), dealloc);

            //==============================================================================
            JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wundeclared-selector")
            addMethod (@selector (quit), _quit);
            JUCE_END_IGNORE_WARNINGS_GCC_LIKE

            registerClass();
        }

        //==============================================================================
        static StandalonePlugInApp* _this (id self)                     { return getIvar<StandalonePlugInApp*> (self, "cppObject"); }
        static void setThis (id self, StandalonePlugInApp* cpp)         { object_setInstanceVariable  (self, "cppObject", cpp); }

        //==============================================================================
        static id init (id _self, SEL)
        {
            ObjCClassType* self = _self;

            self = ObjCMsgSendSuper<NSObject, ObjCClassType*> (self, @selector (init));

            StandalonePlugInApp* juceHandler = new StandalonePlugInApp (self);
            setThis (self, juceHandler);

            return self;
        }

        static void dealloc (id self, SEL)
        {
            delete _this (self);
            setThis (self, nullptr);
        }

        //==============================================================================
        static void _quit(id self, SEL) { _this(self)->quit(); }
    };
};

//==============================================================================
int main() {
    std::unique_ptr<NSAutoreleasePool, NSObjectDeleter> pool([[NSAutoreleasePool alloc] init]);
    auto app = StandalonePlugInApp::createInstance();
    [NSApp run];

    return 0;
}
