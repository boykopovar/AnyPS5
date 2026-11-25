#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/HttpHeaderMapAbi.hpp"
#include "prx/libSceNKWebKit/src/ResourceRequestAbi.hpp"
#include "prx/libSceNKWebKit/src/SecurityOriginAbi.hpp"

#include <cstdint>

namespace NKWebKit::CorsAbi {

inline constexpr std::uint8_t KeepContentType = 1 << 0;
inline constexpr std::uint8_t KeepReferer = 1 << 1;
inline constexpr std::uint8_t KeepOrigin = 1 << 2;
inline constexpr std::uint8_t KeepUserAgent = 1 << 3;
inline constexpr std::uint8_t KeepAcceptEncoding = 1 << 4;
inline constexpr std::uint8_t KeepCacheControl = 1 << 5;

inline constexpr std::uint8_t StoredCredentialsUse = 1;

}

extern "C" {

bool APS5_VABI _ZN7WebCore32isSimpleCrossOriginAccessRequestERKN3WTF6StringERKNS_13HTTPHeaderMapE(
    const NKWebKit::WtfAbi::String& method, const NKWebKit::HttpAbi::HTTPHeaderMap& headers);
void APS5_VABI _ZN7WebCore39cleanHTTPRequestHeadersForAccessControlERNS_15ResourceRequestEN3WTF9OptionSetINS_29HTTPHeadersToKeepFromCleaningEEE(
    NKWebKit::WebCoreAbi::ResourceRequestBase* request, std::uint8_t headersToKeep);
void APS5_VABI _ZN7WebCore29updateRequestForAccessControlERNS_15ResourceRequestERNS_14SecurityOriginENS_23StoredCredentialsPolicyE(
    NKWebKit::WebCoreAbi::ResourceRequestBase* request, NKWebKit::WebCoreAbi::SecurityOrigin* securityOrigin, std::uint8_t storedCredentialsPolicy);

}
