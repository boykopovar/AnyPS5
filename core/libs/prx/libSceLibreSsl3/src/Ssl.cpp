#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/Reference.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>

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

template <typename T>
bool CopyGuestInPreservingErrno(T& destination, const T* source) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&destination, source, sizeof(T));
    errno = savedErrno;
    return ok;
}

void CopyGuestBytesIn(void* destination, const void* source, std::size_t bytes, const char* function) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!ok)
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void CopyGuestBytesOut(void* destination, const void* source, std::size_t bytes, const char* function) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!ok)
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

template <typename T>
T ReadGuest(const T* source, const char* function) {
    T value{};
    CopyGuestBytesIn(&value, source, sizeof(T), function);
    return value;
}

template <typename T>
void WriteGuest(T* destination, const std::type_identity_t<T>& value, const char* function) {
    CopyGuestBytesOut(destination, &value, sizeof(T), function);
}

template <typename TObject, typename TField>
TField ReadField(const TObject* object, TField TObject::* field, const char* function) {
    return ReadGuest(&(object->*field), function);
}

template <typename TObject, typename TField>
void WriteField(TObject* object, TField TObject::* field, const std::type_identity_t<TField>& value,
                const char* function) {
    WriteGuest(&(object->*field), value, function);
}

bool ValidGuestAlpnList(const std::uint8_t* list, std::uint32_t length, const char* function) {
    std::array<std::uint8_t, 4096> chunk;
    std::uint32_t next = 0;
    for (std::uint32_t offset = 0; offset < length;) {
        const auto size = std::min<std::uint32_t>(chunk.size(), length - offset);
        CopyGuestBytesIn(chunk.data(), list + offset, size, function);
        offset += size;
        while (next < offset) {
            const std::uint32_t nameLength = chunk[next - (offset - size)];
            if (nameLength == 0 || nameLength > length - next - 1)
                return false;
            next += nameLength + 1;
        }
    }
    return true;
}

int SetContextVersionBound(LibreSsl3SslCtx* context, std::uint16_t version, bool minimum, const char* function) {
    const auto* method =
        context ? static_cast<const LibreSsl3SslMethod*>(ReadField(context, &LibreSsl3SslCtx::method, function)) : nullptr;
    if (!method)
        throw std::invalid_argument(std::string(function) + " requires a context with a method");
    const LibreSsl3SslMethod methodCopy = ReadGuest(method, function);
    static_assert(offsetof(LibreSsl3SslCtx, maxProtoVersion) - offsetof(LibreSsl3SslCtx, minTlsVersion) == 3 * sizeof(std::uint16_t));
    std::array<std::uint16_t, 4> versions{};
    CopyGuestBytesIn(versions.data(), &context->minTlsVersion, sizeof(versions), function);
    auto& [minTls, maxTls, minProto, maxProto] = versions;
    const int result = minimum ? SetVersionBound(methodCopy, version, true, maxTls, minTls, minProto)
                               : SetVersionBound(methodCopy, version, false, minTls, maxTls, maxProto);
    if (!result) return 0;
    CopyGuestBytesOut(&context->minTlsVersion, versions.data(), sizeof(versions), function);
    return 1;
}

