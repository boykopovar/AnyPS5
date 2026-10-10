#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

extern "C" {
int APS5_VABI sceVideoOutOpen(int userId, int busType, int index, const void* param);
int APS5_VABI sceVideoOutClose(int handle);
int APS5_VABI sceVideoOutGetOutputStatus(int handle, VideoOutOutputStatus* status);
int APS5_VABI sceVideoOutAllowOutputResolutionWqhdDetection(int handle);
}

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;
static constexpr int NEVER_OPENED_HANDLE = 2;

static void Require(bool value) { if (!value) std::abort(); }

static bool RejectsHandle(int handle) {
    try {
        sceVideoOutAllowOutputResolutionWqhdDetection(handle);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool SameStatus(const VideoOutOutputStatus& a, const VideoOutOutputStatus& b) {
    return std::memcmp(&a, &b, sizeof(a)) == 0;
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
    Require(handle > 0);

    VideoOutOutputStatus before{};
    Require(sceVideoOutGetOutputStatus(handle, &before) == 0);
    Require(sceVideoOutAllowOutputResolutionWqhdDetection(handle) == 0);
    Require(sceVideoOutAllowOutputResolutionWqhdDetection(handle) == 0);
    VideoOutOutputStatus after{};
    Require(sceVideoOutGetOutputStatus(handle, &after) == 0);
    Require(SameStatus(before, after));

    Require(RejectsHandle(0));
    Require(RejectsHandle(-1));
    Require(RejectsHandle(NEVER_OPENED_HANDLE));

    Require(sceVideoOutClose(handle) == 0);
    Require(RejectsHandle(handle));
    LibcRunShutdown_nid_postfix();
}
