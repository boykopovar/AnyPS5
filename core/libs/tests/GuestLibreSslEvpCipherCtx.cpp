#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

constexpr std::size_t ContextBytes = 168;
constexpr std::size_t FlagsOffset = 112;

static std::uint64_t Flags(const unsigned char* context) {
    std::uint64_t flags = 0;
    std::memcpy(&flags, context + FlagsOffset, sizeof(flags));
    return flags;
}

static void SetFlags(unsigned char* context, std::uint64_t flags) {
    std::memcpy(context + FlagsOffset, &flags, sizeof(flags));
}

static void InitializeHeap() {
    void* const api[10] {};
    ApplicationHeapRegister_nid_no_patch(api);
}

extern "C" {
void APS5_VABI EVP_CIPHER_CTX_init_nid_postfix(void*);
void* APS5_VABI EVP_CIPHER_CTX_new_nid_postfix(void);
void APS5_VABI free_nid_postfix(void*);
int APS5_VABI EVP_CIPHER_CTX_set_padding_nid_postfix(void*, int);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL EVP_CIPHER_CTX check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static void TestInit() {
    unsigned char memory[ContextBytes + 16];
    std::memset(memory, 0xAA, sizeof(memory));
    EVP_CIPHER_CTX_init_nid_postfix(memory);
    for (std::size_t index = 0; index < ContextBytes; ++index) Require(memory[index] == 0);
    for (std::size_t index = ContextBytes; index < sizeof(memory); ++index) Require(memory[index] == 0xAA);
    RequireThrows([] { EVP_CIPHER_CTX_init_nid_postfix(nullptr); });
}

static void TestNew() {
    for (int round = 0; round < 4; ++round) {
        auto* context = static_cast<unsigned char*>(EVP_CIPHER_CTX_new_nid_postfix());
        Require(context != nullptr);
        for (std::size_t index = 0; index < ContextBytes; ++index) Require(context[index] == 0);
        std::memset(context, 0x5A, ContextBytes);
        free_nid_postfix(context);
    }
}

static void TestSetPadding() {
    unsigned char context[ContextBytes] {};
    Require(EVP_CIPHER_CTX_set_padding_nid_postfix(context, 0) == 1);
    Require(Flags(context) == 0x100);
    Require(EVP_CIPHER_CTX_set_padding_nid_postfix(context, 0) == 1);
    Require(Flags(context) == 0x100);
    Require(EVP_CIPHER_CTX_set_padding_nid_postfix(context, 1) == 1);
    Require(Flags(context) == 0);
    SetFlags(context, UINT64_C(0xFFFFFFFF000000FF));
    Require(EVP_CIPHER_CTX_set_padding_nid_postfix(context, 0) == 1);
    Require(Flags(context) == UINT64_C(0xFFFFFFFF000001FF));
    Require(EVP_CIPHER_CTX_set_padding_nid_postfix(context, -5) == 1);
    Require(Flags(context) == UINT64_C(0xFFFFFFFF000000FF));
    for (std::size_t index = 0; index < ContextBytes; ++index) {
        if (index < FlagsOffset || index >= FlagsOffset + 8) Require(context[index] == 0);
    }
    RequireThrows([] { EVP_CIPHER_CTX_set_padding_nid_postfix(nullptr, 0); });
}

int main() {
    InitializeHeap();
    TestInit();
    TestNew();
    TestSetPadding();
    return 0;
}
