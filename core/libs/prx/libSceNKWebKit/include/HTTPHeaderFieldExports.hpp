#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/HTTPHeaderFieldAbi.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore15HTTPHeaderFieldC1EON3WTF6StringES3_(NKWebKit::WebCoreAbi::HTTPHeaderField* self, NKWebKit::WtfAbi::String* name, NKWebKit::WtfAbi::String* value);
void APS5_VABI _ZN7WebCore15HTTPHeaderFieldC1ERKS0_(NKWebKit::WebCoreAbi::HTTPHeaderField* self, const NKWebKit::WebCoreAbi::HTTPHeaderField* other);
void APS5_VABI _ZN7WebCore15HTTPHeaderFieldC2EON3WTF6StringES3_(NKWebKit::WebCoreAbi::HTTPHeaderField* self, NKWebKit::WtfAbi::String* name, NKWebKit::WtfAbi::String* value);
void APS5_VABI _ZN7WebCore15HTTPHeaderFieldC2ERKS0_(NKWebKit::WebCoreAbi::HTTPHeaderField* self, const NKWebKit::WebCoreAbi::HTTPHeaderField* other);
void APS5_VABI _ZN7WebCore15HTTPHeaderFieldD1Ev(NKWebKit::WebCoreAbi::HTTPHeaderField* self);
void APS5_VABI _ZN7WebCore15HTTPHeaderFieldD2Ev(NKWebKit::WebCoreAbi::HTTPHeaderField* self);
NKWebKit::WebCoreAbi::HTTPHeaderField& APS5_VABI _ZN7WebCore15HTTPHeaderFieldaSERKS0_(NKWebKit::WebCoreAbi::HTTPHeaderField* self, const NKWebKit::WebCoreAbi::HTTPHeaderField* other);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore15HTTPHeaderField4nameEv(const NKWebKit::WebCoreAbi::HTTPHeaderField* self);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore15HTTPHeaderField5valueEv(const NKWebKit::WebCoreAbi::HTTPHeaderField* self);

}
