#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/DOMRectListAbi.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore11DOMRectListC1ERKN3WTF6VectorINS_9FloatQuadELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(NKWebKit::WebCoreAbi::DOMRectList* self, const NKWebKit::WtfAbi::Vector<WebCore::FloatQuad>* quads);
void APS5_VABI _ZN7WebCore11DOMRectListC1ERKN3WTF6VectorINS_9FloatRectELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(NKWebKit::WebCoreAbi::DOMRectList* self, const NKWebKit::WtfAbi::Vector<WebCore::FloatRect>* rects);
void APS5_VABI _ZN7WebCore11DOMRectListC2ERKN3WTF6VectorINS_9FloatQuadELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(NKWebKit::WebCoreAbi::DOMRectList* self, const NKWebKit::WtfAbi::Vector<WebCore::FloatQuad>* quads);
void APS5_VABI _ZN7WebCore11DOMRectListC2ERKN3WTF6VectorINS_9FloatRectELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(NKWebKit::WebCoreAbi::DOMRectList* self, const NKWebKit::WtfAbi::Vector<WebCore::FloatRect>* rects);
void APS5_VABI _ZN7WebCore11DOMRectListD1Ev(NKWebKit::WebCoreAbi::DOMRectList* self);
void APS5_VABI _ZN7WebCore11DOMRectListD2Ev(NKWebKit::WebCoreAbi::DOMRectList* self);

}
