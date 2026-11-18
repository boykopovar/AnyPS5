#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <initializer_list>
#include <source_location>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace {

using Bytes = std::vector<unsigned char>;

constexpr std::int32_t Tls12Version = 0x0303;
constexpr std::int64_t SessionTime = 1700000000;
constexpr std::int64_t SessionTimeout = 7200;
constexpr std::int64_t SessionVerifyResult = 20;
constexpr std::int64_t DefaultTimeout = 3;

std::unordered_map<void*, std::size_t> allocations;
int allocationsUntilFailure = -1;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 SSL_SESSION DER check failed at line %u\n", location.line());
    std::abort();
}

template <typename Callable>
void RequireNotImplemented(Callable call, std::source_location location = std::source_location::current()) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return;
    }
    Require(false, location);
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (allocationsUntilFailure == 0) {
        allocationsUntilFailure = -1;
        return nullptr;
    }
    if (allocationsUntilFailure > 0) --allocationsUntilFailure;
    void* result = std::malloc(size == 0 ? 1 : size);
    Require(result != nullptr);
    allocations.emplace(result, size);
    return result;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    Require(allocations.erase(pointer) == 1);
    std::free(pointer);
}

void* APS5_VABI Callocate_nid_no_patch(std::size_t count, std::size_t size) {
    if (allocationsUntilFailure == 0) {
        allocationsUntilFailure = -1;
        return nullptr;
    }
    if (allocationsUntilFailure > 0) --allocationsUntilFailure;
    void* result = std::calloc(count == 0 ? 1 : count, size == 0 ? 1 : size);
    Require(result != nullptr);
    allocations.emplace(result, count * size);
    return result;
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (!pointer) return Allocate_nid_no_patch(size);
    Require(allocations.erase(pointer) == 1);
    void* result = std::realloc(pointer, size == 0 ? 1 : size);
    Require(result != nullptr);
    allocations.emplace(result, size);
    return result;
}

void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) {
    throw std::runtime_error("unexpected aligned allocation");
}

void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) {
    throw std::runtime_error("unexpected aligned reallocation");
}

int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) {
    throw std::runtime_error("unexpected posix_memalign");
}

constexpr std::uint64_t Error(int reason) {
    return 0x0dfff000u | static_cast<std::uint64_t>(reason);
}

void RequireQueue(std::initializer_list<int> reasons, std::source_location location = std::source_location::current()) {
    for (const int reason : reasons) Require(ERR_get_error_nid_postfix() == Error(reason), location);
    Require(ERR_get_error_nid_postfix() == 0, location);
}

void DrainErrors() {
    while (ERR_get_error_nid_postfix() != 0) {}
}

Bytes Hex(const char* text) {
    Bytes bytes;
    for (std::size_t index = 0; text[index] != '\0'; index += 2) {
        const char pair[3] = {text[index], text[index + 1], '\0'};
        bytes.push_back(static_cast<unsigned char>(std::strtoul(pair, nullptr, 16)));
    }
    return bytes;
}

Bytes Join(std::initializer_list<Bytes> parts) {
    Bytes result;
    for (const Bytes& part : parts) result.insert(result.end(), part.begin(), part.end());
    return result;
}

Bytes LengthPrefix(std::size_t length) {
    if (length < 0x80) return Bytes{static_cast<unsigned char>(length)};
    Bytes bytes;
    for (std::size_t remaining = length; remaining != 0; remaining >>= 8)
        bytes.insert(bytes.begin(), static_cast<unsigned char>(remaining));
    bytes.insert(bytes.begin(), static_cast<unsigned char>(0x80 | bytes.size()));
    return bytes;
}

Bytes Tlv(unsigned char tag, const Bytes& content) {
    Bytes result{tag};
    const Bytes length = LengthPrefix(content.size());
    result.insert(result.end(), length.begin(), length.end());
    result.insert(result.end(), content.begin(), content.end());
    return result;
}

Bytes Sequence(const Bytes& content) {
    return Tlv(0x30, content);
}