template <typename T>
int RetainGuest(T* object, const char* function) {
    std::int32_t* const references = &object->references;
    std::int32_t current{};
    if (!CopyGuestInPreservingErrno(current, references) || !GuestWritable_nid_no_patch(references, sizeof(current)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return Retain_nid_no_patch(*references);
}

} // namespace

extern "C" {

int APS5_VABI SSL_up_ref_nid_postfix(LibreSsl3Ssl* ssl) {
    if (!ssl)
        throw std::invalid_argument("SSL_up_ref requires an SSL");
    return RetainGuest(ssl, __func__);
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

int APS5_VABI SSL_set_alpn_protos_nid_postfix(LibreSsl3Ssl* ssl, const std::uint8_t* protos, std::uint32_t protosLength) {
    if (!ssl)
        throw std::invalid_argument("SSL_set_alpn_protos requires an SSL");
    if (!protos)
        protosLength = 0;
    if (protosLength > 0 && !ValidGuestAlpnList(protos, protosLength, __func__))
        return 1;
    auto* const previous = ReadField(ssl, &LibreSsl3Ssl::alpnClientProtoList, __func__);
    WriteField(ssl, &LibreSsl3Ssl::alpnClientProtoList, nullptr, __func__);
    WriteField(ssl, &LibreSsl3Ssl::alpnClientProtoListLength, 0, __func__);
    free_nid_postfix(previous);
    if (protosLength == 0)
        return 0;
    auto* copy = static_cast<std::uint8_t*>(malloc_nid_postfix(protosLength));
    if (!copy)
        return 1;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(copy, protos, protosLength);
    errno = savedErrno;
    if (!copied) {
        free_nid_postfix(copy);
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    WriteField(ssl, &LibreSsl3Ssl::alpnClientProtoList, copy, __func__);
    WriteField(ssl, &LibreSsl3Ssl::alpnClientProtoListLength, protosLength, __func__);
    return 0;
}

int APS5_VABI SSL_SESSION_up_ref_nid_postfix(LibreSsl3SslSession* session) {
    if (!session)
        throw std::invalid_argument("SSL_SESSION_up_ref requires a session");
    return RetainGuest(session, __func__);
}

const char* APS5_VABI SSL_get_version_nid_postfix(const LibreSsl3Ssl* ssl) {
    if (!ssl)
        throw std::invalid_argument("SSL_get_version requires an SSL");
    switch (ReadField(ssl, &LibreSsl3Ssl::version, __func__)) {
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
    return SetContextVersionBound(context, version, true, __func__);
}

int APS5_VABI SSL_CTX_set_max_proto_version_nid_postfix(LibreSsl3SslCtx* context, std::uint16_t version) {
    return SetContextVersionBound(context, version, false, __func__);
}

std::int64_t APS5_VABI SSL_CTX_set_timeout_nid_postfix(LibreSsl3SslCtx* context, std::int64_t timeout) {
    if (!context) return 0;
    const auto previous = ReadField(context, &LibreSsl3SslCtx::sessionTimeout, __func__);
    WriteField(context, &LibreSsl3SslCtx::sessionTimeout, timeout, __func__);
    return previous;
}

int APS5_VABI SSL_CTX_set_quic_method_nid_postfix(LibreSsl3SslCtx* context, const void* quicMethod) {
    const auto* method =
        context ? static_cast<const LibreSsl3SslMethod*>(ReadField(context, &LibreSsl3SslCtx::method, __func__)) : nullptr;
    if (!method)
        throw std::invalid_argument("SSL_CTX_set_quic_method requires a context with a method");
    if (ReadGuest(&method->dtls, __func__)) return 0;
    WriteField(context, &LibreSsl3SslCtx::quicMethod, quicMethod, __func__);
    return 1;
}

std::size_t APS5_VABI SSL_get_client_random_nid_postfix(const LibreSsl3Ssl* ssl, unsigned char* out, std::size_t maxOut) {
    constexpr std::size_t randomSize = sizeof(LibreSsl3Ssl3State::clientRandom);
    if (!out) return randomSize;
    if (!ssl)
        throw std::invalid_argument("SSL_get_client_random requires an SSL");
    const auto length = std::min(randomSize, maxOut);
    if (length == 0) return 0;
    const auto* state = ReadField(ssl, &LibreSsl3Ssl::s3, __func__);
    if (!state)
        throw std::invalid_argument("SSL_get_client_random requires SSL3 state");
    unsigned char random[randomSize];
    CopyGuestBytesIn(random, state->clientRandom, randomSize, __func__);
    CopyGuestBytesOut(out, random, length, __func__);
    return length;
}

void APS5_VABI SSL_CTX_set_keylog_callback_nid_postfix(LibreSsl3SslCtx* context, LibreSsl3KeylogCallback callback) {
    if (!context)
        throw std::invalid_argument("SSL_CTX_set_keylog_callback requires a context");
    WriteField(context, &LibreSsl3SslCtx::keylogCallback, reinterpret_cast<void*>(callback), __func__);
}

}
