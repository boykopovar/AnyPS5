#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/Event.hpp"
#include "prx/libSceSystemService/include/SystemService.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "prx/libSceAgcDriver/Execution/include/HostHdr.hpp"
#include "prx/libSceSystemService/SystemService.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>

namespace {

constexpr int SystemUser = 255;
constexpr int MainBus = 0;
constexpr int OutputModeEvent = 8;

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    std::abort();
}

VideoOutOutputStatus Status(int handle) {
    VideoOutOutputStatus status{};
    Require(sceVideoOutGetOutputStatus(handle, &status) == 0, "sceVideoOutGetOutputStatus failed");
    std::array<std::byte, 80> withVrr{};
    Require(sceVideoOutGetOutputStatusWithVrr(handle, withVrr.data()) == 0, "sceVideoOutGetOutputStatusWithVrr failed");
    Require(std::memcmp(withVrr.data(), &status, sizeof(status)) == 0, "the output status with VRR differs from the output status");
    return status;
}

SystemServiceHdrToneMapLuminance ToneMap() {
    SystemServiceHdrToneMapLuminance luminance{-1.0f, -1.0f, -1.0f};
    Require(sceSystemServiceGetHdrToneMapLuminance(&luminance) == SYSTEM_SERVICE_OK, "sceSystemServiceGetHdrToneMapLuminance failed");
    return luminance;
}

bool OutputModeEventArrives(KernelEqueue queue, KernelUseconds timeout) {
    KernelEvent event{};
    int count = 0;
    return sceKernelWaitEqueue(queue, &event, 1, &count, &timeout) == 0 && count == 1 && sceVideoOutGetEventId(&event) == OutputModeEvent;
}

}

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
    Require(handle > 0, "sceVideoOutOpen failed");
    AgcDriverSetHostDisplayHdr_nid_postfix({});
    AgcDriverSetSurfaceHdr10_nid_postfix(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto status = Status(handle);
    Require(status.dynamicRange == 1 && status.flags == 0, "an SDR host reports HDR output");
    auto luminance = ToneMap();
    Require(luminance.max_tone_map_luminance == 100.0f && luminance.max_full_frame_tone_map_luminance == 100.0f && luminance.min_tone_map_luminance == 0.0f, "an SDR host does not report SDR reference white");

    KernelEqueue queue = 0;
    Require(sceKernelCreateEqueue(&queue, "HDR test") == 0, "event queue creation failed");
    Require(sceVideoOutAddOutputModeEvent(queue, handle, nullptr) == 0, "output mode subscription failed");
    Require(OutputModeEventArrives(queue, 0), "the output mode subscription is not triggered once");

    AgcDriverSetHostDisplayHdr_nid_postfix(AgcDriver::DisplayHdrFromOutput(true, 1015.0f, 0.005f, 640.0f));
    status = Status(handle);
    Require(status.dynamicRange == 1 && status.flags == 0, "an HDR10 display without an HDR10 surface reports HDR output");
    Require(ToneMap().max_tone_map_luminance == 100.0f, "an HDR10 display without an HDR10 surface reports its luminance");
    Require(!OutputModeEventArrives(queue, 200'000), "an HDR10 display without an HDR10 surface fires the output mode event");

    AgcDriverSetSurfaceHdr10_nid_postfix(true);
    Require(OutputModeEventArrives(queue, 2'000'000), "HDR output becoming available does not fire the output mode event");
    status = Status(handle);
    Require(status.dynamicRange == 2 && status.flags == 0, "HDR output is not reported with flags 0");
    Require(status.resolution != 0 && status.refreshRate == 3 && status.reserved[0] == 0 && status.reserved[1] == 0 && status.reserved[2] == 0, "the rest of the output status changed with HDR");
    luminance = ToneMap();
    Require(luminance.max_tone_map_luminance == 1015.0f && luminance.max_full_frame_tone_map_luminance == 640.0f && luminance.min_tone_map_luminance == 0.005f, "the display's luminance is not the tone-map luminance");
    Require(AgcDriver::CurrentHostHdr().reportedToneMapPeak == 1015.0f, "the tone-map peak given to the title is not recorded for the SDR conversion");

    AgcDriverSetHostDisplayHdr_nid_postfix(AgcDriver::DisplayHdrFromOutput(true, 400.0f, 0.05f, 300.0f));
    Require(OutputModeEventArrives(queue, 2'000'000), "a new display luminance does not fire the output mode event");
    Require(ToneMap().max_tone_map_luminance == 400.0f && Status(handle).dynamicRange == 2, "the new display luminance is not reported");

    AgcDriverSetHostDisplayHdr_nid_postfix({});
    Require(OutputModeEventArrives(queue, 2'000'000), "HDR output going away does not fire the output mode event");
    status = Status(handle);
    Require(status.dynamicRange == 1 && status.flags == 0, "HDR output is reported after the display left HDR mode");
    Require(ToneMap().max_tone_map_luminance == 100.0f && AgcDriver::CurrentHostHdr().reportedToneMapPeak == 100.0f, "SDR reference white is not reported after the display left HDR mode");

    Require(sceVideoOutClose(handle) == 0, "sceVideoOutClose failed");
    Require(sceKernelDeleteEqueue(queue) == 0, "event queue deletion failed");
    std::printf("guest VideoOut HDR tests passed\n");
    LibcRunShutdown_nid_postfix();
    return 0;
}