Bytes Integer(std::uint64_t value) {
    Bytes content;
    for (int shift = 56; shift >= 0; shift -= 8) {
        const auto byte = static_cast<unsigned char>(value >> shift);
        if (content.empty() && byte == 0) continue;
        content.push_back(byte);
    }
    if (content.empty()) content.push_back(0);
    if ((content.front() & 0x80) != 0) content.insert(content.begin(), 0);
    return Tlv(0x02, content);
}

Bytes Range(unsigned char first, std::size_t count) {
    Bytes bytes;
    for (std::size_t index = 0; index < count; ++index) bytes.push_back(static_cast<unsigned char>(first + index));
    return bytes;
}

Bytes MinimalEncoding() {
    return Hex("3013020101020203030402000004000400a4020400");
}

Bytes MinimalContent() {
    return Join({
        Integer(1),
        Integer(Tls12Version),
        Tlv(0x04, Bytes{0x00, 0x00}),
        Tlv(0x04, Bytes{}),
        Tlv(0x04, Bytes{}),
        Tlv(0xa4, Tlv(0x04, Bytes{})),
    });
}

LibreSsl3SslSession MinimalSession() {
    LibreSsl3SslSession session{};
    session.sslVersion = Tls12Version;
    return session;
}

LibreSsl3SslSession FullSession() {
    LibreSsl3SslSession session{};
    session.sslVersion = Tls12Version;
    session.masterKeyLength = sizeof(session.masterKey);
    for (std::size_t index = 0; index < sizeof(session.masterKey); ++index)
        session.masterKey[index] = static_cast<unsigned char>(index);
    session.sessionIdLength = sizeof(session.sessionId);
    for (std::size_t index = 0; index < sizeof(session.sessionId); ++index)
        session.sessionId[index] = static_cast<unsigned char>(0xa0 + index);
    session.sidCtxLength = sizeof(session.sidCtx);
    for (std::size_t index = 0; index < sizeof(session.sidCtx); ++index)
        session.sidCtx[index] = static_cast<unsigned char>(0xc0 + index);
    session.verifyResult = SessionVerifyResult;
    session.timeout = SessionTimeout;
    session.time = SessionTime;
    session.references = 3;
    session.peerCertType = 4;
    return session;
}

Bytes ExpectedFullEncoding() {
    return Sequence(Join({
        Integer(1),
        Integer(Tls12Version),
        Tlv(0x04, Bytes{0x00, 0x00}),
        Tlv(0x04, Range(0xa0, 32)),
        Tlv(0x04, Range(0x00, 48)),
        Tlv(0xa1, Integer(SessionTime)),
        Tlv(0xa2, Integer(SessionTimeout)),
        Tlv(0xa4, Tlv(0x04, Range(0xc0, 32))),
        Tlv(0xa5, Integer(SessionVerifyResult)),
    }));
}

Bytes Encode(LibreSsl3SslSession& session) {
    const int length = i2d_SSL_SESSION_nid_postfix(&session, nullptr);
    Require(length > 0);
    Bytes encoded(static_cast<std::size_t>(length));
    unsigned char* cursor = encoded.data();
    Require(i2d_SSL_SESSION_nid_postfix(&session, &cursor) == length);
    Require(cursor == encoded.data() + encoded.size());
    return encoded;
}

LibreSsl3SslSession* Decode(const Bytes& der, const unsigned char*& cursor) {
    cursor = der.data();
    return d2i_SSL_SESSION_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(der.size()));
}

void RequireRejected(const Bytes& der, int reason, std::source_location location = std::source_location::current()) {
    DrainErrors();
    const unsigned char* cursor = der.data();
    LibreSsl3SslSession* session = d2i_SSL_SESSION_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(der.size()));
    Require(session == nullptr, location);
    Require(cursor == der.data(), location);
    RequireQueue({reason}, location);
    Require(allocations.empty(), location);
}

void TestMinimalExactBytes() {
    LibreSsl3SslSession session = MinimalSession();
    Require(Encode(session) == MinimalEncoding());
}

