#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/StorageMapAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

extern "C" {

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore10StorageMap3keyEj(NKWebKit::WebCoreAbi::StorageMap* self, unsigned index);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore10StorageMap7getItemERKN3WTF6StringE(const NKWebKit::WebCoreAbi::StorageMap* self, const NKWebKit::WtfAbi::String* key);
void APS5_VABI _ZN7WebCore10StorageMap7setItemERKN3WTF6StringES4_RS2_Rb(NKWebKit::WebCoreAbi::StorageMap* self, const NKWebKit::WtfAbi::String* key, const NKWebKit::WtfAbi::String* value, NKWebKit::WtfAbi::String* oldValue, bool* quotaException);
void APS5_VABI _ZN7WebCore10StorageMap20setItemIgnoringQuotaERKN3WTF6StringES4_(NKWebKit::WebCoreAbi::StorageMap* self, const NKWebKit::WtfAbi::String* key, const NKWebKit::WtfAbi::String* value);
void APS5_VABI _ZN7WebCore10StorageMap10removeItemERKN3WTF6StringERS2_(NKWebKit::WebCoreAbi::StorageMap* self, const NKWebKit::WtfAbi::String* key, NKWebKit::WtfAbi::String* oldValue);
bool APS5_VABI _ZNK7WebCore10StorageMap8containsERKN3WTF6StringE(const NKWebKit::WebCoreAbi::StorageMap* self, const NKWebKit::WtfAbi::String* key);

}
