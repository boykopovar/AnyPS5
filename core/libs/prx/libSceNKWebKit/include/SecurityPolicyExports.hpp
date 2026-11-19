#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

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
void APS5_VABI _ZN7WebCore14SecurityPolicy18setLocalLoadPolicyENS0_15LocalLoadPolicyE(NKWebKit::WebCoreAbi::LocalLoadPolicy policy);

}
