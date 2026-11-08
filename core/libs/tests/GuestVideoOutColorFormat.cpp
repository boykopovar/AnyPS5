#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;

static void Require(bool value) {
    if (!value) std::abort();
}

static void ExpectFailure(auto&& action) {
    bool failed = false;
    try {
        action();
    } catch (const std::runtime_error&) {
        failed = true;
    }
    Require(failed);
}

int main() {
    VideoOutTestEnvironment environment;
    VideoOutBufferAttribute attr{};

    sceVideoOutSetBufferAttribute(&attr, 0x80000000u, 0, 0, 1920, 1080, 0);
    Require(attr.pixel_format == 0x80000000u);

    sceVideoOutSetBufferAttribute(&attr, 0x80002200u, 0, 0, 1920, 1080, 0);
    Require(attr.pixel_format == 0x80002200u);

    sceVideoOutSetBufferAttribute(&attr, 0x88000000u, 0, 0, 1920, 1080, 0);
    Require(attr.pixel_format == 0x88000000u);

    int handle = 0;
    try {
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(handle > 0);

    std::array<void*, 2> addresses = {
        reinterpret_cast<void*>(0x10000000),
        reinterpret_cast<void*>(0x10400000)
    };

    sceVideoOutSetBufferAttribute(&attr, 0x80000000u, 0, 0, 1920, 1080, 0);
    Require(sceVideoOutRegisterBuffers(handle, 0, addresses.data(), 2, &attr) == 0);

    sceVideoOutSetBufferAttribute(&attr, 0x80002200u, 0, 0, 1920, 1080, 0);
    Require(sceVideoOutSubmitChangeBufferAttribute(handle, 0, &attr) == 0);

    sceVideoOutSetBufferAttribute(&attr, 0x88000000u, 0, 0, 1920, 1080, 0);
    Require(sceVideoOutSubmitChangeBufferAttribute(handle, 0, &attr) == 0);

    sceVideoOutSetBufferAttribute(&attr, 0x12345678u, 0, 0, 1920, 1080, 0);
    ExpectFailure([&] { sceVideoOutRegisterBuffers(handle, 2, addresses.data(), 2, &attr); });
    ExpectFailure([&] { sceVideoOutSubmitChangeBufferAttribute(handle, 0, &attr); });

    Require(sceVideoOutUnregisterBuffers(handle, 0) == 0);
    Require(sceVideoOutClose(handle) == 0);

    LibcRunShutdown_nid_postfix();
    return 0;
}
