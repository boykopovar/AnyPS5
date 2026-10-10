#include <Testing/Test.hpp>
#include "SceTypes.hpp"

#include <cstdint>
#include <stdexcept>
#include <thread>

extern "C" {
int APS5_VABI sceAgcDriverSetTFRing(const volatile void* base, uint32_t size);
int APS5_VABI sceAgcDriverGetTFRing(uintptr_t* base, uint32_t* size);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr uint32_t Canary = 0xa5a5a5a5;

alignas(256) uint8_t ring[0x20000];

struct RingOutput {
    uintptr_t base = ~uintptr_t{0};
    uint32_t size = 0;
    uint32_t canary = Canary;
};

uintptr_t RingBase() {
    return reinterpret_cast<uintptr_t>(ring);
}

RingOutput CurrentRing() {
    RingOutput output;
    RequireEqual(sceAgcDriverGetTFRing(&output.base, &output.size), 0, "sceAgcDriverGetTFRing result");
    RequireEqual(output.canary, Canary, "sceAgcDriverGetTFRing wrote past the size output");
    return output;
}

void RequireRing(uintptr_t expectedBase, uint32_t expectedSize) {
    const auto output = CurrentRing();
    RequireEqual(output.base, expectedBase, "tessellation factor ring base");
    RequireEqual(output.size, expectedSize, "tessellation factor ring size");
}

void SetRing(const volatile void* base, uint32_t size) {
    RequireEqual(sceAgcDriverSetTFRing(base, size), 0, "sceAgcDriverSetTFRing result");
}

const Case defaultRing{"GetTFRing_BeforeAnySet_ReturnsTheDefaultRing", [] {
    RequireRing(0xff0000000, 0x20000);
}};

const Case setRing{"SetTFRing_ValidRing_IsReturnedByGetTFRing", [] {
    SetRing(ring, 0x1b000);
    RequireRing(RingBase(), 0x1b000);
}};

const Case otherThreadReads{"GetTFRing_OnAnotherThread_SeesTheRingSetByThisThread", [] {
    SetRing(ring, 0x1b000);
    RingOutput seen;
    std::thread([&] { sceAgcDriverGetTFRing(&seen.base, &seen.size); }).join();
    RequireEqual(seen.base, RingBase(), "ring base seen by another thread");
    RequireEqual(seen.size, 0x1b000u, "ring size seen by another thread");
    RequireEqual(seen.canary, Canary, "sceAgcDriverGetTFRing wrote past the size output on another thread");
}};

const Case otherThreadWrites{"SetTFRing_OnAnotherThread_IsSeenByThisThread", [] {
    SetRing(ring, 0x1b000);
    int result = -1;
    std::thread([&] { result = sceAgcDriverSetTFRing(ring + 0x1000, 0x2000); }).join();
    RequireEqual(result, 0, "sceAgcDriverSetTFRing result on another thread");
    RequireRing(RingBase() + 0x1000, 0x2000);
}};

const Case invalidRing{"SetTFRing_NullBaseOrZeroSize_ThrowsAndKeepsTheRing", [] {
    SetRing(ring + 0x1000, 0x2000);
    Testing::RequireThrows<std::invalid_argument>([] { sceAgcDriverSetTFRing(nullptr, 0x2000); }, "a null tessellation factor ring was accepted");
    Testing::RequireThrows<std::invalid_argument>([] { sceAgcDriverSetTFRing(ring, 0); }, "an empty tessellation factor ring was accepted");
    RequireRing(RingBase() + 0x1000, 0x2000);
}};

const Case nullOutputs{"GetTFRing_NullOutput_Throws", [] {
    RingOutput output;
    Testing::RequireThrows<std::invalid_argument>([&] { sceAgcDriverGetTFRing(nullptr, &output.size); }, "a null base output was accepted");
    Testing::RequireThrows<std::invalid_argument>([&] { sceAgcDriverGetTFRing(&output.base, nullptr); }, "a null size output was accepted");
    Require(output.canary == Canary, "a rejected query wrote past the size output");
}};

} // namespace
