#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;
static constexpr int VIDEO_OUT_ERROR_INVALID_ADDRESS = static_cast<int>(0x80290002u);
static constexpr int VIDEO_OUT_ERROR_INVALID_HANDLE = static_cast<int>(0x8029000Bu);
static constexpr int VIDEO_OUT_ERROR_CLEAR_FLIP_MASTER = static_cast<int>(0x8029001Cu);

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "check failed at line %u\n", location.line());
        std::abort();
    }
}

static bool ThrowsVrrPeg(int handle, uint64_t arg1, uint64_t arg2) {
    try {
        sceVideoOutVrrPegToFixedRate(handle, arg1, arg2);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool ThrowsVrrUnpeg(int handle) {
    try {
        sceVideoOutVrrUnpegFromFixedRate(handle);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool ThrowsCursor2xMagnify(int32_t handle, int32_t index, bool enable) {
    try {
        sceVideoOutCursorSet2xMagnify(handle, index, enable);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool ThrowsSetFlipMaster(std::int32_t handle, std::int32_t masterHandle) {
    try {
        sceVideoOutSetFlipMaster(handle, masterHandle);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool ThrowsCursorDisable(int32_t handle, int32_t index) {
    try {
        sceVideoOutCursorDisable(handle, index);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool ThrowsCursorSetPosition(int32_t handle, int32_t index, uint32_t x, uint32_t y) {
    try {
        sceVideoOutCursorSetPosition(handle, index, x, y);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool ThrowsCursorEnable(int32_t handle, int32_t index, const void* address) {
    try {
        sceVideoOutCursorEnable(handle, index, address);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool ThrowsCursorSetImageAddress(int32_t handle, int32_t index, const void* address) {
    try {
        sceVideoOutCursorSetImageAddress(handle, index, address);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool ThrowsDbgVr60Privilege() {
    try {
        sceDbgVideoOutAddOutputModeVr60Privilege();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool ThrowsCursorSetHotSpot() {
    try {
        sceVideoOutCursorSetHotSpot();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int main() {
    VideoOutTestEnvironment environment;

    Require(sceVideoOutAddVrrStatusFlagsPrivilege() == 0);
    Require(ThrowsDbgVr60Privilege());
    Require(ThrowsCursorSetHotSpot());

    Require(sceVideoOutVrrPegToFixedRate(-1, 0, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutVrrPegToFixedRate(-1, 1, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutVrrUnpegFromFixedRate(-1) == VIDEO_OUT_ERROR_INVALID_HANDLE);

    const int unopenedHandles[] = {0, 42, 9999};
    for (const int unopened : unopenedHandles) {
        Require(ThrowsVrrPeg(unopened, 0, 0));
        Require(ThrowsVrrPeg(unopened, 1, 0));
        Require(ThrowsVrrUnpeg(unopened));
    }

    const int allUnopenedHandles[] = {0, -1, 42, 9999};
    for (const int unopened : allUnopenedHandles) {
        Require(sceVideoOutCursorSet2xMagnify(unopened, 0, true) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutCursorSet2xMagnify(unopened, 0, false) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(ThrowsCursor2xMagnify(unopened, 1, true));
        Require(ThrowsCursor2xMagnify(unopened, -1, false));
        Require(sceVideoOutSetFlipMaster(unopened, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutSetFlipMaster(unopened, 1) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutClearFlipMaster(unopened) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutCursorDisable(unopened, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(ThrowsCursorDisable(unopened, 1));
        Require(sceVideoOutCursorSetPosition(unopened, 0, 100, 100) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(ThrowsCursorSetPosition(unopened, 1, 100, 100));
        Require(sceVideoOutCursorEnable(unopened, 0, nullptr) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(ThrowsCursorEnable(unopened, 1, nullptr));
        Require(sceVideoOutCursorSetImageAddress(unopened, 0, nullptr) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(ThrowsCursorSetImageAddress(unopened, 1, nullptr));
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

    Require(sceVideoOutVrrPegToFixedRate(handle, 0, 0) == 0);
    Require(sceVideoOutVrrPegToFixedRate(handle, 0, 0) == 0);
    Require(ThrowsVrrPeg(handle, 1, 0));
    Require(ThrowsVrrPeg(handle, 0, 1));
    Require(ThrowsVrrPeg(handle, 1, 1));
    Require(ThrowsVrrPeg(handle, 0x100000000ull, 0));
    Require(ThrowsVrrPeg(handle, 0, 0x100000000ull));
    Require(ThrowsVrrPeg(handle, ~0ull, ~0ull));

    Require(sceVideoOutVrrUnpegFromFixedRate(handle) == 0);
    Require(sceVideoOutVrrUnpegFromFixedRate(handle) == 0);
    Require(sceVideoOutVrrPegToFixedRate(handle, 0, 0) == 0);
    Require(sceVideoOutVrrUnpegFromFixedRate(handle) == 0);

    Require(sceVideoOutSetFlipMaster(handle, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutSetFlipMaster(handle, 1) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(ThrowsSetFlipMaster(handle, 2));
    Require(ThrowsSetFlipMaster(handle, -1));
    Require(ThrowsSetFlipMaster(handle, 3));
    Require(sceVideoOutClearFlipMaster(handle) == VIDEO_OUT_ERROR_CLEAR_FLIP_MASTER);
    Require(sceVideoOutClearFlipMaster(handle) == VIDEO_OUT_ERROR_CLEAR_FLIP_MASTER);

    Require(sceVideoOutCursorSet2xMagnify(handle, 0, true) == 0);
    Require(sceVideoOutCursorSet2xMagnify(handle, 0, false) == 0);
    Require(ThrowsCursor2xMagnify(handle, 1, true));
    Require(ThrowsCursor2xMagnify(handle, -1, false));
    Require(ThrowsCursor2xMagnify(handle, 2, true));

    Require(sceVideoOutCursorDisable(handle, 0) == 0);
    Require(ThrowsCursorDisable(handle, 1));
    Require(ThrowsCursorDisable(handle, -1));

    Require(sceVideoOutCursorSetPosition(handle, 0, 0, 0) == 0);
    Require(sceVideoOutCursorSetPosition(handle, 0, 1920, 1080) == 0);
    Require(ThrowsCursorSetPosition(handle, 1, 100, 100));

    Require(sceVideoOutCursorEnable(handle, 0, nullptr) == VIDEO_OUT_ERROR_INVALID_ADDRESS);
    uint32_t dummyPixel = 0xFFFFFFFFu;
    Require(ThrowsCursorEnable(handle, 0, &dummyPixel));
    Require(ThrowsCursorEnable(handle, 1, nullptr));

    Require(sceVideoOutCursorSetImageAddress(handle, 0, nullptr) == VIDEO_OUT_ERROR_INVALID_ADDRESS);
    Require(ThrowsCursorSetImageAddress(handle, 0, &dummyPixel));
    Require(ThrowsCursorSetImageAddress(handle, 1, nullptr));

    Require(sceVideoOutClose(handle) == 0);

    Require(ThrowsVrrPeg(handle, 0, 0));
    Require(ThrowsVrrUnpeg(handle));
    Require(sceVideoOutSetFlipMaster(handle, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutSetFlipMaster(handle, 1) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutClearFlipMaster(handle) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutCursorSet2xMagnify(handle, 0, true) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutCursorSet2xMagnify(handle, 0, false) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(ThrowsCursor2xMagnify(handle, 1, true));
    Require(sceVideoOutCursorDisable(handle, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(ThrowsCursorDisable(handle, 1));
    Require(sceVideoOutCursorSetPosition(handle, 0, 10, 10) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(ThrowsCursorSetPosition(handle, 1, 10, 10));
    Require(sceVideoOutCursorEnable(handle, 0, nullptr) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutCursorSetImageAddress(handle, 0, nullptr) == VIDEO_OUT_ERROR_INVALID_HANDLE);

    LibcRunShutdown_nid_postfix();
    return 0;
}