void TestIntegerBoundaries() {
    LibreSsl3SslSession session = MinimalSession();
    session.time = 0x00ff000000000000;
    const Bytes expected = Sequence(Join({
        Integer(1),
        Integer(Tls12Version),
        Tlv(0x04, Bytes{0x00, 0x00}),
        Tlv(0x04, Bytes{}),
        Tlv(0x04, Bytes{}),
        Tlv(0xa1, Join({Bytes{0x02, 0x08}, Hex("00ff000000000000")})),
        Tlv(0xa4, Tlv(0x04, Bytes{})),
    }));
    Require(Encode(session) == expected);

    const unsigned char* cursor = nullptr;
    LibreSsl3SslSession* decoded = Decode(expected, cursor);
    Require(decoded != nullptr);
    Require(cursor == expected.data() + expected.size());
    Require(decoded->time == 0x00ff000000000000);
    Require(decoded->timeout == DefaultTimeout);
    free_nid_postfix(decoded);
    Require(allocations.empty());
}

void TestFullRoundTrip() {
    LibreSsl3SslSession session = FullSession();
    const Bytes expected = ExpectedFullEncoding();
    Require(Encode(session) == expected);

    const unsigned char* cursor = nullptr;
    LibreSsl3SslSession* decoded = Decode(expected, cursor);
    Require(decoded != nullptr);
    Require(cursor == expected.data() + expected.size());
    Require(decoded->sslVersion == Tls12Version);
    Require(decoded->masterKeyLength == sizeof(session.masterKey));
    Require(std::memcmp(decoded->masterKey, session.masterKey, sizeof(session.masterKey)) == 0);
    Require(decoded->sessionIdLength == sizeof(session.sessionId));
    Require(std::memcmp(decoded->sessionId, session.sessionId, sizeof(session.sessionId)) == 0);
    Require(decoded->sidCtxLength == sizeof(session.sidCtx));
    Require(std::memcmp(decoded->sidCtx, session.sidCtx, sizeof(session.sidCtx)) == 0);
    Require(decoded->verifyResult == SessionVerifyResult);
    Require(decoded->timeout == SessionTimeout);
    Require(decoded->time == SessionTime);
    Require(decoded->references == 1);
    Require(decoded->peerCert == nullptr);
    Require(decoded->peerCertType == 0);
    Require(Encode(*decoded) == expected);
    free_nid_postfix(decoded);
    Require(allocations.empty());
}

void TestTwoCallConvention() {
    LibreSsl3SslSession session = FullSession();
    const Bytes expected = ExpectedFullEncoding();

    const int sizeOnly = i2d_SSL_SESSION_nid_postfix(&session, nullptr);
    Require(sizeOnly == static_cast<int>(expected.size()));

    unsigned char* allocated = nullptr;
    Require(i2d_SSL_SESSION_nid_postfix(&session, &allocated) == sizeOnly);
    Require(allocated != nullptr);
    Require(std::memcmp(allocated, expected.data(), expected.size()) == 0);
    Require(allocations.size() == 1);
    free_nid_postfix(allocated);
    Require(allocations.empty());

    std::vector<unsigned char> manual(expected.size() + 4, 0xcc);
    unsigned char* writePointer = manual.data();
    Require(i2d_SSL_SESSION_nid_postfix(&session, &writePointer) == sizeOnly);
    Require(writePointer == manual.data() + expected.size());
    Require(std::memcmp(manual.data(), expected.data(), expected.size()) == 0);
    Require(manual[expected.size()] == 0xcc);
    Require(allocations.empty());

    std::vector<unsigned char> offsetBytes(expected.size() + 8, 0xcc);
    unsigned char* offsetPointer = offsetBytes.data() + 4;
    Require(i2d_SSL_SESSION_nid_postfix(&session, &offsetPointer) == sizeOnly);
    Require(offsetPointer == offsetBytes.data() + 4 + expected.size());
    Require(std::memcmp(offsetBytes.data() + 4, expected.data(), expected.size()) == 0);
    Require(offsetBytes[0] == 0xcc);
    Require(offsetBytes[3] == 0xcc);
    Require(offsetBytes[4 + expected.size()] == 0xcc);
    Require(allocations.empty());
}

