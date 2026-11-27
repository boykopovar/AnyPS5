#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/DisplayListNativeImageAbi.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore11DisplayList15DrawNativeImageC1ERKN3WTF6RefPtrI14_cairo_surfaceNS2_13DumbPtrTraitsIS4_EEEERKNS_9FloatSizeERKNS_9FloatRectESF_RKNS_20ImagePaintingOptionsE(NKWebKit::DisplayListNativeImageAbi::DrawNativeImage* self, const void* image, const WebCore::FloatSize* imageSize, const WebCore::FloatRect* destinationRect, const WebCore::FloatRect* sourceRect, const NKWebKit::DisplayListNativeImageAbi::ImagePaintingOptions* options);
void APS5_VABI _ZN7WebCore11DisplayList15DrawNativeImageC2ERKN3WTF6RefPtrI14_cairo_surfaceNS2_13DumbPtrTraitsIS4_EEEERKNS_9FloatSizeERKNS_9FloatRectESF_RKNS_20ImagePaintingOptionsE(NKWebKit::DisplayListNativeImageAbi::DrawNativeImage* self, const void* image, const WebCore::FloatSize* imageSize, const WebCore::FloatRect* destinationRect, const WebCore::FloatRect* sourceRect, const NKWebKit::DisplayListNativeImageAbi::ImagePaintingOptions* options);
void APS5_VABI _ZN7WebCore11DisplayList15DrawNativeImageD0Ev(NKWebKit::DisplayListNativeImageAbi::DrawNativeImage* self);
void APS5_VABI _ZN7WebCore11DisplayList15DrawNativeImageD1Ev(NKWebKit::DisplayListNativeImageAbi::DrawNativeImage* self);
void APS5_VABI _ZN7WebCore11DisplayList15DrawNativeImageD2Ev(NKWebKit::DisplayListNativeImageAbi::DrawNativeImage* self);

}
