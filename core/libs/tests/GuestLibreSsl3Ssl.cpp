#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <thread>
#include <vector>

namespace {

constexpr std::uint64_t TooLarge = 0x0DFFF0DF;
constexpr std::uint64_t TooSmall = 0x0DFFF0E0;
constexpr std::uint64_t WrongIntegerType = 0x0DFFF0E1;

void Require(bool condition, const char* message, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "line %u: %s\n", location.line(), message);
    std::abort();
}

template<class TObject>
struct Guarded {
    std::array<std::uint64_t, 2> before;
    TObject object;
    std::array<std::uint64_t, 2> after;
};

template<class TObject>
std::array<unsigned char, sizeof(Guarded<TObject>)> Bytes(const Guarded<TObject>& guarded) {
    std::array<unsigned char, sizeof(Guarded<TObject>)> bytes;
    std::memcpy(bytes.data(), &guarded, sizeof(guarded));
    return bytes;
}

template<class TObject>
bool Same(const Guarded<TObject>& guarded, const std::array<unsigned char, sizeof(Guarded<TObject>)>& expected) {
    return std::memcmp(&guarded, expected.data(), sizeof(guarded)) == 0;
}

template<class TObject, class TRetain>
void CheckRetain(TRetain retain, std::size_t referenceOffset) {
    const std::size_t offset = offsetof(Guarded<TObject>, object) + referenceOffset;
    Guarded<TObject> guarded;
    for (std::int32_t initial : {-2, 0, 1, 37, std::numeric_limits<std::int32_t>::max() - 1}) {
        std::memset(&guarded, 0xA5, sizeof(guarded));
        guarded.object.references = initial;
        auto expected = Bytes(guarded);
        const std::int32_t next = initial + 1;
        std::memcpy(expected.data() + offset, &next, sizeof(next));
        Require(retain(&guarded.object) == (next > 1), "retain return value");
        Require(Same(guarded, expected), "retain changed bytes outside the reference count");
    }
    std::memset(&guarded, 0x5A, sizeof(guarded));
    guarded.object.references = 1;
    auto expected = Bytes(guarded);
    std::atomic<bool> start = false;
    std::array<std::thread, 4> threads;
    for (auto& thread : threads) {
        thread = std::thread([&] {
            while (!start.load())
                std::this_thread::yield();
            for (int index = 0; index < 1000; ++index)
                Require(retain(&guarded.object) == 1, "concurrent retain");
        });
    }
    start = true;
    for (auto& thread : threads)
        thread.join();
    const std::int32_t finalCount = 4001;
    std::memcpy(expected.data() + offset, &finalCount, sizeof(finalCount));
    Require(Same(guarded, expected), "concurrent count or adjacent bytes changed");
}

struct Versions {
    std::uint16_t minTls;
    std::uint16_t maxTls;
    std::uint16_t minProto;
    std::uint16_t maxProto;
};

template<class TSet>
void CheckBound(TSet set, const LibreSsl3SslMethod& method, Versions initial, std::uint16_t version, int result,
                Versions final, const char* message, std::source_location location = std::source_location::current()) {
    Guarded<LibreSsl3SslCtx> guarded;
    std::memset(&guarded, 0xA5, sizeof(guarded));
    guarded.object.method = &method;
    guarded.object.minTlsVersion = initial.minTls;
    guarded.object.maxTlsVersion = initial.maxTls;
    guarded.object.minProtoVersion = initial.minProto;
    guarded.object.maxProtoVersion = initial.maxProto;
    auto expected = Bytes(guarded);
    const std::array<std::uint16_t, 4> fields = {final.minTls, final.maxTls, final.minProto, final.maxProto};
    std::memcpy(expected.data() + offsetof(Guarded<LibreSsl3SslCtx>, object) + 128, fields.data(), sizeof(fields));
    Require(set(&guarded.object, version) == result, message, location);
    Require(Same(guarded, expected), message, location);
}

