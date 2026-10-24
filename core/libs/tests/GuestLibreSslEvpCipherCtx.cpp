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

int main() {
    TestInit();
    return 0;
}
