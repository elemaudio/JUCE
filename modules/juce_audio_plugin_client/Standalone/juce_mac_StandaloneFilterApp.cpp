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

#include <juce_audio_utils/juce_audio_utils.h>

#include <memory>
#include <juce_core/native/juce_mac_ObjCHelpers.h>
#include <AppKit/AppKit.h>

using namespace juce;

#include "juce_StandaloneAudio.h"

//==============================================================================
class StandalonePlugInApp
{
public:
    using ObjCClassType = NSObject<NSWindowDelegate, NSApplicationDelegate>;

    StandalonePlugInApp(ObjCClassType* _self)
       : objCInstance(_self), app([NSApplication sharedApplication]),
         audioProcessor(::createPluginFilter()),
         resizeCallback(std::make_shared<std::function<void (NativeWebView&, Rectangle<int> const&)>>([this] (NativeWebView& nv, Rectangle<int> const& rc) { webViewResizeCallback(nv, rc); }))
    {
        [app setDelegate:objCInstance];

        PropertiesFile::Options options;
        options.applicationName     = audioProcessor->getName();
        options.filenameSuffix      = "settings";
        options.osxLibrarySubFolder = "Preferences";

        appProperties.setStorageParameters(options);

        standaloneAudio = std::make_unique<StandaloneAudio>(*audioProcessor, appProperties, [this] (Rectangle<int> const& rc) { resizeSettingsWindow(rc); });

        setupMenuBar();
        setupWindow();
        setupSettings();
    }

    void quit() {
        if (standaloneAudio != nullptr)
            standaloneAudio->detachWebView();

        if (auto* webView = audioProcessor->getNativeWebView())
            webView->detachFromParent();

        if (settingsWindow.get() != nullptr) {
            [settingsWindow.get() close];
        }

        if (mainWindow.get() != nullptr) {
            [mainWindow.get() close];
        }
        
        isRunning = false;
        [app stop:objCInstance];
    }

    void openAudioSettings() {
        [settingsWindow.get() makeKeyAndOrderFront:objCInstance];
    }

    static std::unique_ptr<ObjCClassType, NSObjectDeleter> createInstance()
    {
        static Class cls;
        return std::unique_ptr<ObjCClassType, NSObjectDeleter> ([cls.createInstance() init]);
    }

    static StandalonePlugInApp* cobj(ObjCClassType* _self) {
        return Class::_this(_self);
    }

