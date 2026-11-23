#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/BidiContextAbi.hpp"

extern "C" {

NKWebKit::WebCoreAbi::BidiContextRef* APS5_VABI _ZN7WebCore11BidiContext6createEh14UCharDirectionbNS_19BidiEmbeddingSourceEPS0_(
    NKWebKit::WebCoreAbi::BidiContextRef* result, unsigned char level, std::int32_t direction, bool override, std::uint32_t source, NKWebKit::WebCoreAbi::BidiContext* parent);
NKWebKit::WebCoreAbi::BidiContextRef* APS5_VABI _ZN7WebCore11BidiContext41copyStackRemovingUnicodeEmbeddingContextsEv(
    NKWebKit::WebCoreAbi::BidiContextRef* result, NKWebKit::WebCoreAbi::BidiContext* self);

}
