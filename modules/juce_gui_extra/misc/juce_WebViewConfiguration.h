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
    
    using ExecuteJavascript = std::function<std::future<var> (String const&)>;
    
    std::function<void (ExecuteJavascript &&)>   onLoad;
    std::function<void ()>                       onDestroy;
    std::function<std::future<var> (var const&)> onMessageReceived;
};

}

