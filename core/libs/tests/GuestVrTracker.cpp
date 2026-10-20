#include "prx/libSceVrTracker/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

constexpr int NotInit = static_cast<int>(0x81260801u);
constexpr std::uint8_t Fill = 0xAA;
constexpr std::uint64_t FilledTime = 0xAAAAAAAAAAAAAAAAull;

using Buffer = std::array<std::uint8_t, 0x200>;

std::uint8_t g_onion[16];
std::uint8_t g_garlic[16];
std::uint8_t g_work[16];

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "VrTracker: %s\n", message);
        std::abort();
    }
}

bool Untouched(const Buffer& buffer) {
    for (std::uint8_t byte : buffer)
        if (byte != Fill) return false;
    return true;
}

template <typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

const VrTrackerGpuSubmitParam* SubmitParam(const Buffer& buffer) {
    return reinterpret_cast<const VrTrackerGpuSubmitParam*>(buffer.data());
}

const VrTrackerGpuWaitParam* WaitParam(const Buffer& buffer) {
    return reinterpret_cast<const VrTrackerGpuWaitParam*>(buffer.data());
}

VrTrackerLegacy::InitParam ValidInit() {
    VrTrackerLegacy::InitParam init{};
    init.size = sizeof(init);
    init.direct_memory_onion_size = 0x400000;
    init.direct_memory_onion_alignment = 0x10000;
    init.direct_memory_garlic_size = 0x3000000;
    init.direct_memory_garlic_alignment = 0x10000;
    init.work_memory_size = 0x1000000;
    init.work_memory_alignment = 0x10000;
    init.direct_memory_onion = g_onion;
    init.direct_memory_garlic = g_garlic;
    init.work_memory = g_work;
    return init;
}

void CheckNotInit() {
    Require(sceVrTrackerGetTime(nullptr) == NotInit, "time null");
    std::uint64_t time = FilledTime;
    Require(sceVrTrackerGetTime(&time) == NotInit, "time return");
    Require(time == FilledTime, "time untouched");

    Buffer param{};
    param.fill(Fill);
    Require(sceVrTrackerGpuSubmit(nullptr) == NotInit, "submit null");
    Require(sceVrTrackerGpuSubmit(SubmitParam(param)) == NotInit, "submit return");
    Require(Untouched(param), "submit leaves param");
    Require(sceVrTrackerGpuWait(nullptr) == NotInit, "wait null");
    Require(sceVrTrackerGpuWait(WaitParam(param)) == NotInit, "wait return");
    Require(Untouched(param), "wait leaves param");
}

void CheckInitialized() {
    std::uint64_t time = FilledTime;
    Require(Throws([&] { sceVrTrackerGetTime(&time); }), "time throws once initialized");
    Require(time == FilledTime, "time untouched once initialized");

    Buffer param{};
    param.fill(Fill);
    Require(Throws([&] { sceVrTrackerGpuSubmit(SubmitParam(param)); }), "submit throws once initialized");
    Require(Throws([&] { sceVrTrackerGpuWait(WaitParam(param)); }), "wait throws once initialized");
    Require(Untouched(param), "param untouched once initialized");
}

}

int main() {
    CheckNotInit();

    const auto init = ValidInit();
    Require(sceVrTrackerInit(reinterpret_cast<const VrTrackerInitParam*>(&init)) == 0, "init");
    CheckInitialized();

    Require(sceVrTrackerTerm() == 0, "term");
    CheckNotInit();
    return 0;
}
