#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/InventoryExports.hpp"
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
constexpr int InvalidOption = static_cast<int>(0x8029001Au);
constexpr int PortUnsupported = static_cast<int>(0x8029001Bu);

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    std::abort();
}

}

int main() {
    VideoOutTestEnvironment environment;
    int reserved = 0;
    const std::uint32_t control = 1;

    int handle = 0;
    try {
        handle = sceVideoOutOpen(SystemUser, MainBus, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device for the open-port rows: %s\n", error.what());
        return 77;
    }
    Require(handle > 0, "sceVideoOutOpen failed");

    Require(sceVideoOutSetGlobalAlpha(-1, 1, 0x80) == InvalidHandle, "SetGlobalAlpha on an unknown handle");
    Require(sceVideoOutSetAlphaControl(-1, 1, 0x80, &control, nullptr) == InvalidHandle, "SetAlphaControl on an unknown handle");
    Require(sceVideoOutSetAlphaControl(-1, 1, 0x80, &control, &reserved) == InvalidOption, "the reserved argument was not checked before the handle");

    Require(sceVideoOutSetGlobalAlpha(handle + 1, 1, 0x80) == InvalidHandle, "SetGlobalAlpha on a port that is not open");
    Require(sceVideoOutSetAlphaControl(handle + 1, 1, 0x80, &control, nullptr) == InvalidHandle, "SetAlphaControl on a port that is not open");
    Require(sceVideoOutSetGlobalAlpha(handle, 1, 0x80) == PortUnsupported, "SetGlobalAlpha on the main bus");
    Require(sceVideoOutSetGlobalAlpha(handle, 2, 0x80) == PortUnsupported, "the bus was not refused before the enable value");
    Require(sceVideoOutSetGlobalAlpha(handle, 1, 0x100) == PortUnsupported, "the bus was not refused before the alpha value");
    Require(sceVideoOutSetAlphaControl(handle, 1, 0x80, &control, nullptr) == PortUnsupported, "SetAlphaControl on the main bus");
    Require(sceVideoOutSetAlphaControl(handle, 0, 0x80, nullptr, nullptr) == PortUnsupported, "SetAlphaControl with no control on the main bus");
    Require(sceVideoOutSetAlphaControl(handle, 1, 0x80, &control, &reserved) == InvalidOption, "the reserved argument was not checked first on an open port");

    Require(sceVideoOutClose(handle) == 0, "sceVideoOutClose failed");
    Require(sceVideoOutSetGlobalAlpha(handle, 1, 0x80) == InvalidHandle, "SetGlobalAlpha on a closed port");
    std::printf("guest VideoOut global alpha tests passed\n");
    LibcRunShutdown_nid_postfix();
    return 0;
}
