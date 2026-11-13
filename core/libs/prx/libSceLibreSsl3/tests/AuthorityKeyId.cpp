#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "AUTHORITY_KEYID_free check failed at line %u\n", location.line());
    std::abort();
}

int liveBlocks;

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

void* APS5_VABI Reallocate(void* block, std::size_t bytes) {
    if (!block) ++liveBlocks;
    return std::realloc(block, bytes ? bytes : 1);
}
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

struct AuthorityKeyId {
    Asn1String* keyId;
    LibreSslStack* issuer;
    Asn1String* serial;
};

Asn1String* String(unsigned char value) {
    auto* string = static_cast<Asn1String*>(calloc_nid_postfix(1, sizeof(Asn1String)));
    Require(string != nullptr);
    string->data = static_cast<unsigned char*>(malloc_nid_postfix(1));
    Require(string->data != nullptr);
    string->data[0] = value;
    string->length = 1;
    return string;
}

}

int main() {
    InitializeHeap();
    AUTHORITY_KEYID_free_nid_postfix(nullptr);
    Require(liveBlocks == 0);

    auto* id = static_cast<AuthorityKeyId*>(calloc_nid_postfix(1, sizeof(AuthorityKeyId)));
    Require(id != nullptr);
    id->keyId = String(0x12);
    id->issuer = sk_new_nid_postfix(nullptr);
    Require(id->issuer != nullptr);
    id->serial = String(0x34);
    AUTHORITY_KEYID_free_nid_postfix(id);
    Require(liveBlocks == 0);

    std::puts("AUTHORITY_KEYID_free tests passed");
}
