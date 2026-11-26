#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/Event.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

constexpr int SystemUser = 255;
constexpr int MainBus = 0;
constexpr int OverlayBus = 1;
constexpr int InvalidHandle = static_cast<int>(0x8029000Bu);
constexpr int NotMainBus = static_cast<int>(0x8029001Bu);

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    std::abort();
}

}

int main() {
    VideoOutTestEnvironment environment;
    int mainHandle = 0;
    int overlayHandle = 0;
    try {
        mainHandle = sceVideoOutOpen(SystemUser, MainBus, 0, nullptr);
        overlayHandle = sceVideoOutOpen(SystemUser, OverlayBus, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(mainHandle > 0, "the main bus did not open");
    Require(overlayHandle > 0 && overlayHandle != mainHandle, "the overlay bus did not open");

    KernelEqueue queue = 0;
    Require(sceKernelCreateEqueue(&queue, "output mode bus type test") == 0, "event queue creation failed");

    Require(sceVideoOutAddOutputModeEvent(queue, mainHandle, nullptr) == 0, "the main bus did not register an output-mode event");
    Require(sceVideoOutDeleteOutputModeEvent(queue, mainHandle) == 0, "the main bus did not delete its output-mode event");
    Require(sceVideoOutAddSetModeEvent(queue, overlayHandle, nullptr) == 0, "the overlay bus did not register a set-mode event");
    Require(sceVideoOutDeleteSetModeEvent(queue, overlayHandle) == 0, "the overlay bus did not delete its set-mode event");

    Require(sceVideoOutAddOutputModeEvent(queue, overlayHandle, nullptr) == NotMainBus, "the overlay bus registered an output-mode event");
    Require(sceVideoOutDeleteOutputModeEvent(queue, overlayHandle) == NotMainBus, "the overlay bus deleted an output-mode event");
    Require(sceVideoOutAddOutputModeEvent(0, overlayHandle, nullptr) == NotMainBus, "the bus type was checked after the event queue");
    Require(sceVideoOutDeleteOutputModeEvent(0, overlayHandle) == NotMainBus, "the bus type was checked after the event queue on delete");
    Require(sceVideoOutAddOutputModeEvent(queue, -1, nullptr) == InvalidHandle, "the handle was not looked up before the bus type");
    Require(sceVideoOutDeleteOutputModeEvent(queue, -1) == InvalidHandle, "the handle was not looked up before the bus type on delete");

    Require(sceKernelDeleteEqueue(queue) == 0, "event queue deletion failed");
    Require(sceVideoOutClose(overlayHandle) == 0, "closing the overlay bus failed");
    Require(sceVideoOutClose(mainHandle) == 0, "closing the main bus failed");
    std::printf("VideoOut output-mode event bus type tests passed\n");
    LibcRunShutdown_nid_postfix();
    return 0;
}
