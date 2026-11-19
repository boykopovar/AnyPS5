#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"

struct CairoSurface;

namespace NKWebKit::Cairo {

struct SurfaceRefPtr {
    CairoSurface* m_ptr;
};

}

extern "C" {

WebCore::IntSize APS5_VABI _ZN7WebCore15nativeImageSizeERKN3WTF6RefPtrI14_cairo_surfaceNS0_13DumbPtrTraitsIS2_EEEE(
    const NKWebKit::Cairo::SurfaceRefPtr& image);

}
