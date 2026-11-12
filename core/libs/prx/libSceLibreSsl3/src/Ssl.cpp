#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/Reference.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

constexpr std::uint16_t Tls1Version = 0x0301;
constexpr std::uint16_t Tls11Version = 0x0302;
constexpr std::uint16_t Tls12Version = 0x0303;
constexpr std::uint16_t Tls13Version = 0x0304;
constexpr std::uint16_t Dtls1Version = 0xfeff;
constexpr std::uint16_t Dtls12Version = 0xfefd;

std::uint16_t TlsVersion(std::uint16_t dtlsVersion) {
    if (dtlsVersion == Dtls1Version) return Tls11Version;
    if (dtlsVersion == Dtls12Version) return Tls12Version;
    return 0;
}

std::uint16_t DtlsVersion(std::uint16_t tlsVersion) {
    if (tlsVersion == Tls11Version) return Dtls1Version;
    if (tlsVersion == Tls12Version) return Dtls12Version;
    return 0;
}

int SetVersionBound(const LibreSsl3SslMethod& method, std::uint16_t version, bool minimum, std::uint16_t otherBound,
                    std::uint16_t& tlsVersion, std::uint16_t& protoVersion) {
    if (version == 0) {
        tlsVersion = minimum ? method.minTlsVersion : method.maxTlsVersion;
        protoVersion = 0;
        return 1;
    }
    const bool dtls = method.dtls != 0;
    const auto requested = dtls ? TlsVersion(version) : version;
    if (requested == 0) return 0;
    const auto low = minimum ? requested : otherBound;
    const auto high = minimum ? otherBound : requested;
    if (method.minTlsVersion > method.maxTlsVersion || low > high) return 0;
    if (low > method.maxTlsVersion || high < method.minTlsVersion) return 0;
    const auto bound = minimum ? std::max(low, method.minTlsVersion) : std::min(high, method.maxTlsVersion);
    const auto proto = dtls ? DtlsVersion(bound) : bound;
    if (dtls && proto == 0) return 0;
    tlsVersion = bound;
    protoVersion = proto;
    return 1;
}

} // namespace

extern "C" {

int APS5_VABI SSL_up_ref_nid_postfix(LibreSsl3Ssl* ssl) {
    if (!ssl)
        throw std::invalid_argument("SSL_up_ref requires an SSL");
    return Retain_nid_no_patch(ssl->references);
}

const LibreSsl3SslMethod* APS5_VABI TLS_method_nid_postfix() {
    static constexpr LibreSsl3SslMethod method{0, 1, Tls13Version, Tls1Version, Tls13Version};
    return &method;
}

const LibreSsl3SslMethod* APS5_VABI TLS_server_method_nid_postfix() {
    return TLS_method_nid_postfix();
}

void APS5_VABI SSL_set_info_callback_nid_postfix(LibreSsl3Ssl* ssl, LibreSsl3InfoCallback callback) {
    if (!ssl)
        throw std::invalid_argument("SSL_set_info_callback requires an SSL");
    ssl->infoCallback = reinterpret_cast<void*>(callback);
}

int APS5_VABI SSL_SESSION_up_ref_nid_postfix(LibreSsl3SslSession* session) {
    if (!session)
        throw std::invalid_argument("SSL_SESSION_up_ref requires a session");
    return Retain_nid_no_patch(session->references);
}

const char* APS5_VABI SSL_get_version_nid_postfix(const LibreSsl3Ssl* ssl) {
    if (!ssl)
        throw std::invalid_argument("SSL_get_version requires an SSL");
    switch (ssl->version) {
    case Tls1Version: return "TLSv1";
    case Tls11Version: return "TLSv1.1";
    case Tls12Version: return "TLSv1.2";
    case Tls13Version: return "TLSv1.3";
    case Dtls1Version: return "DTLSv1";
    case Dtls12Version: return "DTLSv1.2";
    default: return "unknown";
    }
}

int APS5_VABI SSL_CTX_set_min_proto_version_nid_postfix(LibreSsl3SslCtx* context, std::uint16_t version) {
    if (!context || !context->method)
        throw std::invalid_argument("SSL_CTX_set_min_proto_version requires a context with a method");
    return SetVersionBound(*static_cast<const LibreSsl3SslMethod*>(context->method), version, true,
                           context->maxTlsVersion, context->minTlsVersion, context->minProtoVersion);
}

int APS5_VABI SSL_CTX_set_max_proto_version_nid_postfix(LibreSsl3SslCtx* context, std::uint16_t version) {
    if (!context || !context->method)
        throw std::invalid_argument("SSL_CTX_set_max_proto_version requires a context with a method");
    return SetVersionBound(*static_cast<const LibreSsl3SslMethod*>(context->method), version, false,
                           context->minTlsVersion, context->maxTlsVersion, context->maxProtoVersion);
}

std::int64_t APS5_VABI SSL_CTX_set_timeout_nid_postfix(LibreSsl3SslCtx* context, std::int64_t timeout) {
    if (!context) return 0;
    const auto previous = context->sessionTimeout;
    context->sessionTimeout = timeout;
    return previous;
}

std::size_t APS5_VABI SSL_get_client_random_nid_postfix(const LibreSsl3Ssl* ssl, unsigned char* out, std::size_t maxOut) {
    constexpr std::size_t randomSize = sizeof(LibreSsl3Ssl3State::clientRandom);
    if (!out) return randomSize;
    if (!ssl)
        throw std::invalid_argument("SSL_get_client_random requires an SSL");
    const auto length = std::min(randomSize, maxOut);
    if (length == 0) return 0;
    if (!ssl->s3)
        throw std::invalid_argument("SSL_get_client_random requires SSL3 state");
    std::memcpy(out, ssl->s3->clientRandom, length);
    return length;
}

void APS5_VABI SSL_CTX_set_keylog_callback_nid_postfix(LibreSsl3SslCtx* context, LibreSsl3KeylogCallback callback) {
    if (!context)
        throw std::invalid_argument("SSL_CTX_set_keylog_callback requires a context");
    context->keylogCallback = reinterpret_cast<void*>(callback);
}

}
