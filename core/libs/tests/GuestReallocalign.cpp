#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

extern "C" {
void* APS5_VABI reallocalign_nid_postfix(void*, std::size_t, std::size_t);
std::size_t APS5_VABI malloc_usable_size_nid_postfix(void*);
void APS5_VABI free_nid_postfix(void*);
int* APS5_VABI __error_nid_postfix();
}

namespace {

void Require(bool value) {
    if (!value) throw std::runtime_error("reallocalign check failed");
}

}

int main() {
    void* api[10]{};
    ApplicationHeapRegister_nid_no_patch(api);
    auto* block = static_cast<unsigned char*>(reallocalign_nid_postfix(nullptr, 64, 64));
    Require(block != nullptr && (reinterpret_cast<std::uintptr_t>(block) % 64) == 0);
    std::memset(block, 0x5a, 64);
    auto* grown = static_cast<unsigned char*>(reallocalign_nid_postfix(block, 256, 64));
    Require(grown != nullptr && (reinterpret_cast<std::uintptr_t>(grown) % 64) == 0);
    for (int index = 0; index < 64; ++index) Require(grown[index] == 0x5a);
    auto* shrunk = static_cast<unsigned char*>(reallocalign_nid_postfix(grown, 16, 16));
    Require(shrunk != nullptr && (reinterpret_cast<std::uintptr_t>(shrunk) % 16) == 0);
    for (int index = 0; index < 16; ++index) Require(shrunk[index] == 0x5a);
    shrunk = static_cast<unsigned char*>(reallocalign_nid_postfix(shrunk, 16, 0));
    Require(shrunk != nullptr && (reinterpret_cast<std::uintptr_t>(shrunk) % 32) == 0);
    for (int index = 0; index < 16; ++index) Require(shrunk[index] == 0x5a);
    shrunk = static_cast<unsigned char*>(reallocalign_nid_postfix(shrunk, 40, 100));
    Require(shrunk != nullptr && (reinterpret_cast<std::uintptr_t>(shrunk) % 128) == 0);
    for (int index = 0; index < 16; ++index) Require(shrunk[index] == 0x5a);
    constexpr std::size_t max = std::numeric_limits<std::size_t>::max();
    *__error_nid_postfix() = 77;
    Require(reallocalign_nid_postfix(shrunk, max, 16) == nullptr && *__error_nid_postfix() == 77);
    Require(reallocalign_nid_postfix(shrunk, max - 0x1008f, 16) == nullptr && *__error_nid_postfix() == 22);
    *__error_nid_postfix() = 77;
    Require(reallocalign_nid_postfix(shrunk, max - 0x110ae, 4096) == nullptr && *__error_nid_postfix() == 77);
    Require(reallocalign_nid_postfix(shrunk, max - 0x110af, 4096) == nullptr && *__error_nid_postfix() == 22);
    *__error_nid_postfix() = 77;
    Require(reallocalign_nid_postfix(shrunk, 16, std::size_t{1} << 63) == nullptr && *__error_nid_postfix() == 77);
    for (int index = 0; index < 16; ++index) Require(shrunk[index] == 0x5a);
    Require(reallocalign_nid_postfix(shrunk, 0, 16) == shrunk && malloc_usable_size_nid_postfix(shrunk) == 0);
    free_nid_postfix(shrunk);
    void* empty = reallocalign_nid_postfix(nullptr, 0, 0);
    Require(empty != nullptr);
    free_nid_postfix(empty);
    return 0;
}
