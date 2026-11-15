#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "prx/libSceVideoOut/include/Event.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "SceTypes.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

constexpr int SystemUser = 255;
constexpr int MainBus = 0;
constexpr int InvalidHandle = -1;
constexpr int NegativeHandle = -100;
constexpr int ZeroHandle = 0;
constexpr int NeverOpenedHandle = 2;
constexpr int OutOfRangeHandle = 9999;

constexpr int InvalidHandleError = static_cast<int>(0x8029000Bu);
constexpr int InvalidEventError = static_cast<int>(0x8029000Du);
constexpr int InvalidAddressError = static_cast<int>(0x80290002u);

constexpr int VideoOutEventPreVblankStart = 2;
constexpr std::int16_t ForeignFilterUser = 1;

void Require(bool value) {
    if (!value) std::abort();
}

template <typename Fn>
bool Throws(Fn&& fn) {
    try {
        fn();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

} // namespace

int main() {
    VideoOutTestEnvironment environment;

    int handle = 0;
    try {
        handle = sceVideoOutOpen(SystemUser, MainBus, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(handle > 0);

    for (const int unopenedHandle : {InvalidHandle, NegativeHandle, ZeroHandle, NeverOpenedHandle, OutOfRangeHandle}) {
        Require(sceVideoOutAddPreVblankStartEvent(0, unopenedHandle, nullptr) == InvalidHandleError);
        Require(sceVideoOutDeletePreVblankStartEvent(0, unopenedHandle) == InvalidHandleError);
    }

    KernelEqueue unopenedProbeEq = 0;
    Require(sceKernelCreateEqueue(&unopenedProbeEq, "unopened_probe_queue") == 0);
    Require(unopenedProbeEq > 0);

    for (const int unopenedHandle : {InvalidHandle, NegativeHandle, ZeroHandle, NeverOpenedHandle, OutOfRangeHandle}) {
        Require(sceVideoOutAddPreVblankStartEvent(unopenedProbeEq, unopenedHandle, nullptr) == InvalidHandleError);
        Require(sceVideoOutDeletePreVblankStartEvent(unopenedProbeEq, unopenedHandle) == InvalidHandleError);
    }
    Require(sceKernelDeleteEqueue(unopenedProbeEq) == 0);

    KernelEvent syntheticPreVblank{};
    syntheticPreVblank.filter = EVFILT_VIDEO_OUT;
    syntheticPreVblank.ident = VideoOutEventPreVblankStart;
    syntheticPreVblank.data = static_cast<intptr_t>((0x12345ULL << 16u) | (7ULL << 12u) | 0xabcULL);

    Require(sceVideoOutGetEventId(&syntheticPreVblank) == VideoOutEventPreVblankStart);
    std::int64_t syntheticData = 0;
    Require(sceVideoOutGetEventData(&syntheticPreVblank, &syntheticData) == 0);
    Require(syntheticData == 0x12345);
    Require(sceVideoOutGetEventCount(&syntheticPreVblank) == 7);

    Require(sceVideoOutGetEventId(nullptr) == InvalidAddressError);
    Require(sceVideoOutGetEventData(nullptr, nullptr) == InvalidAddressError);
    Require(sceVideoOutGetEventData(nullptr, &syntheticData) == InvalidAddressError);
    Require(sceVideoOutGetEventData(&syntheticPreVblank, nullptr) == InvalidAddressError);
    Require(Throws([] { return sceVideoOutGetEventCount(nullptr); }));

    KernelEvent foreignEvent{};
    foreignEvent.filter = ForeignFilterUser;
    Require(sceVideoOutGetEventId(&foreignEvent) == InvalidEventError);
    Require(sceVideoOutGetEventData(&foreignEvent, &syntheticData) == InvalidEventError);

    KernelEvent unknownIdentEvent{};
    unknownIdentEvent.filter = EVFILT_VIDEO_OUT;
    unknownIdentEvent.ident = 99;
    Require(sceVideoOutGetEventId(&unknownIdentEvent) == InvalidEventError);

    for (const KernelEqueue badEq : {static_cast<KernelEqueue>(0), static_cast<KernelEqueue>(-1), static_cast<KernelEqueue>(1234567)}) {
        Require(Throws([&] { return sceVideoOutAddPreVblankStartEvent(badEq, handle, nullptr); }));
        Require(Throws([&] { return sceVideoOutDeletePreVblankStartEvent(badEq, handle); }));
    }

    KernelEqueue eq1 = 0;
    Require(sceKernelCreateEqueue(&eq1, "prevblank_eq1") == 0);
    Require(eq1 > 0);

    Require(sceVideoOutDeletePreVblankStartEvent(eq1, handle) == 0);

    int marker1 = 111;
    Require(sceVideoOutAddPreVblankStartEvent(eq1, handle, &marker1) == 0);

    int marker2 = 222;
    Require(sceVideoOutAddPreVblankStartEvent(eq1, handle, &marker2) == 0);

    KernelEqueue eq2 = 0;
    Require(sceKernelCreateEqueue(&eq2, "prevblank_eq2") == 0);
    Require(eq2 > 0);

    int marker3 = 333;
    Require(sceVideoOutAddPreVblankStartEvent(eq2, handle, &marker3) == 0);

    Require(sceVideoOutWaitVblank(handle) == 0);

    std::array<KernelEvent, 8> events1{};
    int count1 = 0;
    const KernelUseconds zeroTimeout = 0;
    Require(sceKernelWaitEqueue(eq1, events1.data(), static_cast<int>(events1.size()), &count1, &zeroTimeout) == 0);
    Require(count1 >= 1);

    bool eq1Found = false;
    std::int64_t data1 = 0;
    for (int i = 0; i < count1; ++i) {
        if (sceVideoOutGetEventId(&events1[i]) == VideoOutEventPreVblankStart) {
            eq1Found = true;
            Require(events1[i].udata == &marker2);
            Require(sceVideoOutGetEventData(&events1[i], &data1) == 0);
            Require(data1 >= 1);
            const int eventCount = sceVideoOutGetEventCount(&events1[i]);
            Require(eventCount >= 1 && eventCount <= 15);
        }
    }
    Require(eq1Found);

    std::array<KernelEvent, 8> events2{};
    int count2 = 0;
    Require(sceKernelWaitEqueue(eq2, events2.data(), static_cast<int>(events2.size()), &count2, &zeroTimeout) == 0);
    Require(count2 >= 1);

    bool eq2Found = false;
    std::int64_t data2 = 0;
    for (int i = 0; i < count2; ++i) {
        if (sceVideoOutGetEventId(&events2[i]) == VideoOutEventPreVblankStart) {
            eq2Found = true;
            Require(events2[i].udata == &marker3);
            Require(sceVideoOutGetEventData(&events2[i], &data2) == 0);
            Require(data2 >= 1);
        }
    }
    Require(eq2Found);

    Require(sceVideoOutDeletePreVblankStartEvent(eq1, handle) == 0);
    Require(sceVideoOutDeletePreVblankStartEvent(eq1, handle) == 0);

    Require(sceVideoOutWaitVblank(handle) == 0);

    int count1After = 0;
    Require(sceKernelWaitEqueue(eq1, events1.data(), static_cast<int>(events1.size()), &count1After, &zeroTimeout) == 0);
    for (int i = 0; i < count1After; ++i) {
        Require(sceVideoOutGetEventId(&events1[i]) != VideoOutEventPreVblankStart);
    }

    int count2After = 0;
    Require(sceKernelWaitEqueue(eq2, events2.data(), static_cast<int>(events2.size()), &count2After, &zeroTimeout) == 0);
    Require(count2After >= 1);

    bool eq2StillFound = false;
    std::int64_t data2After = 0;
    for (int i = 0; i < count2After; ++i) {
        if (sceVideoOutGetEventId(&events2[i]) == VideoOutEventPreVblankStart) {
            eq2StillFound = true;
            Require(events2[i].udata == &marker3);
            Require(sceVideoOutGetEventData(&events2[i], &data2After) == 0);
            Require(data2After > data2);
        }
    }
    Require(eq2StillFound);

    Require(sceVideoOutDeletePreVblankStartEvent(eq2, handle) == 0);

    Require(sceKernelDeleteEqueue(eq1) == 0);
    Require(Throws([&] { return sceVideoOutAddPreVblankStartEvent(eq1, handle, nullptr); }));
    Require(Throws([&] { return sceVideoOutDeletePreVblankStartEvent(eq1, handle); }));

    Require(sceVideoOutClose(handle) == 0);

    for (const KernelEqueue eq : {eq2, static_cast<KernelEqueue>(0)}) {
        Require(sceVideoOutAddPreVblankStartEvent(eq, handle, nullptr) == InvalidHandleError);
        Require(sceVideoOutDeletePreVblankStartEvent(eq, handle) == InvalidHandleError);
    }

    Require(sceKernelDeleteEqueue(eq2) == 0);

    return 0;
}
