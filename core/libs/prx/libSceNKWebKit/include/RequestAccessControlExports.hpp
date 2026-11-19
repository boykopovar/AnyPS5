#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/ResourceRequestAbi.hpp"
#include "prx/libSceNKWebKit/src/SecurityOriginAbi.hpp"

#include <cstdint>

namespace WebCore {

enum class StoredCredentialsPolicy : std::uint8_t { DoNotUse, Use, EphemeralStateless };

}

extern "C" {

void APS5_VABI _ZN7WebCore29updateRequestForAccessControlERNS_15ResourceRequestERNS_14SecurityOriginENS_23StoredCredentialsPolicyE(
    NKWebKit::WebCoreAbi::ResourceRequestBase* request, NKWebKit::WebCoreAbi::SecurityOrigin* securityOrigin,
    WebCore::StoredCredentialsPolicy storedCredentialsPolicy);

}
