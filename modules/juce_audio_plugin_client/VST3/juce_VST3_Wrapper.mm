/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2020 - Raw Material Software Limited

   JUCE is an open source library subject to commercial or open-source
   licensing.

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

#include <juce_core/system/juce_TargetPlatform.h>
#include <tuple>

#if JUCE_MAC
#if JucePlugin_Build_VST || JucePlugin_Build_VST3

#include "../utility/juce_IncludeSystemHeaders.h"


namespace juce
{

void attachNSViewToParent(void* parentView, void* childView)
{
    [static_cast<NSView*>(parentView) addSubview:static_cast<NSView*>(childView)];
}

void detachNSViewFromParent(void* view)
{
    [static_cast<NSView*>(view) removeFromSuperview];
}

void setNSViewFrameSize(void* view, std::tuple<int, int, int, int>& bounds)
{
    auto [x, y, width, height] = bounds;
    [static_cast<NSView*>(view) setFrameSize:CGSizeMake(width, height)];
}

std::tuple<int, int, int, int> getNSViewFrameSize(void* view)
{
    auto bounds = [static_cast<NSView*>(view) frame];

    return std::make_tuple(
        static_cast<int>(bounds.origin.x),
        static_cast<int>(bounds.origin.y),
        static_cast<int>(bounds.size.width),
        static_cast<int>(bounds.size.height));
}

} // namespace juce

#endif
#endif
