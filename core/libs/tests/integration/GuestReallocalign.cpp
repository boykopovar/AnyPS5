#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstring>
#include <stdexcept>

extern "C" {
void* APS5_VABI reallocalign_nid_postfix(void*, std::size_t, std::size_t);
}

namespace {

void Require(bool value) {
    if (!value) throw std::runtime_error("reallocalign check failed");
}

template<typename TAction>
void Reject(TAction action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    Require(rejected);
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
    Reject([&] { reallocalign_nid_postfix(shrunk, 16, 0); });
    Reject([&] { reallocalign_nid_postfix(shrunk, 16, 3); });
    Require(reallocalign_nid_postfix(shrunk, 0, 16) == nullptr);
}