    bool isRunning = true;
private:
    void setupWindow()
    {
        if (auto* webView = audioProcessor->getNativeWebView())
        {
            audioProcessor->setResizeRequestCallback(resizeCallback);
            auto jbounds = webView->getBounds();
            auto contentRect = NSMakeRect(0, 0, jbounds.getWidth(), jbounds.getHeight());
            mainWindow.reset([[NSWindow alloc] initWithContentRect:contentRect
                                                         styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
                                                           backing:NSBackingStoreBuffered
                                                             defer:YES]);
            
            [mainWindow.get() setDelegate:objCInstance];
            auto* parentView = [mainWindow.get() contentView];
            webView->attachToParent(parentView);
            
            [[[parentView subviews] objectAtIndex:0] setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
            [mainWindow.get() setReleasedWhenClosed:NO];
            [mainWindow.get() center];
            [mainWindow.get() makeKeyAndOrderFront:objCInstance];
        }
        else
        {
            // huh... no UI?
            jassertfalse;
        }
    }

    void setupSettings() {
        auto& settingsView = standaloneAudio->getSettingsView();
        auto jbounds = settingsView.getBounds();
        auto contentRect = NSMakeRect(0, 0, jbounds.getWidth(), jbounds.getHeight());
        settingsWindow.reset([[NSWindow alloc] initWithContentRect:contentRect
                                                         styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
                                                           backing:NSBackingStoreBuffered
                                                             defer:YES]);
        [settingsWindow.get() setReleasedWhenClosed:NO];
        [settingsWindow.get() center];
        
        auto* parentView = [settingsWindow.get() contentView];
        settingsView.attachToParent(parentView);
    }

    void setupMenuBar()
    {
        std::unique_ptr<NSMenu, NSObjectDeleter> mainMenu ([[NSMenu alloc] init]);
        std::unique_ptr<NSMenuItem, NSObjectDeleter> appMenuItem([[NSMenuItem alloc] init]);
        [mainMenu.get() addItem:appMenuItem.get()];
        [app setMainMenu:mainMenu.get()];

        std::unique_ptr<NSMenu, NSObjectDeleter> appMenu ([[NSMenu alloc] init]);

        JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wundeclared-selector")
        std::unique_ptr<NSMenuItem, NSObjectDeleter> audioSettingsItem([[NSMenuItem alloc] initWithTitle:@"Audio Settings..."
                                                                                                  action:@selector(openAudioSettings)
                                                                                           keyEquivalent:@","]);
        std::unique_ptr<NSMenuItem, NSObjectDeleter> quitItem([[NSMenuItem alloc] initWithTitle:@"Quit"
                                                                                         action:@selector(quit)
                                                                                  keyEquivalent:@"q"]);
        JUCE_END_IGNORE_WARNINGS_GCC_LIKE
        [appMenu.get() addItem:audioSettingsItem.get()];
        [appMenu.get() addItem:quitItem.get()];
        [appMenuItem.get() setSubmenu:appMenu.get()];
    }
    
    //==============================================================================
    void webViewResizeCallback(NativeWebView& nv, Rectangle<int> const& rc)
    {
        auto oldFrame = [mainWindow.get() frame];
        auto deltaX = rc.getWidth() - oldFrame.size.width;
        auto deltaY = rc.getHeight() - oldFrame.size.height;
        [mainWindow.get() setFrame:NSMakeRect(oldFrame.origin.x - (deltaX / 2),
                                              oldFrame.origin.y - (deltaY / 2),
                                               rc.getWidth(), rc.getHeight())
                       display:YES];
    }

    void resizeSettingsWindow(Rectangle<int> const& rc)
    {
        auto oldFrame = [settingsWindow.get() frame];
        auto deltaX = rc.getWidth()  - oldFrame.size.width;
        auto deltaY = rc.getHeight() - oldFrame.size.height;
        [settingsWindow.get() setFrame:NSMakeRect(oldFrame.origin.x - (deltaX / 2),
                                                  oldFrame.origin.y - (deltaY / 2),
                                                   rc.getWidth(), rc.getHeight())
                       display:YES];
    }

    NSSize windowWillResize(NSWindow* window, NSSize const& toSize)
    {
        if (window != mainWindow.get() || audioProcessor == nullptr)
            return toSize;
        
        Rectangle<int> current([window frame].size.width, [window frame].size.height);
        Rectangle<int> requested(toSize.width, toSize.height);
        auto constrained = audioProcessor->webViewCheckSizeConstraint(current, requested);

        return NSMakeSize(constrained.getWidth(), constrained.getHeight());
    }

    //==============================================================================
    ObjCClassType* objCInstance;
    NSApplication* app;
    ScopedJuceInitialiser_GUI libraryInitialiser;
    ApplicationProperties appProperties;
    std::unique_ptr<AudioProcessor> audioProcessor;
    std::unique_ptr<StandaloneAudio> standaloneAudio;
    std::unique_ptr<NSWindow, NSObjectDeleter> mainWindow;
    std::unique_ptr<NSWindow, NSObjectDeleter> settingsWindow;
    std::shared_ptr<std::function<void (NativeWebView&, Rectangle<int> const&)>> resizeCallback;

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
            addMethod (@selector (openAudioSettings), _openAudioSettings);
            addMethod (@selector (quit), _quit);
            JUCE_END_IGNORE_WARNINGS_GCC_LIKE

            addMethod(@selector(windowWillResize:toSize:), _windowWillResize);

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
        static void _openAudioSettings(id self, SEL) { _this(self)->openAudioSettings(); }

        //==============================================================================
        static NSSize _windowWillResize(id self, SEL, NSWindow* window, NSSize toSize) { return _this(self)->windowWillResize(window, toSize); }
    };
};

//==============================================================================
int main() {
    {
        std::unique_ptr<NSObject<NSWindowDelegate, NSApplicationDelegate>, NSObjectDeleter> app;

        {
            std::unique_ptr<NSAutoreleasePool, NSObjectDeleter> pool([[NSAutoreleasePool alloc] init]);
            app = StandalonePlugInApp::createInstance();
        }

        while (StandalonePlugInApp::cobj(app.get())->isRunning) {
            std::unique_ptr<NSAutoreleasePool, NSObjectDeleter> pool([[NSAutoreleasePool alloc] init]);
            [NSApp run];
        }
    }

    return 0;
}
