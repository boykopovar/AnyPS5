#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

namespace {

constexpr std::uint32_t kServerTypeHTTP = 1;
constexpr std::uint32_t kServerTypeHTTPS = 2;
constexpr std::uint32_t kServerTypeFTP = 3;
constexpr std::uint32_t kServerTypeFTPS = 4;
constexpr std::uint32_t kProxyTypeHTTP = 5;
constexpr std::uint32_t kProxyTypeHTTPS = 6;
constexpr std::uint32_t kProxyTypeFTP = 7;
constexpr std::uint32_t kProxyTypeSOCKS = 8;

constexpr std::uint32_t kSchemeDefault = 1;
constexpr std::uint32_t kSchemeHTTPBasic = 2;
constexpr std::uint32_t kSchemeHTTPDigest = 3;
constexpr std::uint32_t kSchemeHTMLForm = 4;
constexpr std::uint32_t kSchemeNTLM = 5;
constexpr std::uint32_t kSchemeNegotiate = 6;
constexpr std::uint32_t kSchemeClientCertificateRequested = 7;
constexpr std::uint32_t kSchemeServerTrustEvaluationRequested = 8;
constexpr std::uint32_t kSchemeOAuth = 9;
constexpr std::uint32_t kSchemeUnknown = 100;

bool IsProxy(NKWebKit::ProtectionSpaceServerType type) {
    return type == NKWebKit::ProtectionSpaceServerType::ProxyHTTP
        || type == NKWebKit::ProtectionSpaceServerType::ProxyHTTPS
        || type == NKWebKit::ProtectionSpaceServerType::ProxyFTP
        || type == NKWebKit::ProtectionSpaceServerType::ProxySOCKS;
}

const void* CopyString(const NKWebKit::Retained<NKWebKit::String>& string) {
    if (!string) return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string()));
    return NKWebKit::ToRef(NKWebKit::Retained<NKWebKit::String>::Share(string.Get()));
}

}  // namespace

extern "C" {

const void* APS5_VABI WKProtectionSpaceCopyHost(const void* protectionSpaceRef) {
    const auto* space = NKWebKit::Cast<NKWebKit::ProtectionSpace>(protectionSpaceRef, __func__);
    return CopyString(space->host);
}

const void* APS5_VABI WKProtectionSpaceCopyRealm(const void* protectionSpaceRef) {
    const auto* space = NKWebKit::Cast<NKWebKit::ProtectionSpace>(protectionSpaceRef, __func__);
    return CopyString(space->realm);
}

int APS5_VABI WKProtectionSpaceGetPort(const void* protectionSpaceRef) {
    const auto* space = NKWebKit::Cast<NKWebKit::ProtectionSpace>(protectionSpaceRef, __func__);
    return space->port;
}

bool APS5_VABI WKProtectionSpaceGetIsProxy(const void* protectionSpaceRef) {
    const auto* space = NKWebKit::Cast<NKWebKit::ProtectionSpace>(protectionSpaceRef, __func__);
    return IsProxy(space->serverType);
}

std::uint32_t APS5_VABI WKProtectionSpaceGetServerType(const void* protectionSpaceRef) {
    const auto* space = NKWebKit::Cast<NKWebKit::ProtectionSpace>(protectionSpaceRef, __func__);
    switch (space->serverType) {
    case NKWebKit::ProtectionSpaceServerType::HTTP: return kServerTypeHTTP;
    case NKWebKit::ProtectionSpaceServerType::HTTPS: return kServerTypeHTTPS;
    case NKWebKit::ProtectionSpaceServerType::FTP: return kServerTypeFTP;
    case NKWebKit::ProtectionSpaceServerType::FTPS: return kServerTypeFTPS;
    case NKWebKit::ProtectionSpaceServerType::ProxyHTTP: return kProxyTypeHTTP;
    case NKWebKit::ProtectionSpaceServerType::ProxyHTTPS: return kProxyTypeHTTPS;
    case NKWebKit::ProtectionSpaceServerType::ProxyFTP: return kProxyTypeFTP;
    case NKWebKit::ProtectionSpaceServerType::ProxySOCKS: return kProxyTypeSOCKS;
    }
    return kServerTypeHTTP;
}

bool APS5_VABI WKProtectionSpaceGetReceivesCredentialSecurely(const void* protectionSpaceRef) {
    const auto* space = NKWebKit::Cast<NKWebKit::ProtectionSpace>(protectionSpaceRef, __func__);
    return space->serverType == NKWebKit::ProtectionSpaceServerType::HTTPS
        || space->serverType == NKWebKit::ProtectionSpaceServerType::FTPS
        || space->serverType == NKWebKit::ProtectionSpaceServerType::ProxyHTTPS
        || space->authenticationScheme == NKWebKit::ProtectionSpaceAuthenticationScheme::HTTPDigest;
}

std::uint32_t APS5_VABI WKProtectionSpaceGetAuthenticationScheme(const void* protectionSpaceRef) {
    const auto* space = NKWebKit::Cast<NKWebKit::ProtectionSpace>(protectionSpaceRef, __func__);
    switch (space->authenticationScheme) {
    case NKWebKit::ProtectionSpaceAuthenticationScheme::Default: return kSchemeDefault;
    case NKWebKit::ProtectionSpaceAuthenticationScheme::HTTPBasic: return kSchemeHTTPBasic;
    case NKWebKit::ProtectionSpaceAuthenticationScheme::HTTPDigest: return kSchemeHTTPDigest;
    case NKWebKit::ProtectionSpaceAuthenticationScheme::HTMLForm: return kSchemeHTMLForm;
    case NKWebKit::ProtectionSpaceAuthenticationScheme::NTLM: return kSchemeNTLM;
    case NKWebKit::ProtectionSpaceAuthenticationScheme::Negotiate: return kSchemeNegotiate;
    case NKWebKit::ProtectionSpaceAuthenticationScheme::ClientCertificateRequested: return kSchemeClientCertificateRequested;
    case NKWebKit::ProtectionSpaceAuthenticationScheme::ServerTrustEvaluationRequested: return kSchemeServerTrustEvaluationRequested;
    case NKWebKit::ProtectionSpaceAuthenticationScheme::OAuth: return kSchemeOAuth;
    default: return kSchemeUnknown;
    }
}

}
