#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <unordered_map>

namespace {

using String = Asn1String;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 ECDSA check failed at line %u\n", location.line());
    std::abort();
}

std::unordered_map<void*, std::size_t> allocations;
std::unordered_map<void*, std::size_t> zeroed;
int allocationsUntilFailure = -1;

bool FailAllocation() {
    if (allocationsUntilFailure == 0) {
        allocationsUntilFailure = -1;
        return true;
    }
    if (allocationsUntilFailure > 0) --allocationsUntilFailure;
    return false;
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (FailAllocation()) return nullptr;
    void* result = std::malloc(size);
    Require(result != nullptr);
    allocations.emplace(result, size);
    return result;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    const auto found = allocations.find(pointer);
    Require(found != allocations.end());
    const auto cleared = zeroed.find(pointer);
    if (cleared != zeroed.end()) {
        const auto* bytes = static_cast<const unsigned char*>(pointer);
        for (std::size_t index = 0; index < cleared->second; ++index) Require(bytes[index] == 0);
        zeroed.erase(cleared);
    }
    allocations.erase(found);
    std::free(pointer);
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (FailAllocation()) return nullptr;
    if (!pointer) return Allocate_nid_no_patch(size);
    const auto found = allocations.find(pointer);
    Require(found != allocations.end());
    void* result = std::realloc(pointer, size);
    Require(result != nullptr);
    allocations.erase(found);
    allocations.emplace(result, size);
    return result;
}

void* APS5_VABI Callocate_nid_no_patch(std::size_t count, std::size_t size) {
    if (FailAllocation()) return nullptr;
    void* result = std::calloc(count, size);
    Require(result != nullptr);
    allocations.emplace(result, count * size);
    return result;
}

void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

constexpr std::uint64_t Asn1Error(std::uint64_t reason) {
    return 0x0dfff000 | reason;
}

void ExpectError(std::uint64_t expected) {
    Require(ERR_get_error_nid_postfix() == expected);
    Require(ERR_get_error_nid_postfix() == 0);
}

Bignum* OwnedBignum() {
    auto* value = static_cast<Bignum*>(Allocate_nid_no_patch(sizeof(Bignum)));
    value->d = static_cast<std::uint64_t*>(Allocate_nid_no_patch(2 * sizeof(std::uint64_t)));
    value->d[0] = 1;
    value->d[1] = 0;
    value->top = 1;
    value->dmax = 2;
    value->neg = 0;
    value->flags = 1;
    return value;
}

String* OwnedString(unsigned char byte) {
    auto* value = static_cast<String*>(Allocate_nid_no_patch(sizeof(String)));
    auto* data = static_cast<unsigned char*>(Allocate_nid_no_patch(2));
    data[0] = byte;
    data[1] = 0;
    *value = {1, 2, data, 0};
    return value;
}

void SignatureNew() {
    LibreSsl3EcdsaSig* signature = ECDSA_SIG_new_nid_postfix();
    Require(signature != nullptr);
    Require(signature->r == nullptr && signature->s == nullptr);
    Require(allocations.size() == 1);

    allocationsUntilFailure = 0;
    Require(ECDSA_SIG_new_nid_postfix() == nullptr);
    Require(allocations.size() == 1);
    ExpectError(Asn1Error(65));
    free_nid_postfix(signature);
}

