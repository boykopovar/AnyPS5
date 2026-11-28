#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/MIMETypeCacheAbi.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore13MIMETypeCacheC1ERKS0_(NKWebKit::WebCoreAbi::MIMETypeCache* self, const NKWebKit::WebCoreAbi::MIMETypeCache* other);
void APS5_VABI _ZN7WebCore13MIMETypeCacheC2ERKS0_(NKWebKit::WebCoreAbi::MIMETypeCache* self, const NKWebKit::WebCoreAbi::MIMETypeCache* other);
void APS5_VABI _ZN7WebCore13MIMETypeCacheD2Ev(NKWebKit::WebCoreAbi::MIMETypeCache* self);
NKWebKit::WebCoreAbi::MIMETypeCache* APS5_VABI _ZN7WebCore13MIMETypeCacheaSERKS0_(NKWebKit::WebCoreAbi::MIMETypeCache* self, const NKWebKit::WebCoreAbi::MIMETypeCache* other);
std::uint8_t APS5_VABI _ZN7WebCore13MIMETypeCache13canDecodeTypeERKN3WTF6StringE(NKWebKit::WebCoreAbi::MIMETypeCache* self, const NKWebKit::WtfAbi::String* mimeType);
NKWebKit::WebCoreAbi::MIMETypeSet* APS5_VABI _ZN7WebCore13MIMETypeCache14supportedTypesEv(NKWebKit::WebCoreAbi::MIMETypeCache* self);
void APS5_VABI _ZN7WebCore13MIMETypeCache17addSupportedTypesERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(
    NKWebKit::WebCoreAbi::MIMETypeCache* self, const NKWebKit::WtfAbi::StringVector* types);
bool APS5_VABI _ZN7WebCore13MIMETypeCache21supportsContainerTypeERKN3WTF6StringE(NKWebKit::WebCoreAbi::MIMETypeCache* self, const NKWebKit::WtfAbi::String* containerType);

}
