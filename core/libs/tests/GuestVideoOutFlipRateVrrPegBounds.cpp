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
static constexpr int VIDEO_OUT_ERROR_INVALID_VALUE = static_cast<int>(0x80290001u);
static constexpr int VIDEO_OUT_ERROR_INVALID_HANDLE = static_cast<int>(0x8029000Bu);

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

static bool ThrowsSetFlipRate(int handle, int rate) {
    try {
        sceVideoOutSetFlipRate(handle, rate);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
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

int main() {
    VideoOutTestEnvironment environment;

    Require(sceVideoOutAddVrrStatusFlagsPrivilege() == 0);

    const int unopenedHandles[] = {-1, 0, 42, 9999, 0x7FFFFFFF};
    for (const int unopened : unopenedHandles) {
        for (const int rate : {-1, 0, 1, 2, 3, 60}) {
            Require(ThrowsSetFlipRate(unopened, rate));
        }
        if (unopened == -1) {
            Require(sceVideoOutVrrPegToFixedRate(unopened, 0, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
            Require(sceVideoOutVrrPegToFixedRate(unopened, 1, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
            Require(sceVideoOutVrrPegToFixedRate(unopened, 0, 1) == VIDEO_OUT_ERROR_INVALID_HANDLE);
            Require(sceVideoOutVrrPegToFixedRate(unopened, 60, 60) == VIDEO_OUT_ERROR_INVALID_HANDLE);
            Require(sceVideoOutVrrUnpegFromFixedRate(unopened) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        } else {
            Require(ThrowsVrrPeg(unopened, 0, 0));
            Require(ThrowsVrrPeg(unopened, 1, 0));
            Require(ThrowsVrrPeg(unopened, 0, 1));
            Require(ThrowsVrrPeg(unopened, 60, 60));
            Require(ThrowsVrrUnpeg(unopened));
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

    for (const int rate : {0, 1, 2}) {
        Require(sceVideoOutSetFlipRate(handle, rate) == 0);
    }

    const int invalidRates[] = {-100, -60, -1, 3, 4, 5, 10, 30, 59, 60, 119, 120, 240, 1000, 0x7FFFFFFF};
    for (const int badRate : invalidRates) {
        Require(sceVideoOutSetFlipRate(handle, badRate) == VIDEO_OUT_ERROR_INVALID_VALUE);
    }

    const int rateSequence[] = {0, 1, 2, 1, 0, 2, 1, 0};
    for (const int rate : rateSequence) {
        Require(sceVideoOutSetFlipRate(handle, rate) == 0);
    }

    Require(sceVideoOutVrrPegToFixedRate(handle, 0, 0) == 0);
    Require(sceVideoOutVrrPegToFixedRate(handle, 0, 0) == 0);
    Require(ThrowsVrrPeg(handle, 1, 0));
    Require(ThrowsVrrPeg(handle, 0, 1));
    Require(ThrowsVrrPeg(handle, 1, 1));
    Require(ThrowsVrrPeg(handle, 60, 0));
    Require(ThrowsVrrPeg(handle, 0, 60));
    Require(ThrowsVrrPeg(handle, 60, 60));
    Require(ThrowsVrrPeg(handle, 120, 120));
    Require(ThrowsVrrPeg(handle, 0x100000000ull, 0));
    Require(ThrowsVrrPeg(handle, 0, 0x100000000ull));
    Require(ThrowsVrrPeg(handle, ~0ull, ~0ull));

    Require(sceVideoOutVrrUnpegFromFixedRate(handle) == 0);
    Require(sceVideoOutVrrUnpegFromFixedRate(handle) == 0);

    for (const int rate : {0, 1, 2}) {
        Require(sceVideoOutSetFlipRate(handle, rate) == 0);
        Require(sceVideoOutVrrPegToFixedRate(handle, 0, 0) == 0);
        Require(sceVideoOutVrrUnpegFromFixedRate(handle) == 0);
    }

    Require(sceVideoOutClose(handle) == 0);

    for (const int rate : {-1, 0, 1, 2, 3, 60}) {
        Require(ThrowsSetFlipRate(handle, rate));
    }
    Require(ThrowsVrrPeg(handle, 0, 0));
    Require(ThrowsVrrPeg(handle, 1, 0));
    Require(ThrowsVrrUnpeg(handle));

    LibcRunShutdown_nid_postfix();
    return 0;
}