void CheckVersionBounds() {
    const auto setMin = SSL_CTX_set_min_proto_version_nid_postfix;
    const auto setMax = SSL_CTX_set_max_proto_version_nid_postfix;
    const LibreSsl3SslMethod tls{0, 0, 0x0304, 0x0301, 0x0304};
    const Versions full{0x0301, 0x0304, 0, 0};
    CheckBound(setMin, tls, {0x0302, 0x0304, 0x0302, 0}, 0, 1, full, "TLS minimum 0 resets to the method minimum");
    CheckBound(setMax, tls, {0x0301, 0x0302, 0, 0x0302}, 0, 1, full, "TLS maximum 0 resets to the method maximum");
    CheckBound(setMin, tls, full, 0x0303, 1, {0x0303, 0x0304, 0x0303, 0}, "TLS minimum 1.2");
    CheckBound(setMin, tls, full, 0x0304, 1, {0x0304, 0x0304, 0x0304, 0}, "TLS minimum equal to the maximum");
    CheckBound(setMin, tls, full, 0x0300, 1, {0x0301, 0x0304, 0x0301, 0}, "TLS minimum raised to the method minimum");
    CheckBound(setMin, tls, full, 0x0305, 0, full, "TLS minimum above the maximum");
    CheckBound(setMax, tls, full, 0x0303, 1, {0x0301, 0x0303, 0, 0x0303}, "TLS maximum 1.2");
    CheckBound(setMax, tls, full, 0x0301, 1, {0x0301, 0x0301, 0, 0x0301}, "TLS maximum equal to the minimum");
    CheckBound(setMax, tls, full, 0x0305, 1, {0x0301, 0x0304, 0, 0x0304}, "TLS maximum lowered to the method maximum");
    CheckBound(setMax, tls, full, 0x0300, 0, full, "TLS maximum below the minimum");
    for (std::uint16_t version : {0xFEFF, 0xFEFD}) {
        CheckBound(setMin, tls, full, version, 0, full, "DTLS minimum on a TLS method");
        CheckBound(setMax, tls, full, version, 1, {0x0301, 0x0304, 0, 0x0304}, "DTLS maximum on a TLS method");
    }

    const LibreSsl3SslMethod narrow{0, 0, 0x0303, 0x0301, 0x0303};
    CheckBound(setMin, narrow, {0x0301, 0x0304, 0, 0}, 0x0302, 1, {0x0302, 0x0304, 0x0302, 0},
               "clamping the stored maximum leaves it unchanged");
    const LibreSsl3SslMethod empty{0, 0, 0, 0, 0};
    CheckBound(setMax, empty, {0, 0, 0x0301, 0x0301}, 0x0303, 1, {0, 0, 0x0301, 0}, "a TLS bound of 0 is stored");

    const LibreSsl3SslMethod inverted{0, 0, 0x0303, 0x0303, 0x0302};
    CheckBound(setMin, inverted, {0x0302, 0x0303, 0, 0}, 0x0302, 0, {0x0302, 0x0303, 0, 0}, "inverted method range");
    CheckBound(setMax, inverted, {0x0302, 0x0303, 0, 0}, 0x0303, 0, {0x0302, 0x0303, 0, 0}, "inverted method range");
    CheckBound(setMin, tls, {0x0301, 0x0302, 0, 0x0304}, 0x0303, 0, {0x0301, 0x0302, 0, 0x0304},
               "minimum above the stored TLS maximum");
    CheckBound(setMax, tls, {0x0303, 0x0304, 0x0301, 0}, 0x0302, 0, {0x0303, 0x0304, 0x0301, 0},
               "maximum below the stored TLS minimum");
    CheckBound(setMin, narrow, {0x0301, 0x0304, 0, 0}, 0x0304, 0, {0x0301, 0x0304, 0, 0}, "range above the method");
    const LibreSsl3SslMethod upper{0, 0, 0x0304, 0x0302, 0x0304};
    CheckBound(setMax, upper, {0x0301, 0x0304, 0, 0}, 0x0301, 0, {0x0301, 0x0304, 0, 0}, "range below the method");

    const LibreSsl3SslMethod dtls{1, 0, 0xFEFD, 0x0302, 0x0303};
    const Versions dtlsFull{0x0302, 0x0303, 0, 0};
    CheckBound(setMin, dtls, dtlsFull, 0xFEFF, 1, {0x0302, 0x0303, 0xFEFF, 0}, "DTLS minimum 1.0");
    CheckBound(setMin, dtls, dtlsFull, 0xFEFD, 1, {0x0303, 0x0303, 0xFEFD, 0}, "DTLS minimum 1.2");
    CheckBound(setMax, dtls, dtlsFull, 0xFEFD, 1, {0x0302, 0x0303, 0, 0xFEFD}, "DTLS maximum 1.2");
    CheckBound(setMax, dtls, dtlsFull, 0xFEFF, 1, {0x0302, 0x0302, 0, 0xFEFF}, "DTLS maximum 1.0");
    CheckBound(setMin, dtls, {0x0303, 0x0303, 0xFEFD, 0}, 0, 1, dtlsFull, "DTLS minimum 0");
    CheckBound(setMax, dtls, {0x0302, 0x0302, 0, 0xFEFF}, 0, 1, dtlsFull, "DTLS maximum 0");
    for (std::uint16_t version : {0x0302, 0x0303, 0xFEFE, 0xFEFC, 0x0100}) {
        CheckBound(setMin, dtls, dtlsFull, version, 0, dtlsFull, "DTLS minimum outside the DTLS versions");
        CheckBound(setMax, dtls, dtlsFull, version, 0, dtlsFull, "DTLS maximum outside the DTLS versions");
    }
    CheckBound(setMin, dtls, {0x0302, 0x0302, 0, 0}, 0xFEFD, 0, {0x0302, 0x0302, 0, 0},
               "DTLS minimum above the maximum");
    const LibreSsl3SslMethod anyDtls{-1, 0, 0xFEFD, 0x0302, 0x0303};
    CheckBound(setMin, anyDtls, dtlsFull, 0xFEFF, 1, {0x0302, 0x0303, 0xFEFF, 0}, "nonzero dtls flag");
    CheckBound(setMin, anyDtls, dtlsFull, 0x0302, 0, dtlsFull, "nonzero dtls flag rejects TLS versions");
    const LibreSsl3SslMethod dtlsTls10{1, 0, 0, 0x0301, 0x0301};
    CheckBound(setMax, dtlsTls10, {0x0301, 0x0301, 0, 0}, 0xFEFF, 0, {0x0301, 0x0301, 0, 0},
               "clamped maximum without a DTLS form");
    CheckBound(setMin, dtlsTls10, {0x0302, 0x0301, 0xFEFF, 0}, 0, 1, {0x0301, 0x0301, 0, 0},
               "DTLS minimum 0 is not converted");
    const LibreSsl3SslMethod dtlsTls13{1, 0, 0, 0x0304, 0x0304};
    CheckBound(setMin, dtlsTls13, {0x0304, 0x0304, 0, 0}, 0xFEFF, 0, {0x0304, 0x0304, 0, 0},
               "clamped minimum without a DTLS form");
}

