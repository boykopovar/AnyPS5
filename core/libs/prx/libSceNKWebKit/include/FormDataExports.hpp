#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/FormDataAbi.hpp"
#include "prx/libSceNKWebKit/src/MediaTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"
#include <cstddef>
#include <cstdint>

extern "C" {

WebCore::Ref<NKWebKit::WebCoreAbi::FormData> APS5_VABI _ZN7WebCore8FormData6createEv();
WebCore::Ref<NKWebKit::WebCoreAbi::FormData> APS5_VABI _ZN7WebCore8FormData6createEPKvm(const void* data, std::size_t size);
void APS5_VABI _ZN7WebCore8FormData10appendDataEPKvm(NKWebKit::WebCoreAbi::FormData* self, const void* data, std::size_t size);
void APS5_VABI _ZN7WebCore8FormData10appendBlobERKN3WTF3URLE(NKWebKit::WebCoreAbi::FormData* self, const NKWebKit::WtfAbi::URL* blobURL);
void APS5_VABI _ZN7WebCore8FormDataD1Ev(NKWebKit::WebCoreAbi::FormData* self);
void APS5_VABI _ZN7WebCore8FormDataD2Ev(NKWebKit::WebCoreAbi::FormData* self);
WebCore::Ref<NKWebKit::WebCoreAbi::FormData> APS5_VABI _ZNK7WebCore8FormData12isolatedCopyEv(const NKWebKit::WebCoreAbi::FormData* self);
NKWebKit::WtfAbi::Vector<std::uint8_t> APS5_VABI _ZNK7WebCore8FormData7flattenEv(const NKWebKit::WebCoreAbi::FormData* self);
NKWebKit::WtfAbi::URL APS5_VABI _ZNK7WebCore8FormData9asBlobURLEv(const NKWebKit::WebCoreAbi::FormData* self);
std::uint64_t APS5_VABI _ZNK7WebCore8FormData13lengthInBytesEv(const NKWebKit::WebCoreAbi::FormData* self);

}
