#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

void* APS5_VABI Allocate(std::size_t size) { return std::malloc(size ? size : 1); }
void APS5_VABI Release(void* pointer) { std::free(pointer); }
void* APS5_VABI Callocate(std::size_t count, std::size_t size) { return std::calloc(count ? count : 1, size ? size : 1); }
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Align(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign(void*, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void RegisterHeap() {
    std::array<void*, 7> api{reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Release),
        reinterpret_cast<void*>(Callocate), reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(Align),
        reinterpret_cast<void*>(Realign), reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());
}

void Run() {
    LibreSslEvpCipherCtx context{};
    context.flags = 0x1;
    Require(EVP_CIPHER_CTX_set_flags_nid_postfix(&context, 0x40) == 1, "flags are set");
    Require(context.flags == 0x41, "the existing flags are kept");
    Require(EVP_CIPHER_CTX_set_flags_nid_postfix(&context, 0x80) == 1, "more flags are set");
    Require(context.flags == 0xC1, "the flags accumulate");
    Require(EVP_CIPHER_CTX_set_flags_nid_postfix(&context, -1) == 1, "a negative mask is accepted");
    Require(context.flags == UINT64_MAX, "a negative mask sets every bit");
    bool threw = false;
    try {
        EVP_CIPHER_CTX_set_flags_nid_postfix(nullptr, 1);
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw, "a null context throws");
}

}

int main() {
    RegisterHeap();
    try {
        Run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "EVP_CIPHER_CTX_set_flags threw: %s\n", error.what());
        return 1;
    }
    std::puts("libSceLibreSsl3 EVP_CIPHER_CTX_set_flags tests passed");
    return 0;
}