void SignatureSet() {
    LibreSsl3EcdsaSig* signature = ECDSA_SIG_new_nid_postfix();
    Require(signature != nullptr);
    Require(ECDSA_SIG_set0_nid_postfix(signature, nullptr, nullptr) == 0);
    Require(signature->r == nullptr && signature->s == nullptr);

    Bignum* r = OwnedBignum();
    Bignum* s = OwnedBignum();
    Require(ECDSA_SIG_set0_nid_postfix(signature, r, s) == 1);
    Require(signature->r == r && signature->s == s);

    Require(ECDSA_SIG_set0_nid_postfix(signature, nullptr, s) == 0);
    Require(signature->r == r && signature->s == s);

    Bignum* r2 = OwnedBignum();
    Bignum* s2 = OwnedBignum();
    Require(ECDSA_SIG_set0_nid_postfix(signature, r2, s2) == 1);
    Require(signature->r == r2 && signature->s == s2);
    Require(allocations.size() == 1 + 2 + 2);

    bool threw = false;
    try {
        ECDSA_SIG_set0_nid_postfix(nullptr, r2, s2);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    ECDSA_SIG_free_nid_postfix(signature);
    Require(allocations.empty());
    ECDSA_SIG_free_nid_postfix(nullptr);
}

void TypeFree() {
    auto* stringType = static_cast<Asn1Type*>(Allocate_nid_no_patch(sizeof(Asn1Type)));
    *stringType = {};
    stringType->type = 2;
    stringType->value.pointer = OwnedString(7);
    zeroed.emplace(static_cast<String*>(stringType->value.pointer)->data, 1);
    ASN1_TYPE_free_nid_postfix(stringType);
    Require(allocations.empty());

    auto* objectType = static_cast<Asn1Type*>(Allocate_nid_no_patch(sizeof(Asn1Type)));
    *objectType = {};
    objectType->type = 6;
    auto* object = static_cast<LibreSslAsn1Object*>(Allocate_nid_no_patch(sizeof(LibreSslAsn1Object)));
    auto* objectData = static_cast<unsigned char*>(Allocate_nid_no_patch(2));
    objectData[0] = 1;
    objectData[1] = 2;
    *object = {nullptr, nullptr, 1, 2, objectData, 0x01 | 0x08};
    objectType->value.pointer = object;
    zeroed.emplace(objectData, 2);
    ASN1_TYPE_free_nid_postfix(objectType);
    Require(allocations.empty());

    auto* nullDataObjectType = static_cast<Asn1Type*>(Allocate_nid_no_patch(sizeof(Asn1Type)));
    *nullDataObjectType = {};
    nullDataObjectType->type = 6;
    auto* nullDataObject = static_cast<LibreSslAsn1Object*>(Allocate_nid_no_patch(sizeof(LibreSslAsn1Object)));
    *nullDataObject = {nullptr, nullptr, 1, 4, nullptr, 0x01 | 0x08};
    nullDataObjectType->value.pointer = nullDataObject;
    ASN1_TYPE_free_nid_postfix(nullDataObjectType);
    Require(allocations.empty());

    auto* innerType = static_cast<Asn1Type*>(Allocate_nid_no_patch(sizeof(Asn1Type)));
    *innerType = {};
    innerType->type = 2;
    innerType->value.pointer = OwnedString(9);
    zeroed.emplace(static_cast<String*>(innerType->value.pointer)->data, 1);
    auto* anyType = static_cast<Asn1Type*>(Allocate_nid_no_patch(sizeof(Asn1Type)));
    *anyType = {};
    anyType->type = -4;
    anyType->value.pointer = innerType;
    ASN1_TYPE_free_nid_postfix(anyType);
    Require(allocations.empty());

    auto* booleanType = static_cast<Asn1Type*>(Allocate_nid_no_patch(sizeof(Asn1Type)));
    *booleanType = {};
    booleanType->type = 1;
    booleanType->value.boolean = 0xff;
    ASN1_TYPE_free_nid_postfix(booleanType);
    Require(allocations.empty());

    auto* nullType = static_cast<Asn1Type*>(Allocate_nid_no_patch(sizeof(Asn1Type)));
    *nullType = {};
    nullType->type = 5;
    ASN1_TYPE_free_nid_postfix(nullType);
    Require(allocations.empty());

    ASN1_TYPE_free_nid_postfix(nullptr);
}

}

int main() {
    std::array<void*, 7> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(Callocate_nid_no_patch), reinterpret_cast<void*>(Reallocate_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch), reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    SignatureNew();
    SignatureSet();
    TypeFree();
    Require(allocations.empty());
    std::puts("libSceLibreSsl3 ECDSA and ASN.1 type tests passed");
}
