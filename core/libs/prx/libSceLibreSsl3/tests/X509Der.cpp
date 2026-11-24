#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <source_location>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 i2d_X509 check failed at line %u\n", location.line());
    std::abort();
}

template <typename Function>
void RequireThrows(Function function, std::source_location location = std::source_location::current()) {
    try {
        function();
    } catch (const std::exception&) {
        return;
    }
    Require(false, location);
}

using Bytes = std::vector<unsigned char>;

int liveBlocks = 0;
bool failNextAllocation = false;

void* APS5_VABI Allocate(std::size_t bytes) {
    ++liveBlocks;
    return std::malloc(bytes ? bytes : 1);
}

void APS5_VABI Release(void* block) {
    if (block) --liveBlocks;
    std::free(block);
}

void* APS5_VABI AllocateZeroed(std::size_t count, std::size_t bytes) {
    if (failNextAllocation) {
        failNextAllocation = false;
        return nullptr;
    }
    ++liveBlocks;
    return std::calloc(count ? count : 1, bytes ? bytes : 1);
}

void* APS5_VABI Reallocate(void*, std::size_t) { return nullptr; }
void* APS5_VABI Align(std::size_t, std::size_t) { return nullptr; }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { return nullptr; }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { return 12; }

void InitializeHeap() {
    void* const api[10] {
        reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release), reinterpret_cast<void*>(&AllocateZeroed),
        reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
        reinterpret_cast<void*>(&PosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(api);
}

const Bytes Certificate {
    0x30, 0x82, 0x01, 0x81, 0x30, 0x82, 0x01, 0x27, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x14, 0x5a, 0x6b, 0x39,
    0xe9, 0xff, 0xac, 0x27, 0xb7, 0x72, 0x2d, 0x23, 0x83, 0x56, 0x43, 0xd2, 0x69, 0x53, 0xf6, 0x6c, 0x8f, 0x30,
    0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02, 0x30, 0x16, 0x31, 0x14, 0x30, 0x12, 0x06,
    0x03, 0x55, 0x04, 0x03, 0x0c, 0x0b, 0x41, 0x6e, 0x79, 0x50, 0x53, 0x35, 0x20, 0x74, 0x65, 0x73, 0x74, 0x30,
    0x1e, 0x17, 0x0d, 0x32, 0x36, 0x31, 0x31, 0x31, 0x37, 0x32, 0x30, 0x31, 0x30, 0x34, 0x38, 0x5a, 0x17, 0x0d,
    0x33, 0x36, 0x31, 0x31, 0x31, 0x34, 0x32, 0x30, 0x31, 0x30, 0x34, 0x38, 0x5a, 0x30, 0x16, 0x31, 0x14, 0x30,
    0x12, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x0b, 0x41, 0x6e, 0x79, 0x50, 0x53, 0x35, 0x20, 0x74, 0x65, 0x73,
    0x74, 0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a, 0x86,
    0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00, 0x04, 0xf2, 0xf2, 0xc4, 0x71, 0x1b, 0xd7, 0x55, 0xa8,
    0x4a, 0x10, 0x09, 0x8c, 0x18, 0x9d, 0x57, 0xb9, 0xbf, 0x43, 0xaa, 0x1d, 0x59, 0x3e, 0xe5, 0xfc, 0xd0, 0xf8,
    0x54, 0xe1, 0x1a, 0x3d, 0xa5, 0x27, 0x92, 0x7d, 0x48, 0x98, 0x00, 0xb4, 0x5f, 0x60, 0xca, 0xf8, 0xeb, 0x14,
    0xd2, 0xc8, 0xf3, 0x4c, 0x91, 0x59, 0x27, 0x6b, 0x95, 0x62, 0x1d, 0xb8, 0x95, 0xeb, 0xce, 0x02, 0x41, 0x2b,
    0xf2, 0x36, 0xa3, 0x53, 0x30, 0x51, 0x30, 0x1d, 0x06, 0x03, 0x55, 0x1d, 0x0e, 0x04, 0x16, 0x04, 0x14, 0x6d,
    0xcb, 0xbb, 0xee, 0x59, 0xac, 0x14, 0x84, 0x9b, 0xba, 0x73, 0x5a, 0xbc, 0x02, 0x79, 0x72, 0x16, 0x20, 0x4e,
    0x4a, 0x30, 0x1f, 0x06, 0x03, 0x55, 0x1d, 0x23, 0x04, 0x18, 0x30, 0x16, 0x80, 0x14, 0x6d, 0xcb, 0xbb, 0xee,
    0x59, 0xac, 0x14, 0x84, 0x9b, 0xba, 0x73, 0x5a, 0xbc, 0x02, 0x79, 0x72, 0x16, 0x20, 0x4e, 0x4a, 0x30, 0x0f,
    0x06, 0x03, 0x55, 0x1d, 0x13, 0x01, 0x01, 0xff, 0x04, 0x05, 0x30, 0x03, 0x01, 0x01, 0xff, 0x30, 0x0a, 0x06,
    0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02, 0x03, 0x48, 0x00, 0x30, 0x45, 0x02, 0x21, 0x00, 0x83,
    0x08, 0xa4, 0xa1, 0x7a, 0x2e, 0x64, 0xc3, 0x69, 0x09, 0x5e, 0xfa, 0xfd, 0x74, 0x6f, 0x5a, 0xd6, 0x4a, 0xc5,
    0x8a, 0x33, 0xbf, 0x08, 0x50, 0x81, 0xeb, 0xff, 0x82, 0xf6, 0x69, 0x88, 0xe2, 0x02, 0x20, 0x6a, 0x63, 0xf2,
    0xa7, 0xaa, 0x92, 0xfd, 0x61, 0xfd, 0xc9, 0x1c, 0x67, 0xa7, 0xdf, 0xa0, 0xf6, 0xff, 0xcd, 0x14, 0xb5, 0xd5,
    0x98, 0x16, 0xcc, 0x91, 0x43, 0xff, 0xef, 0x04, 0xe0, 0xf2, 0x1f,
};

constexpr std::size_t CertificateInfoOffset = 4;
constexpr std::size_t CertificateInfoLength = 299;
constexpr std::size_t SignatureAlgorithmOffset = 307;
constexpr std::size_t SignatureAlgorithmLength = 8;
constexpr std::size_t SignatureOffset = 318;

const Bytes Sha256WithRsa {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b};

struct Encoding {
    const unsigned char* bytes;
    std::int64_t length;
    std::int32_t modified;
    std::int32_t padding;
};

struct Fixture {
    LibreSsl3CertificateInfo info {};
    LibreSslAsn1Object object {};
    LibreSsl3X509Algor algorithm {};
    LibreSsl3Asn1String signature {};
    LibreSsl3Certificate certificate {};

    Fixture() {
        std::memset(&info, 0xa5, sizeof(info));
        std::memset(&certificate, 0x5a, sizeof(certificate));
        certificate.certInfo = &info;
        certificate.sigAlg = &algorithm;
        SetSignature(&signature);
        SetEncoding(nullptr, 0, 0);
    }

    void SetEncoding(const unsigned char* bytes, std::int64_t length, std::int32_t modified) {
        const Encoding encoding {bytes, length, modified, 0};
        std::memcpy(info.encoding, &encoding, sizeof(encoding));
    }

    void SetSignature(const LibreSsl3Asn1String* value) {
        std::memcpy(certificate.rest, &value, sizeof(value));
    }

    void SetAlgorithm(const Bytes& oid, LibreSsl3Asn1Type* parameter) {
        object = LibreSslAsn1Object {nullptr, nullptr, 0, static_cast<std::int32_t>(oid.size()), oid.data(), 0};
        algorithm = LibreSsl3X509Algor {&object, parameter};
    }

    void SetBits(const Bytes& bits, std::int64_t flags) {
        signature = LibreSsl3Asn1String {static_cast<int>(bits.size()), 3, const_cast<unsigned char*>(bits.data()), flags};
    }
};

Bytes Encode(const LibreSsl3Certificate* certificate) {
    unsigned char* buffer = nullptr;
    const int length = i2d_X509_nid_postfix(certificate, &buffer);
    Require(length > 0 && buffer != nullptr);
    Bytes result(buffer, buffer + length);
    free_nid_postfix(buffer);
    return result;
}

void CheckRealCertificate() {
    const Bytes oid(Certificate.begin() + SignatureAlgorithmOffset, Certificate.begin() + SignatureAlgorithmOffset + SignatureAlgorithmLength);
    const Bytes bits(Certificate.begin() + SignatureOffset, Certificate.end());
    Fixture fixture;
    fixture.SetEncoding(Certificate.data() + CertificateInfoOffset, CertificateInfoLength, 0);
    fixture.SetAlgorithm(oid, nullptr);
    fixture.SetBits(bits, 0);
    const int size = static_cast<int>(Certificate.size());

    Require(i2d_X509_nid_postfix(&fixture.certificate, nullptr) == size);
    Require(Encode(&fixture.certificate) == Certificate);

    Bytes buffer(Certificate.size() + 16, 0xcc);
    unsigned char* cursor = buffer.data();
    Require(i2d_X509_nid_postfix(&fixture.certificate, &cursor) == size);
    Require(cursor == buffer.data() + size);
    Require(Bytes(buffer.begin(), buffer.begin() + size) == Certificate);
    for (std::size_t index = Certificate.size(); index < buffer.size(); ++index) Require(buffer[index] == 0xcc);
}

void CheckNullParameterAndTrailingZeroBits() {
    const Bytes info {0x30, 0x00};
    const Bytes bits {0xa0, 0x00};
    LibreSsl3Asn1Type null {5, nullptr};
    Fixture fixture;
    fixture.SetEncoding(info.data(), static_cast<std::int64_t>(info.size()), 0);
    fixture.SetAlgorithm(Sha256WithRsa, &null);
    fixture.SetBits(bits, 0);
    const Bytes expected {0x30, 0x16, 0x30, 0x00, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b,
                          0x05, 0x00, 0x03, 0x03, 0x05, 0xa0, 0x00};
    Require(Encode(&fixture.certificate) == expected);
}

void CheckBitStrings() {
    Fixture fixture;
    fixture.certificate.certInfo = nullptr;
    fixture.certificate.sigAlg = nullptr;

    const Bytes full {0xff};
    fixture.SetBits(full, 0x08 | 3);
    Require(Encode(&fixture.certificate) == Bytes {0x30, 0x04, 0x03, 0x02, 0x03, 0xf8});

    const Bytes odd {0x12, 0x34};
    fixture.SetBits(odd, 0);
    Require(Encode(&fixture.certificate) == Bytes {0x30, 0x05, 0x03, 0x03, 0x02, 0x12, 0x34});

    fixture.SetBits({}, 0);
    Require(Encode(&fixture.certificate) == Bytes {0x30, 0x03, 0x03, 0x01, 0x00});

    const Bytes zeros {0x00, 0x00};
    fixture.SetBits(zeros, 0);
    Require(Encode(&fixture.certificate) == Bytes {0x30, 0x05, 0x03, 0x03, 0x00, 0x00, 0x00});

    fixture.signature.length = -1;
    RequireThrows([&] { i2d_X509_nid_postfix(&fixture.certificate, nullptr); });
}

void CheckParameters() {
    const Bytes info {0x30, 0x00};
    Fixture fixture;
    fixture.SetEncoding(info.data(), static_cast<std::int64_t>(info.size()), 0);
    fixture.SetSignature(nullptr);

    LibreSsl3Asn1Type parameter {};
    fixture.SetAlgorithm(Sha256WithRsa, &parameter);

    parameter = LibreSsl3Asn1Type {1, reinterpret_cast<void*>(static_cast<std::uintptr_t>(0xffffffffu))};
    Require(Encode(&fixture.certificate) == Bytes {0x30, 0x0f, 0x30, 0x00, 0x30, 0x0b, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b});

    parameter = LibreSsl3Asn1Type {1, reinterpret_cast<void*>(std::uintptr_t {0xff})};
    Require(Encode(&fixture.certificate) == Bytes {0x30, 0x12, 0x30, 0x00, 0x30, 0x0e, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b,
                                                   0x01, 0x01, 0xff});

    unsigned char raw[] {0x30, 0x03, 0x02, 0x01, 0x05};
    LibreSsl3Asn1String sequence {5, 16, raw, 0};
    parameter = LibreSsl3Asn1Type {16, &sequence};
    Require(Encode(&fixture.certificate) == Bytes {0x30, 0x14, 0x30, 0x00, 0x30, 0x10, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b,
                                                   0x30, 0x03, 0x02, 0x01, 0x05});

    unsigned char text[] {'h', 'i'};
    LibreSsl3Asn1String octets {2, 4, text, 0};
    parameter = LibreSsl3Asn1Type {4, &octets};
    Require(Encode(&fixture.certificate) == Bytes {0x30, 0x13, 0x30, 0x00, 0x30, 0x0f, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b,
                                                   0x04, 0x02, 'h', 'i'});

    const Bytes curve {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07};
    LibreSslAsn1Object curveObject {nullptr, nullptr, 0, static_cast<std::int32_t>(curve.size()), curve.data(), 0};
    parameter = LibreSsl3Asn1Type {6, &curveObject};
    Require(Encode(&fixture.certificate) == Bytes {0x30, 0x19, 0x30, 0x00, 0x30, 0x15, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b,
                                                   0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07});

    std::int32_t integerContent = 5;
    parameter = LibreSsl3Asn1Type {2, &integerContent};
    RequireThrows([&] { i2d_X509_nid_postfix(&fixture.certificate, nullptr); });

    parameter = LibreSsl3Asn1Type {6, nullptr};
    RequireThrows([&] { i2d_X509_nid_postfix(&fixture.certificate, nullptr); });
}

void CheckMissingFields() {
    Fixture fixture;
    fixture.certificate.certInfo = nullptr;
    fixture.certificate.sigAlg = nullptr;
    fixture.SetSignature(nullptr);
    Require(Encode(&fixture.certificate) == Bytes {0x30, 0x00});

    LibreSsl3X509Algor noObject {nullptr, nullptr};
    fixture.certificate.sigAlg = &noObject;
    Require(Encode(&fixture.certificate) == Bytes {0x30, 0x02, 0x30, 0x00});
}

void CheckCachedEncoding() {
    const Bytes info {0x30, 0x03, 0x02, 0x01, 0x01};
    Fixture fixture;
    fixture.certificate.sigAlg = nullptr;
    fixture.SetSignature(nullptr);
    fixture.SetEncoding(info.data(), static_cast<std::int64_t>(info.size()), 0);
    Require(Encode(&fixture.certificate) == Bytes {0x30, 0x05, 0x30, 0x03, 0x02, 0x01, 0x01});

    fixture.SetEncoding(nullptr, 0, 0);
    Require(Encode(&fixture.certificate) == Bytes {0x30, 0x00});

    fixture.SetEncoding(info.data(), static_cast<std::int64_t>(info.size()), 1);
    RequireThrows([&] { i2d_X509_nid_postfix(&fixture.certificate, nullptr); });

    fixture.SetEncoding(info.data(), -1, 0);
    RequireThrows([&] { i2d_X509_nid_postfix(&fixture.certificate, nullptr); });

    fixture.SetEncoding(nullptr, 4, 0);
    RequireThrows([&] { i2d_X509_nid_postfix(&fixture.certificate, nullptr); });
}

void CheckNullCertificate() {
    Require(i2d_X509_nid_postfix(nullptr, nullptr) == 0);
    unsigned char* buffer = nullptr;
    Require(i2d_X509_nid_postfix(nullptr, &buffer) == 0);
    Require(buffer == nullptr);
    unsigned char storage[4] {0x11, 0x22, 0x33, 0x44};
    unsigned char* cursor = storage;
    Require(i2d_X509_nid_postfix(nullptr, &cursor) == 0);
    Require(cursor == storage && storage[0] == 0x11);
}

void CheckAllocationFailure() {
    const Bytes info {0x30, 0x00};
    Fixture fixture;
    fixture.SetEncoding(info.data(), static_cast<std::int64_t>(info.size()), 0);
    unsigned char* buffer = nullptr;
    failNextAllocation = true;
    Require(i2d_X509_nid_postfix(&fixture.certificate, &buffer) == -1);
    Require(buffer == nullptr);
}

void CheckUnreadableGuestPointers() {
    errno = EINVAL;
    RequireThrows([] { i2d_X509_nid_postfix(reinterpret_cast<const LibreSsl3Certificate*>(std::uintptr_t {0x10}), nullptr); });
    Require(errno == EINVAL);

    Fixture fixture;
    fixture.certificate.certInfo = nullptr;
    fixture.certificate.sigAlg = reinterpret_cast<const LibreSsl3X509Algor*>(std::uintptr_t {0x10});
    errno = EINVAL;
    RequireThrows([&] { i2d_X509_nid_postfix(&fixture.certificate, nullptr); });
    Require(errno == EINVAL);
}

} // namespace

int main() {
    InitializeHeap();
    CheckRealCertificate();
    CheckNullParameterAndTrailingZeroBits();
    CheckBitStrings();
    CheckParameters();
    CheckMissingFields();
    CheckCachedEncoding();
    CheckNullCertificate();
    CheckAllocationFailure();
    CheckUnreadableGuestPointers();
    Require(liveBlocks == 0);
    return 0;
}
