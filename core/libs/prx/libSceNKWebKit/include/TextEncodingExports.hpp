#pragma once

#include "SceTypes.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceNKWebKit/src/TextEncodingAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

#include <cstddef>

extern "C" {

void APS5_VABI _ZN7WebCore12TextEncodingC1EPKc(WebCore::TextEncoding* self, const char* name);
void APS5_VABI _ZN7WebCore12TextEncodingC2EPKc(WebCore::TextEncoding* self, const char* name);
void APS5_VABI _ZN7WebCore12TextEncodingC1ERKN3WTF6StringE(WebCore::TextEncoding* self, const NKWebKit::WtfAbi::String* name);
void APS5_VABI _ZN7WebCore12TextEncodingC2ERKN3WTF6StringE(WebCore::TextEncoding* self, const NKWebKit::WtfAbi::String* name);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore12TextEncoding6decodeEPKcmbRb(const WebCore::TextEncoding* self, const char* data, std::size_t length, bool stopOnError, bool* sawError);
NKWebKit::WtfAbi::Vector<std::uint8_t> APS5_VABI _ZNK7WebCore12TextEncoding6encodeEN3WTF10StringViewENS_19UnencodableHandlingE(const WebCore::TextEncoding* self, NKWebKit::WtfAbi::StringView string, WebCore::UnencodableHandling handling);
const char* APS5_VABI _ZNK7WebCore12TextEncoding7domNameEv(const WebCore::TextEncoding* self);

}