void CheckClientRandom() {
    LibreSsl3Ssl3State state;
    std::array<unsigned char, sizeof(state)> stateBytes;
    for (std::size_t index = 0; index < stateBytes.size(); ++index)
        stateBytes[index] = static_cast<unsigned char>(index * 7 + 3);
    std::memcpy(&state, stateBytes.data(), sizeof(state));
    LibreSsl3Ssl ssl;
    std::memset(&ssl, 0x5A, sizeof(ssl));
    ssl.s3 = &state;
    const std::size_t limits[] = {0, 1, 10, 31, 32, 33, 64, std::numeric_limits<std::size_t>::max()};
    for (std::size_t limit : limits) {
        Require(SSL_get_client_random_nid_postfix(nullptr, nullptr, limit) == 32, "null output with a null SSL");
        Require(SSL_get_client_random_nid_postfix(&ssl, nullptr, limit) == 32, "null output");
        std::array<unsigned char, 48> out;
        out.fill(0xEE);
        const auto copied = std::min<std::size_t>(limit, 32);
        Require(SSL_get_client_random_nid_postfix(&ssl, out.data(), limit) == copied, "copied length");
        for (std::size_t index = 0; index < out.size(); ++index)
            Require(out[index] == (index < copied ? stateBytes[40 + index] : 0xEE), "client random bytes or untouched tail");
    }
    Require(std::memcmp(&state, stateBytes.data(), sizeof(state)) == 0, "SSL3 state unchanged");
    ssl.s3 = nullptr;
    std::array<unsigned char, 4> out{};
    Require(SSL_get_client_random_nid_postfix(&ssl, out.data(), 0) == 0, "an empty copy does not read SSL3 state");
}

