#include "prx/libSceAgcDriver/State/include/Configuration.hpp"
#include "prx/libSceAgcDriver/include/AgcDriver.hpp"
#include "SceTypes.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <thread>

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

struct OffchipOutput {
    uint16_t granularity = 0xffff;
    uint16_t buffering = 0xffff;
    uint32_t canary = Canary;
};

void RequireOffchip(uint16_t expectedGranularity, uint16_t expectedBuffering) {
    OffchipOutput output;
    Require(sceAgcDriverGetHsOffchipParam(&output.granularity, &output.buffering) == 0);
    Require(output.granularity == expectedGranularity && output.buffering == expectedBuffering && output.canary == Canary);
}

void CheckHsOffchipParam() {
    RequireOffchip(0, 0);
    Require(sceAgcDriverSetHsOffchipParam(2, 0x100) == 0);
    RequireOffchip(2, 0x100);
    Require(sceAgcDriverSetHsOffchipParam(3, 0x1ff) == 0);
    RequireOffchip(3, 0x1ff);
    Require(sceAgcDriverSetHsOffchipParam(1, 0x3ff) == 0);
    RequireOffchip(1, 0x1ff);
    std::thread([] { RequireOffchip(1, 0x1ff); }).join();
    Require(sceAgcDriverSetHsOffchipParam(0, 0) == 0);
    RequireOffchip(0, 0);
    OffchipOutput output;
    Require(Rejects([&] { sceAgcDriverGetHsOffchipParam(nullptr, &output.buffering); }));
    Require(Rejects([&] { sceAgcDriverGetHsOffchipParam(&output.granularity, nullptr); }));
    errno = 71;
    Require(Rejects([&] { sceAgcDriverGetHsOffchipParam(reinterpret_cast<uint16_t*>(0x10), &output.buffering); }));
    Require(errno == 71);
    Require(Rejects([&] { sceAgcDriverGetHsOffchipParam(&output.granularity, reinterpret_cast<uint16_t*>(0x10)); }));
    Require(errno == 71);
}

void RequireRing(uintptr_t expectedBase, uint32_t expectedSize) {
    RingOutput output;
    Require(sceAgcDriverGetTFRing(&output.base, &output.size) == 0);
    Require(output.base == expectedBase && output.size == expectedSize && output.canary == Canary);
}

}

int main() {
    CheckHsOffchipParam();
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
    errno = 71;
    Require(Rejects([&] { sceAgcDriverGetTFRing(reinterpret_cast<uintptr_t*>(0x10), &output.size); }));
    Require(errno == 71);
    Require(Rejects([&] { sceAgcDriverGetTFRing(&output.base, reinterpret_cast<uint32_t*>(0x10)); }));
    Require(errno == 71);
}