void TestDecodeIntoExisting() {
    const Bytes der = ExpectedFullEncoding();
    LibreSsl3SslSession existing{};
    existing.references = 7;
    existing.peerCertType = 4;
    existing.peerCert = reinterpret_cast<void*>(0x1234);
    LibreSsl3SslSession* slot = &existing;
    const unsigned char* cursor = der.data();
    LibreSsl3SslSession* decoded =
        d2i_SSL_SESSION_nid_postfix(&slot, &cursor, static_cast<std::int64_t>(der.size()));
    Require(decoded == &existing);
    Require(slot == &existing);
    Require(cursor == der.data() + der.size());
    Require(existing.references == 7);
    Require(existing.peerCertType == 4);
    Require(existing.peerCert == nullptr);
    Require(existing.sslVersion == Tls12Version);
    Require(existing.masterKeyLength == 48);
    Require(existing.sessionIdLength == 32);
    Require(existing.sidCtxLength == 32);
    Require(existing.verifyResult == SessionVerifyResult);
    Require(existing.timeout == SessionTimeout);
    Require(existing.time == SessionTime);
    Require(allocations.empty());

    LibreSsl3SslSession* empty = nullptr;
    cursor = der.data();
    decoded = d2i_SSL_SESSION_nid_postfix(&empty, &cursor, static_cast<std::int64_t>(der.size()));
    Require(decoded != nullptr);
    Require(empty == decoded);
    Require(decoded->references == 1);
    free_nid_postfix(decoded);
    Require(allocations.empty());
}

struct CachedEncoding {
    const unsigned char* bytes;
    std::int64_t length;
    std::int32_t modified;
    std::int32_t padding;
};

static_assert(sizeof(CachedEncoding) == sizeof(LibreSsl3CertificateInfo::encoding));

void TestPeerCertificate() {
    const Bytes certificateContent{0x02, 0x01, 0x05};
    const Bytes certificate = Sequence(certificateContent);
    LibreSsl3CertificateInfo info{};
    const CachedEncoding encoding{certificateContent.data(), static_cast<std::int64_t>(certificateContent.size()), 0, 0};
    std::memcpy(info.encoding, &encoding, sizeof(encoding));
    LibreSsl3Certificate stored{};
    stored.certInfo = &info;

    LibreSsl3SslSession session = MinimalSession();
    session.peerCert = &stored;
    const Bytes expected = Sequence(Join({
        Integer(1),
        Integer(Tls12Version),
        Tlv(0x04, Bytes{0x00, 0x00}),
        Tlv(0x04, Bytes{}),
        Tlv(0x04, Bytes{}),
        Tlv(0xa3, certificate),
        Tlv(0xa4, Tlv(0x04, Bytes{})),
    }));
    Require(Encode(session) == expected);

    DrainErrors();
    const unsigned char* cursor = expected.data();
    RequireNotImplemented([&] {
        d2i_SSL_SESSION_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(expected.size()));
    });
    Require(cursor == expected.data());
    Require(allocations.empty());
    DrainErrors();
}

void TestOptionalDefaults() {
    const Bytes minimal = MinimalEncoding();
    const auto before = static_cast<std::int64_t>(std::time(nullptr));
    const unsigned char* cursor = nullptr;
    LibreSsl3SslSession* session = Decode(minimal, cursor);
    Require(session != nullptr);
    Require(cursor == minimal.data() + minimal.size());
    Require(session->sslVersion == Tls12Version);
    Require(session->masterKeyLength == 0);
    Require(session->sessionIdLength == 0);
    Require(session->sidCtxLength == 0);
    Require(session->verifyResult == 0);
    Require(session->timeout == DefaultTimeout);
    Require(session->time >= before);
    Require(session->time <= static_cast<std::int64_t>(std::time(nullptr)));
    Require(session->references == 1);
    Require(session->peerCertType == 0);
    free_nid_postfix(session);
    Require(allocations.empty());

    const Bytes zeroFields = Sequence(Join({
        Integer(1),
        Integer(Tls12Version),
        Tlv(0x04, Bytes{0x00, 0x00}),
        Tlv(0x04, Bytes{}),
        Tlv(0x04, Bytes{}),
        Tlv(0xa1, Integer(0)),
        Tlv(0xa2, Integer(0)),
        Tlv(0xa4, Tlv(0x04, Bytes{})),
        Tlv(0xa5, Integer(0)),
    }));
    session = Decode(zeroFields, cursor);
    Require(session != nullptr);
    Require(session->timeout == DefaultTimeout);
    Require(session->time >= before);
    Require(session->verifyResult == 0);
    free_nid_postfix(session);
    Require(allocations.empty());

    const Bytes zeroLifetime = Sequence(Join({
        MinimalContent(),
        Tlv(0xa9, Integer(0)),
    }));
    session = Decode(zeroLifetime, cursor);
    Require(session != nullptr);
    Require(cursor == zeroLifetime.data() + zeroLifetime.size());
    free_nid_postfix(session);
    Require(allocations.empty());
}

