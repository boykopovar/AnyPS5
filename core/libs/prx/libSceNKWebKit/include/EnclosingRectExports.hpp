#pragma once

#include "SceTypes.hpp"

namespace WebCore {
struct FloatRect;
struct IntRect;
struct LayoutRect;
}

extern "C" {
WebCore::IntRect APS5_VABI _ZN7WebCore16enclosingIntRectERKNS_9FloatRectE(const WebCore::FloatRect& rect);
WebCore::IntRect APS5_VABI _ZN7WebCore16enclosingIntRectERKNS_10LayoutRectE(const WebCore::LayoutRect& rect);
WebCore::LayoutRect APS5_VABI _ZN7WebCore19enclosingLayoutRectERKNS_9FloatRectE(const WebCore::FloatRect& rect);
WebCore::FloatRect APS5_VABI _ZN7WebCore25encloseRectToDevicePixelsERKNS_9FloatRectEf(const WebCore::FloatRect& rect, float deviceScaleFactor);
}