struct VersionResult {
    std::int64_t value;
    std::uint64_t error;
    std::uint64_t following;
};

VersionResult ReadVersion(Asn1String* version) {
    LibreSsl3X509CertInfo info{version};
    LibreSsl3X509 certificate;
    std::memset(&certificate, 0xA5, sizeof(certificate));
    certificate.certInfo = &info;
    ERR_clear_error_nid_postfix();
    VersionResult result;
    result.value = X509_get_version_nid_postfix(&certificate);
    result.error = ERR_get_error_nid_postfix();
    result.following = ERR_get_error_nid_postfix();
    return result;
}

void CheckVersion(std::int32_t type, std::vector<unsigned char> bytes, std::int64_t value, std::uint64_t error,
                  const char* message, std::source_location location = std::source_location::current()) {
    Asn1String integer{static_cast<std::int32_t>(bytes.size()), type, bytes.data(), 0};
    const auto result = ReadVersion(&integer);
    Require(result.value == value && result.error == error && result.following == 0, message, location);
}

void CheckX509Version() {
    const auto absent = ReadVersion(nullptr);
    Require(absent.value == 0 && absent.error == 0, "absent version");
    unsigned char byte = 2;
    for (unsigned char* data : {&byte, static_cast<unsigned char*>(nullptr)}) {
        Asn1String empty{0, 2, data, 0};
        const auto result = ReadVersion(&empty);
        Require(result.value == 0 && result.error == 0, "empty integer");
    }
    for (std::int32_t type : {2, 4}) {
        Asn1String negativeLength{-1, type, &byte, 0};
        const auto result = ReadVersion(&negativeLength);
        Require(result.value == -1 && result.error == 0, "negative length");
    }
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    const auto minimum = std::numeric_limits<std::int64_t>::min();
    CheckVersion(2, {0x02}, 2, 0, "encoded version 2");
    CheckVersion(2, {0x00, 0x02}, 2, 0, "leading zero");
    CheckVersion(2, {0x01, 0x00}, 256, 0, "big-endian magnitude");
    CheckVersion(2, {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0}, 0x123456789ABCDEF0, 0, "eight bytes");
    CheckVersion(2, {0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, maximum, 0, "largest value");
    CheckVersion(2, {0x00, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, maximum, 0, "nine bytes with a leading zero");
    std::vector<unsigned char> padded(20, 0x00);
    padded.push_back(0x05);
    CheckVersion(2, padded, 5, 0, "many leading zeros");
    CheckVersion(0x102, {0x01}, -1, 0, "negative one");
    CheckVersion(0x102, {0x00}, 0, 0, "negative zero");
    CheckVersion(0x102, {0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, minimum, 0, "smallest value");
    CheckVersion(4, {0x02}, -1, WrongIntegerType, "octet string type");
    CheckVersion(0x100, {0x02}, -1, WrongIntegerType, "negative flag alone");
    CheckVersion(2, {0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, -1, TooLarge, "positive value above the maximum");
    CheckVersion(2, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, -1, TooLarge, "64-bit positive value");
    CheckVersion(2, {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, -1, TooLarge, "nine significant bytes");
    CheckVersion(0x102, {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, -1, TooLarge,
                 "nine significant negative bytes");
    CheckVersion(0x102, {0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}, -1, TooSmall, "negative value below the minimum");
}

} // namespace

int main() {
    CheckRetain<LibreSsl3Ssl>(SSL_up_ref_nid_postfix, 176);
    CheckRetain<LibreSsl3SslSession>(SSL_SESSION_up_ref_nid_postfix, 184);
    CheckVersionBounds();
    CheckClientRandom();
    CheckRetain<LibreSsl3X509>(X509_up_ref_nid_postfix, 28);
    CheckX509Version();
    return 0;
}
