#pragma once

#include "prx/libSceNKWebKit/src/Types.hpp"
#include <cstdint>
#include <utility>

namespace NKWebKit {

enum class ProtectionSpaceServerType : std::uint8_t {
    HTTP = 1,
    HTTPS = 2,
    FTP = 3,
    FTPS = 4,
    ProxyHTTP = 5,
    ProxyHTTPS = 6,
    ProxyFTP = 7,
    ProxySOCKS = 8,
};

enum class ProtectionSpaceAuthenticationScheme : std::uint8_t {
    Default = 1,
    HTTPBasic = 2,
    HTTPDigest = 3,
    HTMLForm = 4,
    NTLM = 5,
    Negotiate = 6,
    ClientCertificateRequested = 7,
    ServerTrustEvaluationRequested = 8,
    OAuth = 9,
    Unknown = 100,
};

class ProtectionSpace final : public ObjectOf<ObjectType::ProtectionSpace> {
public:
    ProtectionSpace() = default;
    ProtectionSpace(Retained<String> host, int port, ProtectionSpaceServerType serverType, Retained<String> realm, ProtectionSpaceAuthenticationScheme authenticationScheme, Retained<CertificateInfo> certificateInfo = {})
        : host(std::move(host)), realm(std::move(realm)), port(port), serverType(serverType), authenticationScheme(authenticationScheme), certificateInfo(std::move(certificateInfo)) {}

    Retained<String> host;
    Retained<String> realm;
    int port = 0;
    ProtectionSpaceServerType serverType = ProtectionSpaceServerType::HTTP;
    ProtectionSpaceAuthenticationScheme authenticationScheme = ProtectionSpaceAuthenticationScheme::Default;
    Retained<CertificateInfo> certificateInfo;
};

}  // namespace NKWebKit