void TestTrailingData() {
    const Bytes der = Sequence(Join({
        MinimalContent(),
        Bytes{0x05, 0x00},
    }));
    const unsigned char* cursor = nullptr;
    LibreSsl3SslSession* session = Decode(der, cursor);
    Require(session != nullptr);
    Require(cursor == der.data() + der.size());
    Require(session->sslVersion == Tls12Version);
    free_nid_postfix(session);
    Require(allocations.empty());

    Bytes padded = der;
    padded.insert(padded.end(), {0xcc, 0xcc, 0xcc});
    cursor = nullptr;
    session = Decode(padded, cursor);
    Require(session != nullptr);
    Require(cursor == padded.data() + der.size());
    free_nid_postfix(session);
    Require(allocations.empty());
}

void TestTruncatedInput() {
    const Bytes der = ExpectedFullEncoding();
    for (std::size_t length = 0; length < der.size(); ++length) {
        DrainErrors();
        const unsigned char* cursor = der.data();
        LibreSsl3SslSession* session =
            d2i_SSL_SESSION_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(length));
        Require(session == nullptr);
        Require(cursor == der.data());
        Require(allocations.empty());
    }
    DrainErrors();
}

void TestMalformed() {
    RequireRejected(Tlv(0x31, MinimalContent()), 168);
    RequireRejected(Sequence(Join({
        Tlv(0x04, Bytes{0x01}),
        Integer(Tls12Version),
        Tlv(0x04, Bytes{0x00, 0x00}),
        Tlv(0x04, Bytes{}),
        Tlv(0x04, Bytes{}),
        Tlv(0xa4, Tlv(0x04, Bytes{})),
    })), 168);

    Bytes overlongOuter = MinimalEncoding();
    overlongOuter[1] = 0x14;
    RequireRejected(overlongOuter, 155);

    RequireRejected(Bytes{0x30, 0x80, 0x00, 0x00}, 102);
    RequireRejected(Bytes{0x30, 0x84, 0x00, 0x00, 0x00, 0x00}, 102);
    RequireRejected(Bytes{0x30, 0x82, 0x00, 0x80}, 102);
    RequireRejected(Bytes{0x30}, 102);
    RequireRejected(Bytes{0x1f, 0x00}, 102);

    RequireRejected(Sequence(Join({
        Integer(2),
        Integer(Tls12Version),
        Tlv(0x04, Bytes{0x00, 0x00}),
        Tlv(0x04, Bytes{}),
        Tlv(0x04, Bytes{}),
    })), 102);
    RequireRejected(Sequence(Join({
        Tlv(0x02, Bytes{0x00, 0x01}),
        Integer(Tls12Version),
        Tlv(0x04, Bytes{0x00, 0x00}),
        Tlv(0x04, Bytes{}),
        Tlv(0x04, Bytes{}),
    })), 102);
    RequireRejected(Sequence(Join({
        Tlv(0x02, Bytes{0x81}),
        Integer(Tls12Version),
        Tlv(0x04, Bytes{0x00, 0x00}),
        Tlv(0x04, Bytes{}),
        Tlv(0x04, Bytes{}),
    })), 102);
    RequireRejected(Sequence(Join({
        Tlv(0x02, Bytes{}),
        Integer(Tls12Version),
        Tlv(0x04, Bytes{0x00, 0x00}),
        Tlv(0x04, Bytes{}),
        Tlv(0x04, Bytes{}),
    })), 102);
    RequireRejected(Sequence(Join({
        Integer(1),
        Integer(0x80000000u),
        Tlv(0x04, Bytes{0x00, 0x00}),
        Tlv(0x04, Bytes{}),
        Tlv(0x04, Bytes{}),
    })), 155);

    RequireRejected(Sequence(Join({
        Integer(1),
        Integer(Tls12Version),
        Tlv(0x04, Bytes{0x00}),
        Tlv(0x04, Bytes{}),
        Tlv(0x04, Bytes{}),
    })), 102);
    RequireRejected(Sequence(Join({
        Integer(1),
        Integer(Tls12Version),
        Tlv(0x04, Bytes{0x00, 0x00, 0x00}),
        Tlv(0x04, Bytes{}),
        Tlv(0x04, Bytes{}),
    })), 102);
    RequireRejected(Sequence(Join({
        Integer(1),
        Integer(Tls12Version),
        Tlv(0x04, Bytes{0x00, 0x00}),
        Tlv(0x04, Range(0x00, 33)),
        Tlv(0x04, Bytes{}),
    })), 155);
    RequireRejected(Sequence(Join({
        Integer(1),
        Integer(Tls12Version),
        Tlv(0x04, Bytes{0x00, 0x00}),
        Tlv(0x04, Bytes{}),
        Tlv(0x04, Range(0x00, 49)),
    })), 155);
    RequireRejected(Sequence(Join({
        Integer(1),
        Integer(Tls12Version),
        Tlv(0x04, Bytes{0x00, 0x00}),
        Tlv(0x04, Bytes{}),
        Tlv(0x04, Bytes{}),
        Tlv(0xa4, Tlv(0x04, Range(0x00, 33))),
    })), 155);

    RequireRejected(Sequence(Join({
        Integer(1),
        Integer(Tls12Version),
        Tlv(0x04, Bytes{0x00, 0x00}),
        Tlv(0x04, Bytes{}),
        Tlv(0x04, Bytes{}),
        Tlv(0xa1, Tlv(0x04, Bytes{0x01})),
    })), 168);
    RequireRejected(Sequence(Join({
        Integer(1),
        Integer(Tls12Version),
        Tlv(0x04, Bytes{0x00, 0x00}),
        Tlv(0x04, Bytes{}),
        Tlv(0x04, Bytes{}),
        Tlv(0xa4, Tlv(0x02, Bytes{0x01})),
    })), 168);
    RequireRejected(Sequence(Join({
        Integer(1),
        Integer(Tls12Version),
        Tlv(0x04, Bytes{0x00, 0x00}),
        Tlv(0x04, Bytes{}),
        Tlv(0x04, Bytes{}),
        Tlv(0xa4, Join({Tlv(0x04, Bytes{}), Bytes{0x05, 0x00}})),
    })), 155);
    RequireRejected(Sequence(Join({
        Integer(1),
        Integer(Tls12Version),
        Tlv(0x04, Bytes{0x00, 0x00}),
        Tlv(0x04, Bytes{}),
        Tlv(0x04, Bytes{}),
        Tlv(0xa1, Tlv(0x02, Bytes{0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00})),
    })), 155);
}

