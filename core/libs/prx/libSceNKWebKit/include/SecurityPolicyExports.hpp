#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/SecurityOriginAbi.hpp"
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
bool SecurityPolicyIsAccessAllowed(const SecurityOrigin& activeOrigin, const SecurityOrigin& targetOrigin);

}

extern "C" {

bool APS5_VABI _ZN7WebCore14SecurityPolicy18shouldHideReferrerERKN3WTF3URLERKNS1_6StringE(const NKWebKit::WtfAbi::URL* url, const NKWebKit::WtfAbi::String* referrer);
void APS5_VABI _ZN7WebCore14SecurityPolicy18setLocalLoadPolicyENS0_15LocalLoadPolicyE(NKWebKit::WebCoreAbi::LocalLoadPolicy policy);
void APS5_VABI _ZN7WebCore14SecurityPolicy27resetOriginAccessAllowlistsEv();
void APS5_VABI _ZN7WebCore14SecurityPolicy29addOriginAccessAllowlistEntryERKNS_14SecurityOriginERKN3WTF6StringES7_b(const NKWebKit::WebCoreAbi::SecurityOrigin* sourceOrigin, const NKWebKit::WtfAbi::String* destinationProtocol, const NKWebKit::WtfAbi::String* destinationDomain, bool allowDestinationSubdomains);
void APS5_VABI _ZN7WebCore14SecurityPolicy32removeOriginAccessAllowlistEntryERKNS_14SecurityOriginERKN3WTF6StringES7_b(const NKWebKit::WebCoreAbi::SecurityOrigin* sourceOrigin, const NKWebKit::WtfAbi::String* destinationProtocol, const NKWebKit::WtfAbi::String* destinationDomain, bool allowDestinationSubdomains);

}
