#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <source_location>

namespace {

int liveBlocks = 0;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 RSAPublicKey_dup check failed at line %u\n", location.line());
    std::abort();
}

void* APS5_VABI Allocate(std::size_t bytes) {
    ++liveBlocks;
    return std::malloc(bytes ? bytes : 1);
}

void APS5_VABI Release(void* block) {
    if (block) --liveBlocks;
    std::free(block);
}

void* APS5_VABI AllocateZeroed(std::size_t count, std::size_t bytes) {
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

LibreSsl3Bignum* Number(std::initializer_list<std::uint64_t> words) {
    auto* number = static_cast<LibreSsl3Bignum*>(calloc_nid_postfix(1, sizeof(LibreSsl3Bignum)));
    Require(number != nullptr);
    const auto count = static_cast<int>(words.size());
    auto* limbs = static_cast<std::uint64_t*>(calloc_nid_postfix(static_cast<std::size_t>(count), sizeof(std::uint64_t)));
    Require(limbs != nullptr);
    int index = 0;
    for (std::uint64_t word : words) limbs[index++] = word;
    number->data = limbs;
    number->top = count;
    number->capacity = count;
    number->negative = 0;
    number->flags = 1;
    return number;
}

}

int main() {
    InitializeHeap();
    Require(RSAPublicKey_dup_nid_postfix(nullptr) == nullptr);

    LibreSsl3Rsa empty{};
    Require(RSAPublicKey_dup_nid_postfix(&empty) == nullptr);

    LibreSsl3Rsa missingExponent{};
    auto* onlyModulus = Number({0x0123456789abcdefULL, 0xfedcba9876543210ULL});
    missingExponent.n = onlyModulus;
    Require(RSAPublicKey_dup_nid_postfix(&missingExponent) == nullptr);
    missingExponent.n = nullptr;

    LibreSsl3Rsa missingModulus{};
    auto* onlyExponent = Number({65537});
    missingModulus.e = onlyExponent;
    Require(RSAPublicKey_dup_nid_postfix(&missingModulus) == nullptr);
    missingModulus.e = nullptr;

    BN_clear_free_nid_postfix(onlyModulus);
    BN_clear_free_nid_postfix(onlyExponent);

    auto* source = static_cast<LibreSsl3Rsa*>(calloc_nid_postfix(1, sizeof(LibreSsl3Rsa)));
    Require(source != nullptr);
    auto* n = Number({0x0123456789abcdefULL, 0xfedcba9876543210ULL});
    auto* e = Number({65537});
    const std::array<std::uint64_t, 2> modulusWords{0x0123456789abcdefULL, 0xfedcba9876543210ULL};
    Require(RSA_set0_key_nid_postfix(source, n, e, nullptr) == 1);

    const LibreSsl3Rsa sourceBefore = *source;
    const LibreSsl3Bignum modulusBefore = *n;
    const LibreSsl3Bignum exponentBefore = *e;

    LibreSsl3Rsa* copy = RSAPublicKey_dup_nid_postfix(source);
    Require(copy != nullptr);
    Require(copy != source);
    Require(copy->version == 0);
    Require(copy->references == 1);
    Require(copy->method == nullptr && copy->pss == nullptr && copy->extraData == nullptr);
    Require(copy->flags == 0);
    Require(copy->methodModN == nullptr && copy->methodModP == nullptr && copy->methodModQ == nullptr);
    Require(copy->blinding == nullptr && copy->mtBlinding == nullptr);
    Require(copy->n != nullptr && copy->e != nullptr);
    Require(copy->n != source->n && copy->e != source->e);
    Require(copy->d == nullptr && copy->p == nullptr && copy->q == nullptr);
    Require(copy->dmp1 == nullptr && copy->dmq1 == nullptr && copy->iqmp == nullptr);

    auto* copiedModulus = static_cast<LibreSsl3Bignum*>(copy->n);
    auto* copiedExponent = static_cast<LibreSsl3Bignum*>(copy->e);
    Require(copiedModulus != n && copiedExponent != e);
    Require(copiedModulus->data != n->data && copiedExponent->data != e->data);
    Require(copiedModulus->top == n->top && copiedModulus->capacity == n->top && copiedModulus->negative == 0);
    Require(copiedExponent->top == e->top && copiedExponent->capacity == e->top && copiedExponent->negative == 0);
    for (int i = 0; i < n->top; ++i) Require(copiedModulus->data[i] == modulusWords[i]);
    Require(copiedExponent->data[0] == 65537);

    Require(std::memcmp(source, &sourceBefore, sizeof(sourceBefore)) == 0);
    Require(std::memcmp(n, &modulusBefore, sizeof(modulusBefore)) == 0);
    Require(std::memcmp(e, &exponentBefore, sizeof(exponentBefore)) == 0);
    for (int i = 0; i < n->top; ++i) Require(n->data[i] == modulusWords[i]);

    BN_clear_free_nid_postfix(copiedModulus);
    BN_clear_free_nid_postfix(copiedExponent);
    copy->n = nullptr;
    copy->e = nullptr;
    free_nid_postfix(copy);
    BN_clear_free_nid_postfix(n);
    BN_clear_free_nid_postfix(e);
    source->n = nullptr;
    source->e = nullptr;
    free_nid_postfix(source);
    Require(liveBlocks == 0);

    std::puts("libSceLibreSsl3 RSAPublicKey_dup tests passed");
}