void TestUnrepresentableFields() {
    const Bytes hostname = Sequence(Join({
        MinimalContent(),
        Tlv(0xa6, Tlv(0x04, Bytes{'a', 'b'})),
    }));
    const Bytes lifetime = Sequence(Join({MinimalContent(), Tlv(0xa9, Integer(5))}));
    const Bytes ticket = Sequence(Join({MinimalContent(), Tlv(0xaa, Tlv(0x04, Bytes{0x01}))}));
    for (const Bytes& der : {hostname, lifetime, ticket}) {
        DrainErrors();
        const unsigned char* cursor = der.data();
        RequireNotImplemented([&] {
            d2i_SSL_SESSION_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(der.size()));
        });
        Require(cursor == der.data());
        Require(allocations.empty());
        DrainErrors();
    }
}

void RequireEncodeRejected(LibreSsl3SslSession& session, std::source_location location = std::source_location::current()) {
    DrainErrors();
    Require(i2d_SSL_SESSION_nid_postfix(&session, nullptr) == -1, location);
    unsigned char* untouched = nullptr;
    Require(i2d_SSL_SESSION_nid_postfix(&session, &untouched) == -1, location);
    Require(untouched == nullptr, location);
    Require(allocations.empty(), location);
    DrainErrors();
}

void TestArgumentErrors() {
    DrainErrors();
    Require(i2d_SSL_SESSION_nid_postfix(nullptr, nullptr) == 0);
    unsigned char* untouched = reinterpret_cast<unsigned char*>(0x10);
    Require(i2d_SSL_SESSION_nid_postfix(nullptr, &untouched) == 0);
    Require(untouched == reinterpret_cast<unsigned char*>(0x10));
    Require(ERR_get_error_nid_postfix() == 0);

    LibreSsl3SslSession session = MinimalSession();
    session.sslVersion = -1;
    RequireEncodeRejected(session);
    session = MinimalSession();
    session.time = -1;
    RequireEncodeRejected(session);
    session = MinimalSession();
    session.timeout = -1;
    RequireEncodeRejected(session);
    session = MinimalSession();
    session.verifyResult = -1;
    RequireEncodeRejected(session);
    session = MinimalSession();
    session.sessionIdLength = 33;
    RequireEncodeRejected(session);
    session = MinimalSession();
    session.masterKeyLength = 49;
    RequireEncodeRejected(session);
    session = MinimalSession();
    session.sidCtxLength = 33;
    RequireEncodeRejected(session);
    session = MinimalSession();
    session.sslVersion = 0x7fffffff;
    Require(i2d_SSL_SESSION_nid_postfix(&session, nullptr) > 0);

    const Bytes der = MinimalEncoding();
    DrainErrors();
    const unsigned char* cursor = der.data();
    Require(d2i_SSL_SESSION_nid_postfix(nullptr, nullptr, 4) == nullptr);
    RequireQueue({102});
    Require(d2i_SSL_SESSION_nid_postfix(nullptr, &cursor, -1) == nullptr);
    RequireQueue({102});
    Require(cursor == der.data());
    const unsigned char* nullInput = nullptr;
    Require(d2i_SSL_SESSION_nid_postfix(nullptr, &nullInput, 4) == nullptr);
    RequireQueue({102});
    Require(allocations.empty());
}

