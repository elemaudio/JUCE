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
    
    using JSMessagePoster = std::function<void (String const&)>;
    
    std::function<void (JSMessagePoster &&)>   onLoad;
    std::function<void ()>                     onDestroy;
    std::function<void (String const&)>        onMessageReceived;
};

}

