#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfTextAbi.hpp"

#include <cstddef>

namespace NKWebKit::WebCoreAbi {

struct FloatBoxExtent {
    float top;
    float right;
    float bottom;
    float left;
};

static_assert(sizeof(FloatBoxExtent) == 16);
static_assert(offsetof(FloatBoxExtent, top) == 0);
static_assert(offsetof(FloatBoxExtent, right) == 4);
static_assert(offsetof(FloatBoxExtent, bottom) == 8);
static_assert(offsetof(FloatBoxExtent, left) == 12);
static_assert(sizeof(WebCore::FloatRoundedRect) == 48);

}

extern "C" {

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_16FloatRoundedRectE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::FloatRoundedRect& value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_9RectEdgesIfEE(NKWebKit::WtfAbi::TextStream& stream, const NKWebKit::WebCoreAbi::FloatBoxExtent& value);

}
