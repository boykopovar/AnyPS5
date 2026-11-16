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
#include <stdexcept>

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

    EcKey* const createdEc = EC_KEY_new_by_curve_name_nid_postfix(415);
    Require(createdEc != nullptr, "allocates curve key");
    Require(EcReferences(createdEc) == 1, "new EC key has one reference");

    void* const validEcPkey = EVP_PKEY_new_nid_postfix();
    Require(validEcPkey != nullptr, "allocates valid EC pkey");
    auto* const validTestPkey = static_cast<TestPkey*>(validEcPkey);
    validTestPkey->type = 408;
    validTestPkey->saveType = 408;
    validTestPkey->key = createdEc;

    EcKey* const retrievedEc = EVP_PKEY_get1_EC_KEY_nid_postfix(validEcPkey);
    Require(retrievedEc == createdEc, "retrieves same EC key pointer");
    Require(EcReferences(retrievedEc) == 2, "increments EC key references");

    EVP_PKEY_free_nid_postfix(validEcPkey);
    Require(liveAllocations == 0, "freeing valid EC pkey releases its EVP_PKEY allocation");
    Require(EcReferences(retrievedEc) == 1, "EVP_PKEY_free drops one EC key reference");
    EC_KEY_free_nid_postfix(retrievedEc);

    EcKey* const soleEc = EC_KEY_new_by_curve_name_nid_postfix(415);
    Require(soleEc != nullptr, "allocates sole curve key");
    void* const soleEcPkey = EVP_PKEY_new_nid_postfix();
    Require(soleEcPkey != nullptr, "allocates sole EC pkey");
    auto* const soleTestPkey = static_cast<TestPkey*>(soleEcPkey);
    soleTestPkey->type = 408;
    soleTestPkey->saveType = 408;
    soleTestPkey->key = soleEc;
    EVP_PKEY_free_nid_postfix(soleEcPkey);
    Require(liveAllocations == 0, "freeing sole EC pkey releases all allocations");

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
