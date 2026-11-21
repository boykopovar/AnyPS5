#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfRefPtr.hpp"

extern "C" {

WebCore::IntSize APS5_VABI _ZN7WebCore15nativeImageSizeERKN3WTF6RefPtrI14_cairo_surfaceNS0_13DumbPtrTraitsIS2_EEEE(const NKWebKit::WtfAbi::RefPtr<CairoSurface>* image);

}
