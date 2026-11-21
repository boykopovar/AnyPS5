#pragma once

#include "SceTypes.hpp"

namespace WebCore {
struct FloatSize;
struct IntSize;
}

extern "C" {
void APS5_VABI _ZN7WebCore9FloatSizeC1ERKNS_7IntSizeE(WebCore::FloatSize* self, const WebCore::IntSize& size);
void APS5_VABI _ZN7WebCore9FloatSizeC2ERKNS_7IntSizeE(WebCore::FloatSize* self, const WebCore::IntSize& size);
}
