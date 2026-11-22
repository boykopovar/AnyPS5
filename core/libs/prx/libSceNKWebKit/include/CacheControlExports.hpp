#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/CacheControlAbi.hpp"
#include "prx/libSceNKWebKit/src/HttpHeaderMapAbi.hpp"
#include "prx/libSceNKWebKit/src/ResourceResponseAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfTimeTypes.hpp"

extern "C" {

NKWebKit::HttpAbi::CacheControlDirectives APS5_VABI _ZN7WebCore27parseCacheControlDirectivesERKNS_13HTTPHeaderMapE(const NKWebKit::HttpAbi::HTTPHeaderMap& headers);
WTF::Seconds APS5_VABI _ZN7WebCore17computeCurrentAgeERKNS_16ResourceResponseEN3WTF8WallTimeE(const NKWebKit::WebCoreAbi::ResourceResponseBase& response, WTF::WallTime responseTime);

}
