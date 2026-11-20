#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/DragImageAbi.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore9DragImageC1EOS0_(NKWebKit::WebCoreAbi::DragImage* self, NKWebKit::WebCoreAbi::DragImage* other);
void APS5_VABI _ZN7WebCore9DragImageC1Ev(NKWebKit::WebCoreAbi::DragImage* self);
void APS5_VABI _ZN7WebCore9DragImageC2EOS0_(NKWebKit::WebCoreAbi::DragImage* self, NKWebKit::WebCoreAbi::DragImage* other);
void APS5_VABI _ZN7WebCore9DragImageC2Ev(NKWebKit::WebCoreAbi::DragImage* self);
void APS5_VABI _ZN7WebCore9DragImageD1Ev(NKWebKit::WebCoreAbi::DragImage* self);
void APS5_VABI _ZN7WebCore9DragImageD2Ev(NKWebKit::WebCoreAbi::DragImage* self);
NKWebKit::WebCoreAbi::DragImage& APS5_VABI _ZN7WebCore9DragImageaSEOS0_(NKWebKit::WebCoreAbi::DragImage* self, NKWebKit::WebCoreAbi::DragImage* other);

}
