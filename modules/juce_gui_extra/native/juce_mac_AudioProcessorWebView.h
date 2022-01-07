/*
  ==============================================================================

   Copyright (c) 2021 - Fabian Renn-Giles, fabian.renn@gmail.com

  ==============================================================================
*/

namespace juce
{
#if JUCE_MAC || DOXYGEN

std::unique_ptr<NSView, NSObjectDeleter> createWebViewController(WebViewConfiguration const& userConfig,
                                                                 std::function<void (int width, int height)> && resizeCallback = {});

#endif

}
