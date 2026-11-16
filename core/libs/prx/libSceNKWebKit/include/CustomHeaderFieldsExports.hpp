#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/CustomHeaderFieldsAbi.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore18CustomHeaderFieldsC1Ev(NKWebKit::WebCoreAbi::CustomHeaderFields* self);
void APS5_VABI _ZN7WebCore18CustomHeaderFieldsC2Ev(NKWebKit::WebCoreAbi::CustomHeaderFields* self);
void APS5_VABI _ZN7WebCore18CustomHeaderFieldsC1ERKS0_(NKWebKit::WebCoreAbi::CustomHeaderFields* self, const NKWebKit::WebCoreAbi::CustomHeaderFields* other);
void APS5_VABI _ZN7WebCore18CustomHeaderFieldsC2ERKS0_(NKWebKit::WebCoreAbi::CustomHeaderFields* self, const NKWebKit::WebCoreAbi::CustomHeaderFields* other);
NKWebKit::WebCoreAbi::CustomHeaderFields& APS5_VABI _ZN7WebCore18CustomHeaderFieldsaSERKS0_(NKWebKit::WebCoreAbi::CustomHeaderFields* self, const NKWebKit::WebCoreAbi::CustomHeaderFields* other);
void APS5_VABI _ZN7WebCore18CustomHeaderFieldsD2Ev(NKWebKit::WebCoreAbi::CustomHeaderFields* self);

}
