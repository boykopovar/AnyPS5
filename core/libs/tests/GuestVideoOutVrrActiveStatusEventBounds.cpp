#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/Event.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;
static constexpr int VIDEO_OUT_ERROR_INVALID_HANDLE = static_cast<int>(0x8029000Bu);
static constexpr int VIDEO_OUT_ERROR_INVALID_EVENT = static_cast<int>(0x8029000Du);
static constexpr int VIDEO_OUT_EVENT_VRR_STATUS = 16;

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

static bool ThrowsAddEvent(KernelEqueue eq, int handle, void* udata) {
    try {
        sceVideoOutAddVrrActiveStatusEvent(eq, handle, udata);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool ThrowsDeleteEvent(KernelEqueue eq, int handle) {
    try {
        sceVideoOutDeleteVrrActiveStatusEvent(eq, handle);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int main() {
    VideoOutTestEnvironment environment;

    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "vrr_test_queue") == 0);
    Require(eq > 0);

    int handle = 0;
    try {
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error& error) {
        sceKernelDeleteEqueue(eq);
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(handle > 0);

    const int unopenedHandles[] = {-1, 0, handle + 1, handle + 10, -9999, 0x7FFFFFFF};
    int dummyUdata = 42;
    for (const int unopened : unopenedHandles) {
        Require(sceVideoOutAddVrrActiveStatusEvent(eq, unopened, nullptr) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutAddVrrActiveStatusEvent(eq, unopened, &dummyUdata) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutDeleteVrrActiveStatusEvent(eq, unopened) == VIDEO_OUT_ERROR_INVALID_HANDLE);

        Require(sceVideoOutAddVrrActiveStatusEvent(0, unopened, nullptr) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutAddVrrActiveStatusEvent(0, unopened, &dummyUdata) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutDeleteVrrActiveStatusEvent(0, unopened) == VIDEO_OUT_ERROR_INVALID_HANDLE);

        Require(sceVideoOutAddVrrActiveStatusEvent(0x7FFFFFFF, unopened, nullptr) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutDeleteVrrActiveStatusEvent(0x7FFFFFFF, unopened) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    }

    Require(ThrowsAddEvent(0, handle, nullptr));
    Require(ThrowsAddEvent(0, handle, &dummyUdata));
    Require(ThrowsDeleteEvent(0, handle));

    Require(ThrowsAddEvent(0x7FFFFFFF, handle, nullptr));
    Require(ThrowsDeleteEvent(0x7FFFFFFF, handle));

    int udataA = 100;
    Require(sceVideoOutAddVrrActiveStatusEvent(eq, handle, &udataA) == 0);

    int udataB = 200;
    Require(sceVideoOutAddVrrActiveStatusEvent(eq, handle, &udataB) == 0);
    Require(sceVideoOutAddVrrActiveStatusEvent(eq, handle, nullptr) == 0);

    KernelEqueue eq2 = 0;
    Require(sceKernelCreateEqueue(&eq2, "vrr_test_queue_2") == 0);
    Require(eq2 > 0);
    Require(sceVideoOutAddVrrActiveStatusEvent(eq2, handle, &udataA) == 0);
    Require(sceVideoOutDeleteVrrActiveStatusEvent(eq2, handle) == 0);
    Require(sceVideoOutDeleteVrrActiveStatusEvent(eq2, handle) == 0);
    Require(sceKernelDeleteEqueue(eq2) == 0);

    Require(sceVideoOutDeleteVrrActiveStatusEvent(eq, handle) == 0);
    Require(sceVideoOutDeleteVrrActiveStatusEvent(eq, handle) == 0);

    Require(sceVideoOutAddVrrActiveStatusEvent(eq, handle, &udataA) == 0);
    Require(sceVideoOutDeleteVrrActiveStatusEvent(eq, handle) == 0);

    KernelEvent vrrEvent{};
    vrrEvent.ident = VIDEO_OUT_EVENT_VRR_STATUS;
    vrrEvent.filter = EVFILT_VIDEO_OUT;
    Require(sceVideoOutGetEventId(&vrrEvent) == VIDEO_OUT_EVENT_VRR_STATUS);

    KernelEvent nonVrrEvent{};
    nonVrrEvent.ident = 999;
    nonVrrEvent.filter = EVFILT_VIDEO_OUT;
    Require(sceVideoOutGetEventId(&nonVrrEvent) == VIDEO_OUT_ERROR_INVALID_EVENT);

    KernelEvent otherFilterEvent{};
    otherFilterEvent.ident = VIDEO_OUT_EVENT_VRR_STATUS;
    otherFilterEvent.filter = EVFILT_USER;
    Require(sceVideoOutGetEventId(&otherFilterEvent) == VIDEO_OUT_ERROR_INVALID_EVENT);

    Require(sceVideoOutClose(handle) == 0);

    for (const int closed : {handle, -1, 0, 42, 9999}) {
        Require(sceVideoOutAddVrrActiveStatusEvent(eq, closed, nullptr) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutAddVrrActiveStatusEvent(eq, closed, &dummyUdata) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutDeleteVrrActiveStatusEvent(eq, closed) == VIDEO_OUT_ERROR_INVALID_HANDLE);

        Require(sceVideoOutAddVrrActiveStatusEvent(0, closed, nullptr) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutDeleteVrrActiveStatusEvent(0, closed) == VIDEO_OUT_ERROR_INVALID_HANDLE);

        Require(sceVideoOutAddVrrActiveStatusEvent(0x7FFFFFFF, closed, nullptr) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutDeleteVrrActiveStatusEvent(0x7FFFFFFF, closed) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    }

    Require(sceKernelDeleteEqueue(eq) == 0);

    LibcRunShutdown_nid_postfix();
    return 0;
}
