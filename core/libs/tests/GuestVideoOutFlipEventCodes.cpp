#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/Event.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

constexpr int SystemUser = 255;
constexpr int MainBus = 0;
constexpr int InvalidHandle = static_cast<int>(0x8029000Bu);
constexpr int InvalidEventQueue = static_cast<int>(0x8029000Cu);
constexpr int InvalidEvent = static_cast<int>(0x8029000Du);

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    std::abort();
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
    Require(sceKernelCreateEqueue(&queue, "flip event codes") == 0, "event queue creation failed");
    Require(sceVideoOutDeleteFlipEvent(queue, handle) == InvalidEvent, "deleting a flip event that was never added did not return INVALID_EVENT");
    Require(sceVideoOutAddFlipEvent(queue, handle, nullptr) == 0, "the flip event was not added");
    Require(sceVideoOutDeleteFlipEvent(queue, handle) == 0, "the flip event was not deleted");
    Require(sceVideoOutDeleteFlipEvent(queue, handle) == InvalidEvent, "deleting the flip event a second time did not return INVALID_EVENT");

    Require(sceVideoOutAddFlipEvent(0, handle, nullptr) == InvalidEventQueue, "adding a flip event to queue 0 did not return INVALID_EVENT_QUEUE");
    Require(sceVideoOutDeleteFlipEvent(0, handle) == InvalidEventQueue, "deleting a flip event from queue 0 did not return INVALID_EVENT_QUEUE");
    KernelEqueue deleted = 0;
    Require(sceKernelCreateEqueue(&deleted, "deleted queue") == 0 && sceKernelDeleteEqueue(deleted) == 0, "the queue to delete was not created and deleted");
    Require(sceVideoOutAddFlipEvent(deleted, handle, nullptr) == InvalidEventQueue, "adding a flip event to a deleted queue did not return INVALID_EVENT_QUEUE");
    Require(sceVideoOutDeleteFlipEvent(deleted, handle) == InvalidEventQueue, "deleting a flip event from a deleted queue did not return INVALID_EVENT_QUEUE");
    Require(sceVideoOutAddFlipEvent(0, handle + 1, nullptr) == InvalidHandle, "the handle was not checked before the queue");

    Require(sceVideoOutClose(handle) == 0, "sceVideoOutClose failed");
    Require(sceKernelDeleteEqueue(queue) == 0, "event queue deletion failed");
    std::printf("guest VideoOut flip event code tests passed\n");
    LibcRunShutdown_nid_postfix();
    return 0;
}
