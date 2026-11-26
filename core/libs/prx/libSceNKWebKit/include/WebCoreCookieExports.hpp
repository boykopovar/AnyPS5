#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/CookieAbi.hpp"

extern "C" {

unsigned APS5_VABI _ZNK7WebCore6Cookie4hashEv(const NKWebKit::WebCoreAbi::Cookie* self);
bool APS5_VABI _ZNK7WebCore6CookieeqERKS0_(const NKWebKit::WebCoreAbi::Cookie* self, const NKWebKit::WebCoreAbi::Cookie* other);

}
