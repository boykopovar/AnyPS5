#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;
static constexpr int VIDEO_OUT_ERROR_INVALID_HANDLE = static_cast<int>(0x8029000Bu);
static constexpr int VIDEO_OUT_ERROR_INVALID_ADDRESS = static_cast<int>(0x80290002u);

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

static bool ThrowsCursorEnable(int handle, int index, const void* address) {
    try {
        sceVideoOutCursorEnable(handle, index, address);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool ThrowsCursorDisable(int handle, int index) {
    try {
        sceVideoOutCursorDisable(handle, index);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool ThrowsCursorSetImageAddress(int handle, int index, const void* address) {
    try {
        sceVideoOutCursorSetImageAddress(handle, index, address);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool ThrowsCursorSetPosition(int handle, int index, uint32_t x, uint32_t y) {
    try {
        sceVideoOutCursorSetPosition(handle, index, x, y);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool ThrowsCursorSet2xMagnify(int handle, int index, bool enable) {
    try {
        sceVideoOutCursorSet2xMagnify(handle, index, enable);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int main() {
    VideoOutTestEnvironment environment;

    const int unopenedHandles[] = {-1, 0, 1, 2, 42, 9999, -9999, 0x7FFFFFFF};
    const uint32_t dummyImage[64] = {};
    for (const int unopened : unopenedHandles) {
        Require(sceVideoOutCursorSet2xMagnify(unopened, 0, true) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutCursorSet2xMagnify(unopened, 0, false) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutCursorDisable(unopened, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutCursorSetPosition(unopened, 0, 0, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutCursorSetPosition(unopened, 0, 100, 100) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutCursorEnable(unopened, 0, nullptr) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutCursorEnable(unopened, 0, dummyImage) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutCursorSetImageAddress(unopened, 0, nullptr) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutCursorSetImageAddress(unopened, 0, dummyImage) == VIDEO_OUT_ERROR_INVALID_HANDLE);

        for (const int badIndex : {-10, -1, 1, 2, 3, 10, 42, 0x7FFFFFFF}) {
            Require(ThrowsCursorSet2xMagnify(unopened, badIndex, true));
            Require(ThrowsCursorSet2xMagnify(unopened, badIndex, false));
            Require(ThrowsCursorDisable(unopened, badIndex));
            Require(ThrowsCursorSetPosition(unopened, badIndex, 0, 0));
            Require(ThrowsCursorEnable(unopened, badIndex, nullptr));
            Require(ThrowsCursorEnable(unopened, badIndex, dummyImage));
            Require(ThrowsCursorSetImageAddress(unopened, badIndex, nullptr));
            Require(ThrowsCursorSetImageAddress(unopened, badIndex, dummyImage));
        }
    }

    int handle = 0;
    try {
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(handle > 0);

    Require(sceVideoOutCursorDisable(handle, 0) == 0);
    Require(sceVideoOutCursorDisable(handle, 0) == 0);

    Require(sceVideoOutCursorSetPosition(handle, 0, 0, 0) == 0);
    Require(sceVideoOutCursorSetPosition(handle, 0, 100, 100) == 0);
    Require(sceVideoOutCursorSetPosition(handle, 0, 1920, 1080) == 0);
    Require(sceVideoOutCursorSetPosition(handle, 0, 3840, 2160) == 0);
    Require(sceVideoOutCursorSetPosition(handle, 0, 0xFFFFFFFFu, 0xFFFFFFFFu) == 0);

    Require(sceVideoOutCursorSet2xMagnify(handle, 0, true) == 0);
    Require(sceVideoOutCursorSet2xMagnify(handle, 0, false) == 0);
    Require(sceVideoOutCursorSet2xMagnify(handle, 0, true) == 0);

    Require(sceVideoOutCursorEnable(handle, 0, nullptr) == VIDEO_OUT_ERROR_INVALID_ADDRESS);
    Require(ThrowsCursorEnable(handle, 0, dummyImage));

    Require(sceVideoOutCursorSetImageAddress(handle, 0, nullptr) == VIDEO_OUT_ERROR_INVALID_ADDRESS);
    Require(ThrowsCursorSetImageAddress(handle, 0, dummyImage));

    for (const int badIndex : {-100, -1, 1, 2, 3, 5, 10, 42, 100, 0x7FFFFFFF}) {
        Require(ThrowsCursorDisable(handle, badIndex));
        Require(ThrowsCursorSetPosition(handle, badIndex, 100, 100));
        Require(ThrowsCursorSet2xMagnify(handle, badIndex, true));
        Require(ThrowsCursorSet2xMagnify(handle, badIndex, false));
        Require(ThrowsCursorEnable(handle, badIndex, nullptr));
        Require(ThrowsCursorEnable(handle, badIndex, dummyImage));
        Require(ThrowsCursorSetImageAddress(handle, badIndex, nullptr));
        Require(ThrowsCursorSetImageAddress(handle, badIndex, dummyImage));
    }

    Require(sceVideoOutClose(handle) == 0);

    Require(sceVideoOutCursorSet2xMagnify(handle, 0, true) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutCursorSet2xMagnify(handle, 0, false) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutCursorDisable(handle, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutCursorSetPosition(handle, 0, 100, 100) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutCursorEnable(handle, 0, nullptr) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutCursorEnable(handle, 0, dummyImage) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutCursorSetImageAddress(handle, 0, nullptr) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutCursorSetImageAddress(handle, 0, dummyImage) == VIDEO_OUT_ERROR_INVALID_HANDLE);

    for (const int badIndex : {-1, 1, 2, 42, 0x7FFFFFFF}) {
        Require(ThrowsCursorSet2xMagnify(handle, badIndex, true));
        Require(ThrowsCursorDisable(handle, badIndex));
        Require(ThrowsCursorSetPosition(handle, badIndex, 0, 0));
        Require(ThrowsCursorEnable(handle, badIndex, nullptr));
        Require(ThrowsCursorEnable(handle, badIndex, dummyImage));
        Require(ThrowsCursorSetImageAddress(handle, badIndex, nullptr));
        Require(ThrowsCursorSetImageAddress(handle, badIndex, dummyImage));
    }

    LibcRunShutdown_nid_postfix();
    return 0;
}
