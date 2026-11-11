#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/CacheControlAbi.hpp"
#include "prx/libSceNKWebKit/src/HttpHeaderMapAbi.hpp"

extern "C" {

NKWebKit::HttpAbi::CacheControlDirectives APS5_VABI _ZN7WebCore27parseCacheControlDirectivesERKNS_13HTTPHeaderMapE(const NKWebKit::HttpAbi::HTTPHeaderMap& headers);

}
