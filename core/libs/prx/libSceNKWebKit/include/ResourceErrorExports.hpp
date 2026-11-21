#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/ResourceErrorAbi.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore17ResourceErrorBase7setTypeENS0_4TypeE(NKWebKit::WebCoreAbi::ResourceError* self, NKWebKit::WebCoreAbi::ResourceErrorType type);
NKWebKit::WebCoreAbi::ResourceError APS5_VABI _ZNK7WebCore17ResourceErrorBase12isolatedCopyEv(const NKWebKit::WebCoreAbi::ResourceError* self);
void APS5_VABI _ZNK7WebCore17ResourceErrorBase8lazyInitEv(const NKWebKit::WebCoreAbi::ResourceError* self);

}
