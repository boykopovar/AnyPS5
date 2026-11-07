#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;

static void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

int main() {
    VideoOutTestEnvironment environment;
    int handle = 0;
    try {
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(sceVideoOutWaitVblank(handle) == 0, "the first vblank wait failed");
    const std::uint64_t before = sceKernelReadTsc();
    Require(sceVideoOutWaitVblank(handle) == 0, "the second vblank wait failed");
    VideoOutVblankStatus status;
    std::memset(&status, 0xff, sizeof(status));
    Require(sceVideoOutGetVblankStatus(handle, &status) == 0, "sceVideoOutGetVblankStatus failed");
    const std::uint64_t after = sceKernelReadTsc();
    std::printf("TSC before the wait 0x%llx, third word 0x%llx, TSC after the call 0x%llx\n", static_cast<unsigned long long>(before),
                static_cast<unsigned long long>(status.reserved), static_cast<unsigned long long>(after));
    Require(status.reserved != 0, "the third word of the vblank status is 0");
    Require(before <= status.reserved && status.reserved <= after, "the third word is not the TSC of a vblank between the wait and the call");
    Require(sceVideoOutClose(handle) == 0, "close failed");
    LibcRunShutdown_nid_postfix();
    std::printf("guest VideoOut vblank status tests passed\n");
}
