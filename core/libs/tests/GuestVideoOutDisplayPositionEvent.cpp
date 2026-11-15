#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/InventoryExports.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
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
    KernelEqueue queue = 0;
    Require(sceKernelCreateEqueue(&queue, "display position test") == 0, "event queue creation failed");
    Require(sceVideoOutAddDisplayPositionEvent(queue, -1, nullptr) == InvalidHandle, "a display-position event was added on an unknown handle");
    Require(sceVideoOutAddDisplayPositionEvent(0, -1, nullptr) == InvalidHandle, "the queue was checked before the handle on add");
    Require(sceVideoOutDeleteDisplayPositionEvent(0, -1) == InvalidHandle, "the queue was checked before the handle on delete");

    int handle = 0;
    try {
        handle = sceVideoOutOpen(SystemUser, MainBus, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device for the open-port rows: %s\n", error.what());
        return 77;
    }
    Require(handle > 0, "sceVideoOutOpen failed");

    Require(sceVideoOutAddDisplayPositionEvent(queue, handle + 1, nullptr) == InvalidHandle, "a display-position event was added on a port that is not open");
    Require(sceVideoOutDeleteDisplayPositionEvent(queue, handle + 1) == InvalidHandle, "a display-position event was deleted on a port that is not open");
    Require(sceVideoOutAddDisplayPositionEvent(0, handle, nullptr) == InvalidEventQueue, "a display-position event was added to queue 0");
    Require(sceVideoOutDeleteDisplayPositionEvent(0, handle) == InvalidEventQueue, "a display-position event was deleted from queue 0");
    Require(sceVideoOutDeleteDisplayPositionEvent(queue, handle) == InvalidEvent, "deleting a display-position event that was never added did not return 0x8029000d");

    KernelEqueue deleted = 0;
    Require(sceKernelCreateEqueue(&deleted, "deleted queue") == 0 && sceKernelDeleteEqueue(deleted) == 0, "creating and deleting a second queue failed");
    Require(sceVideoOutAddDisplayPositionEvent(deleted, handle, nullptr) == InvalidEventQueue, "a display-position event was added to a deleted queue");
    Require(sceVideoOutDeleteDisplayPositionEvent(deleted, handle) == InvalidEventQueue, "a display-position event was deleted from a deleted queue");

    Require(sceVideoOutClose(handle) == 0, "sceVideoOutClose failed");
    Require(sceVideoOutDeleteDisplayPositionEvent(queue, handle) == InvalidHandle, "a display-position event was deleted on a closed port");
    Require(sceKernelDeleteEqueue(queue) == 0, "event queue deletion failed");
    std::printf("guest VideoOut display position event tests passed\n");
    LibcRunShutdown_nid_postfix();
    return 0;
}
