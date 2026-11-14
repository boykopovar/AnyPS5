#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/Event.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

constexpr int SystemUser = 255;
constexpr int MainBus = 0;
constexpr int InvalidHandle = static_cast<int>(0x8029000Bu);
constexpr int SetModeEvent = 8;
constexpr std::int64_t DefaultOutputMode = 1;

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    std::abort();
}

int Collect(KernelEqueue queue, std::array<KernelEvent, 2>& events) {
    KernelUseconds timeout = 0;
    int count = 0;
    return sceKernelWaitEqueue(queue, events.data(), static_cast<int>(events.size()), &count, &timeout) == 0 ? count : 0;
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

    KernelEqueue queue = 0;
    Require(sceKernelCreateEqueue(&queue, "set mode test") == 0, "event queue creation failed");
    Require(sceVideoOutAddSetModeEvent(queue, handle + 1, nullptr) == InvalidHandle, "a set-mode event was added on a port that is not open");
    Require(sceVideoOutDeleteSetModeEvent(queue, handle + 1) == InvalidHandle, "a set-mode event was deleted on a port that is not open");

    int setMode = 0;
    int outputMode = 0;
    std::array<KernelEvent, 2> events{};
    Require(sceVideoOutAddSetModeEvent(queue, handle, &setMode) == 0, "the set-mode event was not added");
    Require(sceVideoOutAddOutputModeEvent(queue, handle, &outputMode) == 0, "the output-mode event was not added after the set-mode event");
    Require(Collect(queue, events) == 1, "the set-mode and output-mode events are two registrations");
    Require(sceVideoOutGetEventId(&events[0]) == SetModeEvent && events[0].udata == &outputMode, "the output-mode event did not take over the set-mode registration");
    std::int64_t mode = 0;
    Require(sceVideoOutGetEventData(&events[0], &mode) == 0 && mode == DefaultOutputMode && sceVideoOutGetEventCount(&events[0]) == 1, "the set-mode event does not carry the current output mode");

    Require(sceVideoOutDeleteSetModeEvent(queue, handle) == 0, "the set-mode event was not deleted");
    Require(sceVideoOutAddOutputModeEvent(queue, handle, &outputMode) == 0, "the output-mode event was not added after the set-mode deletion");
    Require(Collect(queue, events) == 1 && events[0].udata == &outputMode, "deleting the set-mode event left the output-mode registration in place");
    Require(sceVideoOutDeleteOutputModeEvent(queue, handle) == 0, "the output-mode event was not deleted");

    Require(sceVideoOutAddSetModeEvent(queue, handle, &setMode) == 0, "the set-mode event was not added again");
    Require(Collect(queue, events) == 1 && events[0].udata == &setMode, "the set-mode event was not armed when added alone");
    Require(sceVideoOutDeleteOutputModeEvent(queue, handle) == 0, "the output-mode deletion failed on a set-mode registration");
    Require(sceVideoOutAddSetModeEvent(queue, handle, &setMode) == 0, "the set-mode event was not added after the output-mode deletion");
    Require(Collect(queue, events) == 1, "deleting the output-mode event left the set-mode registration in place");
    Require(sceVideoOutDeleteSetModeEvent(queue, handle) == 0, "the set-mode event was not deleted at the end");

    Require(sceVideoOutClose(handle) == 0, "sceVideoOutClose failed");
    Require(sceVideoOutAddSetModeEvent(queue, handle, nullptr) == InvalidHandle, "a set-mode event was added on a closed port");
    Require(sceVideoOutDeleteSetModeEvent(queue, handle) == InvalidHandle, "a set-mode event was deleted on a closed port");
    Require(sceKernelDeleteEqueue(queue) == 0, "event queue deletion failed");
    std::printf("guest VideoOut set-mode event tests passed\n");
    LibcRunShutdown_nid_postfix();
    return 0;
}
