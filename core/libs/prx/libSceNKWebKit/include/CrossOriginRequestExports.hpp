#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/HttpHeaderMapAbi.hpp"
#include "prx/libSceNKWebKit/src/ResourceRequestAbi.hpp"

#include <cstdint>

extern "C" {

bool APS5_VABI _ZN7WebCore32isSimpleCrossOriginAccessRequestERKN3WTF6StringERKNS_13HTTPHeaderMapE(
    const NKWebKit::WtfAbi::String& method, const NKWebKit::HttpAbi::HTTPHeaderMap& headers);

void APS5_VABI _ZN7WebCore39cleanHTTPRequestHeadersForAccessControlERNS_15ResourceRequestEN3WTF9OptionSetINS_29HTTPHeadersToKeepFromCleaningEEE(
    NKWebKit::WebCoreAbi::ResourceRequestBase* request, std::uint8_t headersToKeep);

}
