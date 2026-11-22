#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstdint>

namespace NKWebKit::WebCoreAbi {

enum LocalLoadPolicy {
    AllowLocalLoadsForAll,
    AllowLocalLoadsForLocalAndSubstituteData,
    AllowLocalLoadsForLocalOnly,
};

static_assert(sizeof(LocalLoadPolicy) == 4);

bool SecurityPolicyRestrictAccessToLocal();
bool SecurityPolicyAllowSubstituteDataAccessToLocal();

}

extern "C" {

bool APS5_VABI _ZN7WebCore14SecurityPolicy18shouldHideReferrerERKN3WTF3URLERKNS1_6StringE(const NKWebKit::WtfAbi::URL* url, const NKWebKit::WtfAbi::String* referrer);
NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore14SecurityPolicy22generateReferrerHeaderENS_14ReferrerPolicyERKN3WTF3URLERKNS2_6StringE(std::uint8_t referrerPolicy, const NKWebKit::WtfAbi::URL* url, const NKWebKit::WtfAbi::String* referrer);
void APS5_VABI _ZN7WebCore14SecurityPolicy18setLocalLoadPolicyENS0_15LocalLoadPolicyE(NKWebKit::WebCoreAbi::LocalLoadPolicy policy);

}
