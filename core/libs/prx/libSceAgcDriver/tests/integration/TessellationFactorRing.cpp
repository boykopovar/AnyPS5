#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <thread>

extern "C" {
int APS5_VABI sceAgcDriverSetTFRing(const volatile void* base, uint32_t size);
int APS5_VABI sceAgcDriverGetTFRing(uintptr_t* base, uint32_t* size);
}

namespace {

constexpr uint32_t Canary = 0xa5a5a5a5;

struct RingOutput {
    uintptr_t base = ~uintptr_t{0};
    uint32_t size = 0;
    uint32_t canary = Canary;
};

void Require(bool value) {
    if (!value) std::abort();
}

template<typename TAction>
bool Rejects(TAction action) {
    try { action(); }
    catch (const std::invalid_argument&) { return true; }
    return false;
}

void RequireRing(uintptr_t expectedBase, uint32_t expectedSize) {
    RingOutput output;
    Require(sceAgcDriverGetTFRing(&output.base, &output.size) == 0);
    Require(output.base == expectedBase && output.size == expectedSize && output.canary == Canary);
}

}

int main() {
    alignas(256) static uint8_t ring[0x20000];
    const auto ringBase = reinterpret_cast<uintptr_t>(ring);
    RequireRing(0xff0000000, 0x20000);
    Require(sceAgcDriverSetTFRing(ring, 0x1b000) == 0);
    RequireRing(ringBase, 0x1b000);
    std::thread([&] { RequireRing(ringBase, 0x1b000); }).join();
    std::thread([] { Require(sceAgcDriverSetTFRing(ring + 0x1000, 0x2000) == 0); }).join();
    RequireRing(ringBase + 0x1000, 0x2000);
    Require(Rejects([] { sceAgcDriverSetTFRing(nullptr, 0x2000); }));
    Require(Rejects([] { sceAgcDriverSetTFRing(ring, 0); }));
    RequireRing(ringBase + 0x1000, 0x2000);
    RingOutput output;
    Require(Rejects([&] { sceAgcDriverGetTFRing(nullptr, &output.size); }));
    Require(Rejects([&] { sceAgcDriverGetTFRing(&output.base, nullptr); }));
}
