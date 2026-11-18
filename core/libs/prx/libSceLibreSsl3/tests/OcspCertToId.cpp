#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
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
    std::fprintf(stderr, "LibreSSL3 OCSP_cert_to_id check failed at line %u\n", location.line());
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
int zeroedAllocations = 0;
int failAllocation = 0;

void* APS5_VABI Allocate(std::size_t bytes) {
    ++liveBlocks;
    return std::malloc(bytes ? bytes : 1);
}

void APS5_VABI Release(void* block) {
    if (block) --liveBlocks;
    std::free(block);
}

void* APS5_VABI AllocateZeroed(std::size_t count, std::size_t bytes) {
    if (++zeroedAllocations == failAllocation) return nullptr;
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

const Bytes IssuerName {
    0x30, 0x2a, 0x31, 0x0f, 0x30, 0x0d, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x06, 0x41, 0x6e, 0x79,
    0x50, 0x53, 0x35, 0x31, 0x17, 0x30, 0x15, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x0e, 0x41, 0x6e,
    0x79, 0x50, 0x53, 0x35, 0x20, 0x74, 0x65, 0x73, 0x74, 0x20, 0x43, 0x41,
};
const Bytes LeafName {
    0x30, 0x16, 0x31, 0x14, 0x30, 0x12, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x0b, 0x41, 0x6e, 0x79,
    0x50, 0x53, 0x35, 0x20, 0x6c, 0x65, 0x61, 0x66,
};
const Bytes IssuerKey {
    0x04, 0x32, 0xe1, 0x10, 0x12, 0x01, 0x2e, 0xd4, 0xf0, 0x03, 0x23, 0x21, 0x59, 0xe3, 0x43, 0x94,
    0xb5, 0x03, 0x41, 0x5e, 0x72, 0x22, 0xcb, 0xb0, 0xe3, 0x3d, 0x37, 0x43, 0x56, 0x1e, 0xea, 0x29,
    0xc2, 0x29, 0xb0, 0x7d, 0x13, 0x0f, 0x25, 0x58, 0x8e, 0xa1, 0xcd, 0xbc, 0x3e, 0x40, 0x95, 0xd3,
    0x7c, 0x21, 0xa9, 0xde, 0xea, 0xfa, 0x18, 0xdb, 0xeb, 0xfa, 0x38, 0xad, 0xc0, 0x46, 0xed, 0x4c,
    0x87,
};
const Bytes Serial {
    0xc3, 0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0x07, 0x18, 0x29, 0x3a, 0x4b, 0x5c, 0x6d, 0x7e, 0x8f,
};
const Bytes Sha1NameHash {
    0xf2, 0x41, 0x75, 0x59, 0xa4, 0x63, 0x06, 0xb3, 0xd6, 0x6d, 0x1b, 0xb0, 0xb5, 0x0b, 0xab, 0x96,
    0x1d, 0x75, 0xe2, 0xd3,
};
const Bytes Sha1KeyHash {
    0x3c, 0x92, 0x27, 0x12, 0xca, 0x9c, 0xd1, 0x05, 0x59, 0x4f, 0x63, 0x95, 0x6c, 0xa3, 0x4a, 0xad,
    0xea, 0xd4, 0xd4, 0x48,
};
const Bytes Sha256NameHash {
    0xbc, 0x9a, 0x0d, 0xe9, 0x59, 0x9b, 0xfc, 0x14, 0x15, 0xb1, 0x7a, 0xc0, 0x18, 0x00, 0x17, 0xce,
    0x8c, 0x6a, 0x60, 0x9e, 0xb1, 0x28, 0xd9, 0xb2, 0xe2, 0x11, 0x42, 0xf9, 0x10, 0x5e, 0x43, 0xb5,
};
const Bytes Sha256KeyHash {
    0xe5, 0x7b, 0xdd, 0xd8, 0xb2, 0x99, 0xa4, 0x06, 0xe7, 0xf0, 0xd9, 0x50, 0x35, 0xcb, 0x11, 0x56,
    0x46, 0x2c, 0x99, 0x82, 0xab, 0x06, 0x17, 0x3f, 0x39, 0x57, 0xc1, 0x1c, 0xc0, 0x13, 0xc2, 0x5a,
};
const Bytes Sha384NameHash {
    0x53, 0x07, 0xe5, 0x11, 0x3d, 0xb3, 0x76, 0xb7, 0xb4, 0x8f, 0x30, 0x10, 0x6c, 0xbd, 0xc2, 0x82,
    0x3a, 0x7a, 0x77, 0x98, 0x31, 0xb7, 0x9f, 0xb7, 0xca, 0x65, 0xba, 0x4d, 0xe1, 0xea, 0xbe, 0x18,
    0x75, 0x4c, 0x44, 0xde, 0x25, 0xa9, 0xbf, 0x62, 0x4a, 0xc2, 0x93, 0x79, 0x5e, 0x11, 0xad, 0xca,
};
const Bytes Sha384KeyHash {
    0x4b, 0x94, 0x80, 0xb6, 0xe1, 0xdb, 0x7a, 0x8b, 0x8d, 0x8c, 0x47, 0x6d, 0xfd, 0xcb, 0x5f, 0xf6,
    0xd8, 0x1f, 0x3e, 0xef, 0x5e, 0xce, 0x73, 0xce, 0x05, 0xed, 0xda, 0x9b, 0x4a, 0x96, 0x24, 0x03,
    0x68, 0x2f, 0x76, 0x31, 0xed, 0x1a, 0x1f, 0x56, 0x14, 0x0a, 0x4a, 0x2d, 0x55, 0x85, 0x1f, 0x73,
};
const Bytes Sha1EmptyKeyHash {
    0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b, 0x0d, 0x32, 0x55, 0xbf, 0xef, 0x95, 0x60, 0x18, 0x90,
    0xaf, 0xd8, 0x07, 0x09,
};

constexpr int Sha1Nid = 64;
constexpr int Sha256Nid = 672;
constexpr int Sha384Nid = 673;
constexpr int IntegerType = 2;
constexpr int NegativeIntegerType = 0x102;
constexpr int OctetStringType = 4;
constexpr int NullType = 5;

struct BufMem {
    std::size_t length;
    const unsigned char* data;
    std::size_t max;
};

struct Name {
    BufMem bytes {};
    LibreSslX509Name name {};

    explicit Name(const Bytes& der) {
        bytes = BufMem {der.size(), der.data(), der.size()};
        name = LibreSslX509Name {nullptr, 0, &bytes, nullptr, 0};
    }
};

struct Fixture {
    Name issuerName {IssuerName};
    Name leafName {LeafName};
    Asn1String serial {static_cast<std::int32_t>(Serial.size()), IntegerType, const_cast<unsigned char*>(Serial.data()), 0x10 | 0x20};
    LibreSsl3Asn1String issuerBits {static_cast<int>(IssuerKey.size()), 3, const_cast<unsigned char*>(IssuerKey.data()), 0x08};
    LibreSsl3X509Pubkey issuerKey {};
    LibreSsl3CertificateInfo leafInfo {};
    LibreSsl3CertificateInfo issuerInfo {};
    LibreSsl3Certificate leaf {};
    LibreSsl3Certificate issuer {};

    Fixture() {
        std::memset(&issuerKey, 0xa5, sizeof(issuerKey));
        std::memset(&leafInfo, 0xa5, sizeof(leafInfo));
        std::memset(&issuerInfo, 0xa5, sizeof(issuerInfo));
        std::memset(&leaf, 0x5a, sizeof(leaf));
        std::memset(&issuer, 0x5a, sizeof(issuer));
        issuerKey.publicKey = &issuerBits;
        leafInfo.issuer = &issuerName.name;
        leafInfo.serialNumber = &serial;
        issuerInfo.subject = &issuerName.name;
        issuerInfo.key = &issuerKey;
        leaf.certInfo = &leafInfo;
        issuer.certInfo = &issuerInfo;
    }
};

void RequireError(int library, int reason, std::source_location location = std::source_location::current()) {
    const std::uint64_t code = ERR_get_error_nid_postfix();
    Require(((code >> 24) & 0xff) == static_cast<std::uint64_t>(library) && (code & 0xfff) == static_cast<std::uint64_t>(reason), location);
    Require(ERR_get_error_nid_postfix() == 0, location);
}

void RequireNoError(std::source_location location = std::source_location::current()) {
    Require(ERR_get_error_nid_postfix() == 0, location);
}

void RequireString(const Asn1String* string, int type, const Bytes& expected, std::int64_t flags = 0, std::source_location location = std::source_location::current()) {
    Require(string != nullptr, location);
    Require(string->type == type && string->flags == flags, location);
    Require(string->length == static_cast<std::int32_t>(expected.size()), location);
    if (expected.empty()) {
        Require(string->data == nullptr, location);
        return;
    }
    Require(string->data != nullptr && std::memcmp(string->data, expected.data(), expected.size()) == 0, location);
    Require(string->data[expected.size()] == 0, location);
}

void RequireCertId(OCSP_CERTID* id, int nid, const Bytes& nameHash, const Bytes& keyHash, bool withSerial, std::source_location location = std::source_location::current()) {
    Require(id != nullptr && id->hashAlgorithm != nullptr, location);
    const auto* object = static_cast<const LibreSslAsn1Object*>(id->hashAlgorithm->algorithm);
    Require(object != nullptr && object->nid == nid && (object->flags & 0x01) == 0, location);
    const auto* parameter = reinterpret_cast<const Asn1Type*>(id->hashAlgorithm->parameter);
    Require(parameter != nullptr && parameter->type == NullType && parameter->value.pointer == nullptr, location);
    RequireString(id->issuerNameHash, OctetStringType, nameHash, 0, location);
    RequireString(id->issuerKeyHash, OctetStringType, keyHash, 0, location);
    if (withSerial) {
        RequireString(id->serialNumber, IntegerType, Serial, 0x20, location);
        Require(id->serialNumber->data != Serial.data(), location);
    } else {
        RequireString(id->serialNumber, IntegerType, {}, 0, location);
    }
    OCSP_CERTID_free_nid_postfix(id);
    Require(liveBlocks == 0, location);
    RequireNoError(location);
}

const EvpMd* LibreSsl3Sha1() {
    return reinterpret_cast<const EvpMd*>(EVP_sha1_nid_postfix());
}

void CheckRealCertificates() {
    Fixture fixture;
    RequireCertId(OCSP_cert_to_id_nid_postfix(nullptr, &fixture.leaf, &fixture.issuer), Sha1Nid, Sha1NameHash, Sha1KeyHash, true);
    RequireCertId(OCSP_cert_to_id_nid_postfix(LibreSsl3Sha1(), &fixture.leaf, &fixture.issuer), Sha1Nid, Sha1NameHash, Sha1KeyHash, true);
    RequireCertId(OCSP_cert_to_id_nid_postfix(EVP_sha256_nid_postfix(), &fixture.leaf, &fixture.issuer), Sha256Nid, Sha256NameHash, Sha256KeyHash, true);
    RequireCertId(OCSP_cert_to_id_nid_postfix(EVP_sha384_nid_postfix(), &fixture.leaf, &fixture.issuer), Sha384Nid, Sha384NameHash, Sha384KeyHash, true);
}

void CheckNameSources() {
    Fixture fixture;
    fixture.issuerInfo.subject = &fixture.leafName.name;
    RequireCertId(OCSP_cert_to_id_nid_postfix(nullptr, &fixture.leaf, &fixture.issuer), Sha1Nid, Sha1NameHash, Sha1KeyHash, true);

    Fixture withoutSubject;
    RequireCertId(OCSP_cert_to_id_nid_postfix(nullptr, nullptr, &withoutSubject.issuer), Sha1Nid, Sha1NameHash, Sha1KeyHash, false);
}

void CheckSerialNumbers() {
    Fixture negative;
    negative.serial.type = NegativeIntegerType;
    OCSP_CERTID* id = OCSP_cert_to_id_nid_postfix(nullptr, &negative.leaf, &negative.issuer);
    Require(id != nullptr && id->serialNumber->type == NegativeIntegerType && id->serialNumber->flags == 0x20);
    OCSP_CERTID_free_nid_postfix(id);

    Fixture empty;
    empty.serial = Asn1String {0, IntegerType, nullptr, 0};
    id = OCSP_cert_to_id_nid_postfix(nullptr, &empty.leaf, &empty.issuer);
    Require(id != nullptr && id->serialNumber->length == 0 && id->serialNumber->data != nullptr && id->serialNumber->data[0] == 0);
    OCSP_CERTID_free_nid_postfix(id);

    Fixture invalid;
    invalid.serial.length = -1;
    Require(OCSP_cert_to_id_nid_postfix(nullptr, &invalid.leaf, &invalid.issuer) == nullptr);
    Require(liveBlocks == 0);
    RequireNoError();

    Fixture missing;
    missing.leafInfo.serialNumber = nullptr;
    RequireCertId(OCSP_cert_to_id_nid_postfix(nullptr, &missing.leaf, &missing.issuer), Sha1Nid, Sha1NameHash, Sha1KeyHash, false);
}

void CheckMissingIssuerKey() {
    Fixture fixture;
    Require(OCSP_cert_to_id_nid_postfix(nullptr, &fixture.leaf, nullptr) == nullptr);
    fixture.issuerKey.publicKey = nullptr;
    Require(OCSP_cert_to_id_nid_postfix(nullptr, &fixture.leaf, &fixture.issuer) == nullptr);
    Require(liveBlocks == 0);
    RequireNoError();

    Fixture emptyKey;
    emptyKey.issuerBits = LibreSsl3Asn1String {0, 3, nullptr, 0};
    RequireCertId(OCSP_cert_to_id_nid_postfix(nullptr, &emptyKey.leaf, &emptyKey.issuer), Sha1Nid, Sha1NameHash, Sha1EmptyKeyHash, true);
}

void CheckInvalidInputs() {
    Fixture fixture;
    RequireThrows([] { OCSP_cert_to_id_nid_postfix(nullptr, nullptr, nullptr); });

    fixture.issuerInfo.key = nullptr;
    RequireThrows([&] { OCSP_cert_to_id_nid_postfix(nullptr, &fixture.leaf, &fixture.issuer); });
    fixture.issuerInfo.key = &fixture.issuerKey;

    fixture.issuer.certInfo = nullptr;
    RequireThrows([&] { OCSP_cert_to_id_nid_postfix(nullptr, &fixture.leaf, &fixture.issuer); });
    fixture.issuer.certInfo = &fixture.issuerInfo;

    fixture.issuerBits.length = -1;
    RequireThrows([&] { OCSP_cert_to_id_nid_postfix(nullptr, &fixture.leaf, &fixture.issuer); });
    Require(liveBlocks == 0);
    fixture.issuerBits.length = static_cast<int>(IssuerKey.size());

    fixture.leafInfo.issuer = nullptr;
    RequireThrows([&] { OCSP_cert_to_id_nid_postfix(nullptr, &fixture.leaf, &fixture.issuer); });
    fixture.leafInfo.issuer = &fixture.issuerName.name;
    Require(liveBlocks == 0);
    ERR_clear_error_nid_postfix();
}

void CheckIssuerNameEncoding() {
    Fixture modified;
    modified.issuerName.name.modified = 1;
    RequireThrows([&] { OCSP_cert_to_id_nid_postfix(nullptr, &modified.leaf, &modified.issuer); });
    Require(liveBlocks == 0);

    Fixture missing;
    missing.issuerName.name.bytes = nullptr;
    RequireThrows([&] { OCSP_cert_to_id_nid_postfix(nullptr, &missing.leaf, &missing.issuer); });
    Require(liveBlocks == 0);

    Fixture empty;
    empty.issuerName.bytes.length = 0;
    Require(OCSP_cert_to_id_nid_postfix(nullptr, &empty.leaf, &empty.issuer) == nullptr);
    Require(liveBlocks == 0);
    RequireError(39, 102);
}

int (APS5_VABI* realFinal)(EvpMdContext*, unsigned char*) = nullptr;
int finalCalls = 0;
int failingFinalCall = 0;

int APS5_VABI CountingFinal(EvpMdContext* context, unsigned char* digest) {
    if (++finalCalls == failingFinalCall) return 0;
    return realFinal(context, digest);
}

void CheckDigests() {
    Fixture fixture;
    EvpMd undefined = *EVP_sha256_nid_postfix();
    undefined.type = 0;
    Require(OCSP_cert_to_id_nid_postfix(&undefined, &fixture.leaf, &fixture.issuer) == nullptr);
    Require(liveBlocks == 0);
    RequireError(39, 120);

    EvpMd unknown = *EVP_sha256_nid_postfix();
    unknown.type = 5000;
    Require(OCSP_cert_to_id_nid_postfix(&unknown, &fixture.leaf, &fixture.issuer) == nullptr);
    Require(liveBlocks == 0);
    RequireError(8, 101);

    EvpMd counting = *EVP_sha256_nid_postfix();
    realFinal = counting.finalize;
    counting.finalize = &CountingFinal;
    finalCalls = 0;
    failingFinalCall = 1;
    Require(OCSP_cert_to_id_nid_postfix(&counting, &fixture.leaf, &fixture.issuer) == nullptr);
    Require(liveBlocks == 0);
    RequireError(39, 102);

    finalCalls = 0;
    failingFinalCall = 2;
    Require(OCSP_cert_to_id_nid_postfix(&counting, &fixture.leaf, &fixture.issuer) == nullptr);
    Require(liveBlocks == 0);
    while (const std::uint64_t code = ERR_get_error_nid_postfix()) Require(((code >> 24) & 0xff) != 39);

    finalCalls = 0;
    failingFinalCall = 3;
    RequireCertId(OCSP_cert_to_id_nid_postfix(&counting, &fixture.leaf, &fixture.issuer), Sha256Nid, Sha256NameHash, Sha256KeyHash, true);

    EvpMd broken = *EVP_sha256_nid_postfix();
    broken.init = nullptr;
    RequireThrows([&] { OCSP_cert_to_id_nid_postfix(&broken, &fixture.leaf, &fixture.issuer); });
    Require(liveBlocks == 0);
    ERR_clear_error_nid_postfix();
}

void CheckAllocationFailures() {
    Fixture fixture;
    int failures = 0;
    for (int failing = 1;; ++failing) {
        zeroedAllocations = 0;
        failAllocation = failing;
        OCSP_CERTID* id = OCSP_cert_to_id_nid_postfix(nullptr, &fixture.leaf, &fixture.issuer);
        failAllocation = 0;
        if (id) {
            RequireCertId(id, Sha1Nid, Sha1NameHash, Sha1KeyHash, true);
            break;
        }
        ++failures;
        Require(liveBlocks == 0);
        RequireError(13, 65);
    }
    Require(failures == 10);
}

}

int main() {
    InitializeHeap();
    ERR_clear_error_nid_postfix();
    CheckRealCertificates();
    CheckNameSources();
    CheckSerialNumbers();
    CheckMissingIssuerKey();
    CheckInvalidInputs();
    CheckIssuerNameEncoding();
    CheckDigests();
    CheckAllocationFailures();
    Require(liveBlocks == 0);
    std::puts("OCSP_cert_to_id: real certificates, inputs, digests and allocation failures passed");
    return 0;
}