void TestAllocationFailures() {
    LibreSsl3SslSession session = FullSession();
    allocationsUntilFailure = 0;
    Require(i2d_SSL_SESSION_nid_postfix(&session, nullptr) > 0);
    unsigned char* allocated = nullptr;
    Require(i2d_SSL_SESSION_nid_postfix(&session, &allocated) == -1);
    Require(allocated == nullptr);
    Require(allocations.empty());

    const Bytes der = ExpectedFullEncoding();
    DrainErrors();
    allocationsUntilFailure = 0;
    const unsigned char* cursor = der.data();
    Require(d2i_SSL_SESSION_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(der.size())) == nullptr);
    Require(cursor == der.data());
    Require(allocations.empty());
    DrainErrors();
}

void TestExactBytesNegativeControl() {
    LibreSsl3SslSession session = MinimalSession();
    const Bytes expected = MinimalEncoding();
    const Bytes encoded = Encode(session);
    Require(encoded == expected);
    for (std::size_t index = 0; index < expected.size(); ++index) {
        Bytes corrupted = expected;
        corrupted[index] ^= 0x01;
        Require(corrupted != expected);
        Require(encoded != corrupted);
    }
}

}

int main() {
    std::array<void*, 7> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch),
        reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(Callocate_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    DrainErrors();
    TestMinimalExactBytes();
    TestIntegerBoundaries();
    TestFullRoundTrip();
    TestTwoCallConvention();
    TestDecodeIntoExisting();
    TestPeerCertificate();
    TestOptionalDefaults();
    TestTrailingData();
    TestTruncatedInput();
    TestMalformed();
    TestUnrepresentableFields();
    TestArgumentErrors();
    TestAllocationFailures();
    TestExactBytesNegativeControl();
    Require(allocations.empty());
    std::puts("libSceLibreSsl3 SSL_SESSION DER tests passed");
}
