#include "prx/libSceVideoOut/include/Output.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;
static constexpr int NEVER_OPENED_HANDLE = 2;
static constexpr int VIDEO_OUT_ERROR_INVALID_VALUE = static_cast<int>(0x80290001u);

static void Require(bool value) { if (!value) std::abort(); }

static bool RejectsHandle(int handle, int rate) {
    try {
        sceVideoOutSetFlipRate(handle, rate);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int main() {
    VideoOutTestEnvironment environment;

    Require(RejectsHandle(0, 0));
    Require(RejectsHandle(-1, 0));
    Require(RejectsHandle(NEVER_OPENED_HANDLE, 0));
    Require(RejectsHandle(0, -1));
    Require(RejectsHandle(-1, -1));
    Require(RejectsHandle(NEVER_OPENED_HANDLE, 3));

    int handle = 0;
    try {
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(handle > 0);

    Require(sceVideoOutSetFlipRate(handle, 0) == 0);
    Require(sceVideoOutSetFlipRate(handle, 1) == 0);
    Require(sceVideoOutSetFlipRate(handle, 2) == 0);
    Require(sceVideoOutSetFlipRate(handle, 1) == 0);
    Require(sceVideoOutSetFlipRate(handle, 0) == 0);

    Require(sceVideoOutSetFlipRate(handle, -1) == VIDEO_OUT_ERROR_INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(handle, -2) == VIDEO_OUT_ERROR_INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(handle, -100) == VIDEO_OUT_ERROR_INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(handle, 3) == VIDEO_OUT_ERROR_INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(handle, 4) == VIDEO_OUT_ERROR_INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(handle, 60) == VIDEO_OUT_ERROR_INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(handle, 120) == VIDEO_OUT_ERROR_INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(handle, 1000) == VIDEO_OUT_ERROR_INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(handle, 0x7FFFFFFF) == VIDEO_OUT_ERROR_INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(handle, static_cast<int>(0x80000000u)) == VIDEO_OUT_ERROR_INVALID_VALUE);

    Require(sceVideoOutClose(handle) == 0);
    Require(RejectsHandle(handle, 0));
    Require(RejectsHandle(handle, 1));
    Require(RejectsHandle(handle, -1));
    Require(RejectsHandle(handle, 3));

    LibcRunShutdown_nid_postfix();
    return 0;
}
