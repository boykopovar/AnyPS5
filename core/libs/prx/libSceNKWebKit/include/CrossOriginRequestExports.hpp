#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/HttpHeaderMapAbi.hpp"

extern "C" {

bool APS5_VABI _ZN7WebCore32isSimpleCrossOriginAccessRequestERKN3WTF6StringERKNS_13HTTPHeaderMapE(
    const NKWebKit::WtfAbi::String& method, const NKWebKit::HttpAbi::HTTPHeaderMap& headers);

}
