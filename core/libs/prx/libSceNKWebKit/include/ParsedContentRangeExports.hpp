#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/ParsedContentRangeAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore18ParsedContentRangeC1ERKN3WTF6StringE(NKWebKit::WebCoreAbi::ParsedContentRange* self, const NKWebKit::WtfAbi::String* headerValue);
void APS5_VABI _ZN7WebCore18ParsedContentRangeC1Elll(NKWebKit::WebCoreAbi::ParsedContentRange* self, std::int64_t firstBytePosition, std::int64_t lastBytePosition, std::int64_t instanceLength);
void APS5_VABI _ZN7WebCore18ParsedContentRangeC2ERKN3WTF6StringE(NKWebKit::WebCoreAbi::ParsedContentRange* self, const NKWebKit::WtfAbi::String* headerValue);
void APS5_VABI _ZN7WebCore18ParsedContentRangeC2Elll(NKWebKit::WebCoreAbi::ParsedContentRange* self, std::int64_t firstBytePosition, std::int64_t lastBytePosition, std::int64_t instanceLength);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore18ParsedContentRange11headerValueEv(const NKWebKit::WebCoreAbi::ParsedContentRange* self);

}
