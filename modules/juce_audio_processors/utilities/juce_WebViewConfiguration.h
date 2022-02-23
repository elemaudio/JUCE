/*
  ==============================================================================

   Copyright (c) 2021 - Fabian Renn-Giles, fabian.renn@gmail.com

  ==============================================================================
*/

namespace juce
{

struct WebViewConfiguration
{
    URL url;
    Rectangle<int> size;
    bool wantsKeyboardFocus = false;
    // TODO: likely more options to come
};

}
