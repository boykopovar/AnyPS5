#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/HttpHeaderMapAbi.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore13HTTPHeaderMap3addERKN3WTF6StringES4_(NKWebKit::HttpAbi::HTTPHeaderMap* self, const NKWebKit::WtfAbi::String& name, const NKWebKit::WtfAbi::String& value);
void APS5_VABI _ZN7WebCore13HTTPHeaderMap3setERKN3WTF6StringES4_(NKWebKit::HttpAbi::HTTPHeaderMap* self, const NKWebKit::WtfAbi::String& name, const NKWebKit::WtfAbi::String& value);
void APS5_VABI _ZN7WebCore13HTTPHeaderMap6appendERKN3WTF6StringES4_(NKWebKit::HttpAbi::HTTPHeaderMap* self, const NKWebKit::WtfAbi::String& name, const NKWebKit::WtfAbi::String& value);
bool APS5_VABI _ZN7WebCore13HTTPHeaderMap6removeENS_14HTTPHeaderNameE(NKWebKit::HttpAbi::HTTPHeaderMap* self, NKWebKit::HttpAbi::HTTPHeaderName name);
bool APS5_VABI _ZN7WebCore13HTTPHeaderMap6removeERKN3WTF6StringE(NKWebKit::HttpAbi::HTTPHeaderMap* self, const NKWebKit::WtfAbi::String& name);
void APS5_VABI _ZN7WebCore13HTTPHeaderMapC1Ev(NKWebKit::HttpAbi::HTTPHeaderMap* self);
void APS5_VABI _ZN7WebCore13HTTPHeaderMapC2Ev(NKWebKit::HttpAbi::HTTPHeaderMap* self);
NKWebKit::HttpAbi::ASCIILiteral APS5_VABI _ZN7WebCore20httpHeaderNameStringENS_14HTTPHeaderNameE(NKWebKit::HttpAbi::HTTPHeaderName name);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore13HTTPHeaderMap3getENS_14HTTPHeaderNameE(const NKWebKit::HttpAbi::HTTPHeaderMap* self, NKWebKit::HttpAbi::HTTPHeaderName name);
bool APS5_VABI _ZNK7WebCore13HTTPHeaderMap8containsENS_14HTTPHeaderNameE(const NKWebKit::HttpAbi::HTTPHeaderMap* self, NKWebKit::HttpAbi::HTTPHeaderName name);
bool APS5_VABI _ZNK7WebCore13HTTPHeaderMap8containsERKN3WTF6StringE(const NKWebKit::HttpAbi::HTTPHeaderMap* self, const NKWebKit::WtfAbi::String& name);

}
