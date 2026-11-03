#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 BN_dup check failed at line %u\n", location.line());
    std::abort();
}

int liveBlocks = 0;

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

Bignum* FromHex(const char* hex) {
    Bignum* value = nullptr;
    Require(BN_hex2bn_nid_postfix(&value, hex) > 0);
    return value;
}

std::vector<unsigned char> Bytes(const Bignum* value) {
    std::vector<unsigned char> bytes((BN_num_bits_nid_postfix(value) + 7) / 8);
    Require(BN_bn2bin_nid_postfix(value, bytes.data()) == static_cast<int>(bytes.size()));
    return bytes;
}

}

int main() {
    InitializeHeap();
    Require(BN_dup_nid_postfix(nullptr) == nullptr);

    Bignum* source = FromHex("-1234567890ABCDEF0FEDCBA0987654321");
    Bignum* copy = BN_dup_nid_postfix(source);
    Require(copy != nullptr && copy != source && copy->d != source->d);
    Require(copy->top == source->top && copy->dmax == source->top && copy->neg == 1);
    for (int i = 0; i < source->top; ++i) Require(copy->d[i] == source->d[i]);
    Require(Bytes(copy) == Bytes(source));
    Require(BN_add_word_nid_postfix(copy, 1) == 1);
    Require(Bytes(copy) != Bytes(source));
    BN_free_nid_postfix(copy);

    source->flags |= 0x04;
    copy = BN_dup_nid_postfix(source);
    Require((copy->flags & 0x04) != 0);
    BN_free_nid_postfix(copy);
    BN_free_nid_postfix(source);

    Bignum* zero = BN_new_nid_postfix();
    Bignum* zeroCopy = BN_dup_nid_postfix(zero);
    Require(zeroCopy != nullptr && zeroCopy->top == 0 && zeroCopy->neg == 0 && zeroCopy->d == nullptr && zeroCopy->dmax == 0);
    Require((zeroCopy->flags & 0x04) == 0);
    BN_free_nid_postfix(zeroCopy);
    BN_free_nid_postfix(zero);
    Require(liveBlocks == 0);
}
