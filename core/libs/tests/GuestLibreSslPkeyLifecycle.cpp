#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <stdexcept>
#include <vector>

namespace {
bool failNextAllocation = false;
int liveAllocations = 0;
int allocationAttempts = 0;
int frees = 0;

struct TestPkey {
    int type;
    int saveType;
    int references;
    const void* ameth;
    void* engine;
    void* key;
    int saveParameters;
    void* attributes;
};

std::int32_t EcReferences(const EcKey* key) {
    std::int32_t references = 0;
    std::memcpy(&references, reinterpret_cast<const std::byte*>(key) + 48, sizeof(references));
    return references;
}

using Bytes = std::vector<unsigned char>;

Bytes Cat(std::initializer_list<Bytes> parts) {
    Bytes joined;
    for (const auto& part : parts) joined.insert(joined.end(), part.begin(), part.end());
    return joined;
}

Bytes Tlv(unsigned char tag, const Bytes& content) {
    Bytes encoded{tag};
    if (content.size() < 128) {
        encoded.push_back(static_cast<unsigned char>(content.size()));
    } else if (content.size() < 256) {
        encoded.push_back(0x81);
        encoded.push_back(static_cast<unsigned char>(content.size()));
    } else {
        encoded.push_back(0x82);
        encoded.push_back(static_cast<unsigned char>(content.size() >> 8));
        encoded.push_back(static_cast<unsigned char>(content.size()));
    }
    encoded.insert(encoded.end(), content.begin(), content.end());
    return encoded;
}

Bytes EcCertificateDer() {
    const Bytes sha256WithRsa{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b};
    const Bytes commonName{0x55, 0x04, 0x03};
    const Bytes ecPublicKey{0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01};
    const Bytes curvePrime256{0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07};
    const Bytes ecPoint{
        0x04, 0xaa, 0xef, 0x40, 0xaa, 0x75, 0xba, 0xb3, 0x9a, 0xbc, 0x01, 0xb3, 0x0f, 0x72, 0x05, 0xfb, 0xd5,
        0x92, 0xfa, 0xfc, 0x7f, 0xd7, 0xbc, 0xad, 0xa4, 0x39, 0x71, 0x2d, 0xd1, 0x32, 0x7f, 0x90, 0xbb,
        0x8b, 0x62, 0x6d, 0x8f, 0x04, 0x3c, 0xea, 0xa6, 0x1b, 0x4d, 0x2c, 0x5b, 0x97, 0x04, 0x38, 0x6f,
        0x9f, 0x02, 0x9d, 0xb0, 0xa0, 0xd8, 0xc6, 0x4d, 0xd0, 0x03, 0x43, 0xa2, 0x61, 0xfe, 0x2b, 0xb2,
    };
    const Bytes algorithm = Tlv(0x30, Cat({Tlv(0x06, sha256WithRsa), Tlv(0x05, {})}));
    auto name = [&](std::initializer_list<unsigned char> label) {
        return Tlv(0x30, Tlv(0x31, Tlv(0x30, Cat({Tlv(0x06, commonName), Tlv(0x0c, label)}))));
    };
    const Bytes validity = Tlv(0x30, Cat({
        Tlv(0x17, {'2', '0', '0', '1', '0', '1', '0', '0', '0', '0', '0', '0', 'Z'}),
        Tlv(0x18, {'2', '0', '5', '0', '0', '1', '0', '1', '0', '0', '0', '0', '0', '0', 'Z'}),
    }));
    const Bytes keyInfo = Tlv(0x30, Cat({
        Tlv(0x30, Cat({Tlv(0x06, ecPublicKey), Tlv(0x06, curvePrime256)})),
        Tlv(0x03, Cat({{0x00}, ecPoint})),
    }));
    const Bytes info = Tlv(0x30, Cat({
        Tlv(0x02, {0x01}),
        algorithm,
        name({'T', 'e', 's', 't', ' ', 'C', 'A'}),
        validity,
        name({'L', 'e', 'a', 'f'}),
        keyInfo,
    }));
    return Tlv(0x30, Cat({info, algorithm, Tlv(0x03, {0x03, 0xde, 0xad, 0xbf})}));
}

constexpr std::uint64_t ErrorCode(int library, int function, int reason) {
    return (static_cast<std::uint64_t>(library) << 24) |
           (static_cast<std::uint64_t>(function) << 12) |
           static_cast<std::uint64_t>(reason);
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::terminate();
}

void* APS5_VABI Allocate(std::size_t size) {
    ++allocationAttempts;
    if (failNextAllocation) {
        failNextAllocation = false;
        return nullptr;
    }
    void* const result = std::malloc(size);
    if (result) ++liveAllocations;
    return result;
}
void APS5_VABI Free(void* pointer) {
    if (pointer) {
        --liveAllocations;
        ++frees;
    }
    std::free(pointer);
}
void* APS5_VABI Calloc(std::size_t count, std::size_t size) {
    void* const result = std::calloc(count, size);
    if (result) ++liveAllocations;
    return result;
}
void* APS5_VABI Reallocate(void* pointer, std::size_t size) {
    void* const result = std::realloc(pointer, size);
    if (result && !pointer) ++liveAllocations;
    if (!result && size == 0 && pointer) --liveAllocations;
    return result;
}
void* APS5_VABI Align(std::size_t, std::size_t) {
    throw std::logic_error("unexpected aligned key allocation");
}
void* APS5_VABI Realign(void* pointer, std::size_t size, std::size_t alignment) {
    static_cast<void>(pointer);
    static_cast<void>(size);
    static_cast<void>(alignment);
    return nullptr;
}
int APS5_VABI PosixAlign(void** output, std::size_t alignment, std::size_t size) {
    static_cast<void>(output);
    static_cast<void>(alignment);
    static_cast<void>(size);
    return 12;
}
}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Free), reinterpret_cast<void*>(Calloc),
        reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(Align), reinterpret_cast<void*>(Realign),
        reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    void* pkey = EVP_PKEY_new_nid_postfix();
    Require(pkey != nullptr, "allocates a key");
    int fields[3];
    std::memcpy(fields, pkey, sizeof(fields));
    Require(fields[0] == 0 && fields[1] == 0, "a new key has type and save_type EVP_PKEY_NONE");
    Require(fields[2] == 1, "a new key holds one reference");
    const int extra = 2;
    std::memcpy(static_cast<std::byte*>(pkey) + 8, &extra, sizeof(extra));
    EVP_PKEY_free_nid_postfix(pkey);
    std::memcpy(fields, pkey, sizeof(fields));
    Require(fields[2] == 1, "freeing a shared key only drops one reference");
    EVP_PKEY_free_nid_postfix(pkey);
    EVP_PKEY_free_nid_postfix(nullptr);
    Require(liveAllocations == 0, "key lifecycle releases its allocation");

    constexpr std::uint64_t firstSentinel = ErrorCode(2, 3, 17);
    constexpr std::uint64_t secondSentinel = ErrorCode(4, 5, 19);
    const int allocationsBeforeUnsupportedType = allocationAttempts;
    const int freesBeforeUnsupportedType = frees;
    LibreSslPutError_nid_no_patch(2, 3, 17);
    LibreSslPutError_nid_no_patch(4, 5, 19);
    const int result = SSL_CTX_use_PrivateKey_ASN1_nid_postfix(
        0, nullptr, reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(1)), -77);
    Require(result == 0, "an unsupported EVP_PKEY_NONE type fails before reading context or DER");
    Require(ERR_get_error_nid_postfix() == firstSentinel && ERR_get_error_nid_postfix() == secondSentinel,
        "the unsupported key type preserves earlier errors in FIFO order");
    Require(ERR_get_error_nid_postfix() == ErrorCode(6, 0xfff, 156), "unsupported algorithm error is first");
    Require(ERR_get_error_nid_postfix() == ErrorCode(13, 0xfff, 163), "ASN.1 unknown key type error is second");
    Require(ERR_get_error_nid_postfix() == ErrorCode(20, 0xfff, 13), "SSL ASN.1 wrapper error is last");
    Require(ERR_get_error_nid_postfix() == 0, "the wrapper adds exactly three errors");
    Require(allocationAttempts == allocationsBeforeUnsupportedType + 1, "EVP_PKEY_NONE allocates one temporary key");
    Require(frees == freesBeforeUnsupportedType + 1 && liveAllocations == 0, "unsupported key type frees its temporary key");

    constexpr std::uint64_t allocationSentinel = ErrorCode(2, 3, 23);
    const int allocationsBeforeFailure = allocationAttempts;
    const int freesBeforeFailure = frees;
    LibreSslPutError_nid_no_patch(2, 3, 23);
    failNextAllocation = true;
    Require(SSL_CTX_use_PrivateKey_ASN1_nid_postfix(
        0, nullptr, reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(1)), -77) == 0,
        "a failed EVP_PKEY allocation returns zero");
    Require(ERR_get_error_nid_postfix() == allocationSentinel, "allocation failure preserves existing errors");
    Require(ERR_get_error_nid_postfix() == ErrorCode(6, 0xfff, 65), "EVP_PKEY allocation failure queues EVP malloc first");
    Require(ERR_get_error_nid_postfix() == ErrorCode(13, 0xfff, 6), "d2i queues ASN.1 EVP-library after allocation failure");
    Require(ERR_get_error_nid_postfix() == ErrorCode(20, 0xfff, 13), "SSL appends ASN.1-library after d2i failure");
    Require(ERR_get_error_nid_postfix() == 0, "allocation failure adds exactly three errors after the sentinel");
    Require(allocationAttempts == allocationsBeforeFailure + 1, "the OOM path attempts one EVP_PKEY allocation");
    Require(frees == freesBeforeFailure, "the OOM path does not free an unallocated key");
    Require(liveAllocations == 0, "a failed key allocation leaves no live key");

    try {
        EVP_PKEY_get1_EC_KEY_nid_postfix(nullptr);
        Require(false, "null pkey throws invalid_argument");
    } catch (const std::invalid_argument&) {}

    void* const nonEcKey = EVP_PKEY_new_nid_postfix();
    Require(nonEcKey != nullptr, "allocates non-EC key");
    Require(EVP_PKEY_get1_EC_KEY_nid_postfix(nonEcKey) == nullptr, "non-EC key returns nullptr");
    Require(ERR_get_error_nid_postfix() == ErrorCode(6, 0xfff, 142), "queues ReasonExpectingAnEcKey");
    EVP_PKEY_free_nid_postfix(nonEcKey);

    void* const emptyEcKey = EVP_PKEY_new_nid_postfix();
    Require(emptyEcKey != nullptr, "allocates empty EC key");
    auto* const emptyTestPkey = static_cast<TestPkey*>(emptyEcKey);
    emptyTestPkey->type = 408;
    emptyTestPkey->saveType = 408;
    try {
        EVP_PKEY_get1_EC_KEY_nid_postfix(emptyEcKey);
        Require(false, "EC key without object throws");
    } catch (const std::invalid_argument&) {}
    EVP_PKEY_free_nid_postfix(emptyEcKey);

    const Bytes ecCertDer = EcCertificateDer();
    auto createEcPublicKey = [&]() -> void* {
        const unsigned char* cursor = ecCertDer.data();
        LibreSslX509* const cert = d2i_X509_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(ecCertDer.size()));
        Require(cert != nullptr, "decodes EC certificate");
        void* const pkey = X509_get_pubkey_nid_postfix(cert);
        Require(pkey != nullptr, "extracts EC public key");
        X509_free_nid_postfix(cert);
        Require(ERR_get_error_nid_postfix() == 0, "extracting EC public key queues no errors");
        Require(liveAllocations == 1, "only the extracted EC EVP_PKEY stays on guest heap");
        return pkey;
    };

    void* const validEcPkey = createEcPublicKey();
    auto* const validTestPkey = static_cast<TestPkey*>(validEcPkey);
    Require(validTestPkey->type == 408 && validTestPkey->saveType == 408, "decoded EC pkey has EC type");
    Require(validTestPkey->ameth != nullptr && validTestPkey->key != nullptr, "decoded EC pkey binds method and object");
    auto* const createdEc = static_cast<EcKey*>(validTestPkey->key);
    Require(EcReferences(createdEc) == 1, "decoded EC key has one reference");

    EcKey* const retrievedEc = EVP_PKEY_get1_EC_KEY_nid_postfix(validEcPkey);
    Require(retrievedEc == createdEc, "retrieves same EC key pointer");
    Require(EcReferences(retrievedEc) == 2, "increments EC key references");

    EVP_PKEY_free_nid_postfix(validEcPkey);
    Require(liveAllocations == 0, "freeing valid EC pkey releases its EVP_PKEY allocation");
    Require(EcReferences(retrievedEc) == 1, "EVP_PKEY_free drops one EC key reference");
    EC_KEY_free_nid_postfix(retrievedEc);

    void* const soleEcPkey = createEcPublicKey();
    EVP_PKEY_free_nid_postfix(soleEcPkey);
    Require(liveAllocations == 0, "freeing sole EC pkey releases all allocations");

    {
        const unsigned char* cursor = ecCertDer.data();
        LibreSslX509* const cert = d2i_X509_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(ecCertDer.size()));
        Require(cert != nullptr, "decodes shared EC certificate");
        void* const firstEcPkey = X509_get_pubkey_nid_postfix(cert);
        void* const secondEcPkey = X509_get_pubkey_nid_postfix(cert);
        Require(firstEcPkey != nullptr && firstEcPkey == secondEcPkey, "cached EC pkey is shared");
        X509_free_nid_postfix(cert);
        auto* const sharedFields = static_cast<TestPkey*>(firstEcPkey);
        auto* const sharedEc = static_cast<EcKey*>(sharedFields->key);
        Require(sharedFields->references == 2 && EcReferences(sharedEc) == 1, "two caller refs remain on shared EC pkey");
        EVP_PKEY_free_nid_postfix(firstEcPkey);
        Require(sharedFields->references == 1 && EcReferences(sharedEc) == 1 && liveAllocations == 1,
            "dropping first EC pkey ref keeps key and EC_KEY alive");
        try {
            EVP_PKEY_copy_parameters_nid_postfix(secondEcPkey, secondEcPkey);
            Require(false, "copying EC parameters throws NotImplemented");
        } catch (const std::exception&) {}
        EVP_PKEY_free_nid_postfix(secondEcPkey);
        Require(liveAllocations == 0, "dropping last EC pkey ref releases all allocations");
    }

    {
        EcKey* const unattachedEc = EC_KEY_new_by_curve_name_nid_postfix(415);
        Require(unattachedEc != nullptr, "allocates unattached EC key");
        void* const typeOnlyEcPkey = EVP_PKEY_new_nid_postfix();
        Require(typeOnlyEcPkey != nullptr, "allocates type-only EC pkey");
        auto* const typeOnlyEcFields = static_cast<TestPkey*>(typeOnlyEcPkey);
        typeOnlyEcFields->type = 408;
        typeOnlyEcFields->saveType = 408;
        typeOnlyEcFields->key = unattachedEc;
        try {
            EVP_PKEY_free_nid_postfix(typeOnlyEcPkey);
            Require(false, "EC pkey without method throws instead of freeing by type");
        } catch (const std::exception&) {}
        Require(typeOnlyEcFields->references == 1, "failed EC free restores pkey reference");
        Require(EcReferences(unattachedEc) == 1, "failed EC free does not release unattached EC key");
        typeOnlyEcFields->key = nullptr;
        EVP_PKEY_free_nid_postfix(typeOnlyEcPkey);
        EC_KEY_free_nid_postfix(unattachedEc);
        Require(liveAllocations == 0, "cleanup after type-only EC rejection leaves no live allocations");
    }

    {
        LibreSslRsa* const unattachedRsa = RSA_new_nid_postfix();
        Require(unattachedRsa != nullptr, "allocates unattached RSA key");
        void* const typeOnlyRsaPkey = EVP_PKEY_new_nid_postfix();
        Require(typeOnlyRsaPkey != nullptr, "allocates type-only RSA pkey");
        auto* const typeOnlyRsaFields = static_cast<TestPkey*>(typeOnlyRsaPkey);
        typeOnlyRsaFields->type = 6;
        typeOnlyRsaFields->saveType = 6;
        typeOnlyRsaFields->key = unattachedRsa;
        try {
            EVP_PKEY_free_nid_postfix(typeOnlyRsaPkey);
            Require(false, "RSA pkey without method throws instead of freeing by type");
        } catch (const std::exception&) {}
        Require(typeOnlyRsaFields->references == 1, "failed RSA free restores pkey reference");
        Require(unattachedRsa->references == 1, "failed RSA free does not release unattached RSA key");
        typeOnlyRsaFields->key = nullptr;
        EVP_PKEY_free_nid_postfix(typeOnlyRsaPkey);
        RSA_free_nid_postfix(unattachedRsa);
        Require(liveAllocations == 0, "cleanup after type-only RSA rejection leaves no live allocations");
    }

    {
        const int customMethodMarker = 999;
        void* const customMethodPkey = EVP_PKEY_new_nid_postfix();
        Require(customMethodPkey != nullptr, "allocates custom-method pkey");
        auto* const customFields = static_cast<TestPkey*>(customMethodPkey);
        customFields->type = 408;
        customFields->saveType = 408;
        customFields->ameth = &customMethodMarker;
        try {
            EVP_PKEY_free_nid_postfix(customMethodPkey);
            Require(false, "unknown algorithm method throws");
        } catch (const std::exception&) {}
        Require(customFields->references == 1, "failed custom-method free restores pkey reference");
        customFields->ameth = nullptr;
        EVP_PKEY_free_nid_postfix(customMethodPkey);
        Require(liveAllocations == 0, "cleanup after custom-method rejection leaves no live allocations");
    }

    {
        void* const emptyTypedPkey = EVP_PKEY_new_nid_postfix();
        Require(emptyTypedPkey != nullptr, "allocates typed pkey without method or key");
        static_cast<TestPkey*>(emptyTypedPkey)->type = 28;
        EVP_PKEY_free_nid_postfix(emptyTypedPkey);
        Require(liveAllocations == 0, "empty typed pkey without method frees cleanly");
    }

    void* const pkeyA = EVP_PKEY_new_nid_postfix();
    void* const pkeyB = EVP_PKEY_new_nid_postfix();
    static_cast<TestPkey*>(pkeyA)->type = 408;
    static_cast<TestPkey*>(pkeyB)->type = 6;
    Require(EVP_PKEY_copy_parameters_nid_postfix(pkeyA, pkeyB) == 0, "mismatched types return zero");
    Require(ERR_get_error_nid_postfix() == ErrorCode(6, 0xfff, 101), "queues ReasonDifferentKeyTypes");
    EVP_PKEY_free_nid_postfix(pkeyA);
    EVP_PKEY_free_nid_postfix(pkeyB);
    Require(liveAllocations == 0, "cleanup leaves no live allocations");

    return 0;
}
