#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;
static constexpr int HMD2_MAIN_BUS = 0x20;

static constexpr int INVALID_VALUE = static_cast<int>(0x80290001);
static constexpr int INVALID_ADDRESS = static_cast<int>(0x80290002);
static constexpr int INVALID_HANDLE = static_cast<int>(0x8029000B);
static constexpr int RESOURCE_BUSY = static_cast<int>(0x80290009);
static constexpr int UNSUPPORTED_OUTPUT_MODE = static_cast<int>(0x80290016);
static constexpr int UNKNOWN_OUTPUT_MODE = static_cast<int>(0x8029001E);

static constexpr std::uint64_t REFRESH_RATE_59_94HZ = 3;
static constexpr std::uint64_t REFRESH_RATE_119_88HZ = 13;
static constexpr std::uint64_t MODE_DEFAULT = 1;
static constexpr std::uint64_t MODE_119_88HZ = 15;

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

int main() {
    VideoOutTestEnvironment environment;

    Require(sceVideoOutInitializeOutputOptions(nullptr) == INVALID_ADDRESS);

    VideoOutOutputOptions options{};
    Require(sceVideoOutInitializeOutputOptions(&options) == 0);
    Require(options.internalData[0] == 0x00FF0000);
    for (std::size_t i = 1; i < 16; ++i) {
        Require(options.internalData[i] == 0);
    }

    for (int unopened : {-1, 99, 999, 0x7FFFFFFF}) {
        Require(sceVideoOutIsOutputSupported(unopened, MODE_DEFAULT, &options, nullptr, 0) == INVALID_HANDLE);
        Require(sceVideoOutConfigureOutput(unopened, MODE_DEFAULT, &options, nullptr, 0) == INVALID_HANDLE);
        Require(sceVideoOutClose(unopened) == INVALID_HANDLE);
        Require(sceVideoOutIsFlipPending(unopened) == INVALID_HANDLE);

        std::uint64_t cap = 0;
        Require(sceVideoOutGetDeviceCapabilityInfo_(unopened, &cap, sizeof(cap)) == INVALID_HANDLE);

        std::array<std::byte, 96> vrr{};
        Require(sceVideoOutGetOutputStatusWithVrr(unopened, vrr.data()) == INVALID_HANDLE);

        bool caughtRes = false;
        try {
            VideoOutResolutionStatus dummyRes{};
            sceVideoOutGetResolutionStatus(unopened, &dummyRes);
        } catch (const std::runtime_error&) {
            caughtRes = true;
        }
        Require(caughtRes);

        bool caughtOut = false;
        try {
            VideoOutOutputStatus dummyOut{};
            sceVideoOutGetOutputStatus(unopened, &dummyOut);
        } catch (const std::runtime_error&) {
            caughtOut = true;
        }
        Require(caughtOut);

        bool caughtVblank = false;
        try {
            VideoOutVblankStatus dummyVb{};
            sceVideoOutGetVblankStatus(unopened, &dummyVb);
        } catch (const std::runtime_error&) {
            caughtVblank = true;
        }
        Require(caughtVblank);
    }

    Require(sceVideoOutOpen(SYSTEM_USER, HMD2_MAIN_BUS, 0, nullptr) == INVALID_VALUE);
    Require(sceVideoOutOpen(0, HMD2_MAIN_BUS, 0, nullptr) == INVALID_VALUE);

    int handle = 0;
    try {
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) {
            throw;
        }
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(handle > 0);
    Require(sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr) == RESOURCE_BUSY);

    VideoOutResolutionStatus resStatus{};
    std::memset(&resStatus, 0xEE, sizeof(resStatus));
    Require(sceVideoOutGetResolutionStatus(handle, &resStatus) == 0);
    Require(resStatus.refreshRate == REFRESH_RATE_59_94HZ || resStatus.refreshRate == REFRESH_RATE_119_88HZ);
    Require(resStatus.fullWidth == 1920 || resStatus.fullWidth == 2560 || resStatus.fullWidth == 3840);
    Require(resStatus.fullHeight == 1080 || resStatus.fullHeight == 1440 || resStatus.fullHeight == 2160);
    Require(resStatus.paneWidth == resStatus.fullWidth);
    Require(resStatus.paneHeight == resStatus.fullHeight);
    Require(resStatus.screenSizeInInch == 0.0f);
    Require(resStatus.flags == 0);
    Require(resStatus.reserved0 == 0);
    Require(resStatus.reserved1[0] == 0 && resStatus.reserved1[1] == 0 && resStatus.reserved1[2] == 0);

    VideoOutOutputStatus outStatus{};
    Require(sceVideoOutGetOutputStatus(handle, &outStatus) == 0);
    Require(outStatus.refreshRate == resStatus.refreshRate);
    Require(outStatus.resolution == 1 || outStatus.resolution == 2);
    Require(outStatus.dynamicRange == 1 || outStatus.dynamicRange == 2);
    Require(outStatus.flags == 0);

    Require(sceVideoOutIsOutputSupported(handle, MODE_DEFAULT, &options, nullptr, 0) == 1);
    Require(sceVideoOutIsOutputSupported(handle, MODE_DEFAULT, nullptr, nullptr, 0) == 1);
    Require(sceVideoOutConfigureOutput(handle, MODE_DEFAULT, &options, nullptr, 0) == 0);

    Require(sceVideoOutGetResolutionStatus(handle, &resStatus) == 0);
    Require(resStatus.refreshRate == REFRESH_RATE_59_94HZ);
    Require(sceVideoOutGetOutputStatus(handle, &outStatus) == 0);
    Require(outStatus.refreshRate == REFRESH_RATE_59_94HZ);

    const int hfrSupported = sceVideoOutIsOutputSupported(handle, MODE_119_88HZ, &options, nullptr, 0);
    Require(hfrSupported == 0 || hfrSupported == 1);
    if (hfrSupported == 1) {
        if (sceVideoOutConfigureOutput(handle, MODE_119_88HZ, &options, nullptr, 0) == 0) {
            Require(sceVideoOutGetResolutionStatus(handle, &resStatus) == 0);
            Require(resStatus.refreshRate == REFRESH_RATE_119_88HZ);
            Require(sceVideoOutGetOutputStatus(handle, &outStatus) == 0);
            Require(outStatus.refreshRate == REFRESH_RATE_119_88HZ);

            Require(sceVideoOutConfigureOutput(handle, MODE_DEFAULT, &options, nullptr, 0) == 0);
            Require(sceVideoOutGetResolutionStatus(handle, &resStatus) == 0);
            Require(resStatus.refreshRate == REFRESH_RATE_59_94HZ);
        }
    }

    for (std::uint64_t mode : {0x4ull, 0x7ull, 0x8ull, 0xCull, 0xDull, 0xEull, 0x10ull, 0x11ull, 0x13ull}) {
        Require(sceVideoOutIsOutputSupported(handle, mode, &options, nullptr, 0) == UNSUPPORTED_OUTPUT_MODE);
        Require(sceVideoOutConfigureOutput(handle, mode, &options, nullptr, 0) == UNSUPPORTED_OUTPUT_MODE);
    }
    for (std::uint64_t mode : {0x0ull, 0x2ull, 0x3ull, 0x5ull, 0x6ull, 0x9ull, 0xAull, 0xBull, 0x14ull, 0x20ull, 0x100ull}) {
        Require(sceVideoOutIsOutputSupported(handle, mode, &options, nullptr, 0) == UNKNOWN_OUTPUT_MODE);
        Require(sceVideoOutConfigureOutput(handle, mode, &options, nullptr, 0) == UNKNOWN_OUTPUT_MODE);
    }

    bool caughtNullRes = false;
    try {
        sceVideoOutGetResolutionStatus(handle, nullptr);
    } catch (const std::runtime_error&) {
        caughtNullRes = true;
    }
    Require(caughtNullRes);

    bool caughtNullOut = false;
    try {
        sceVideoOutGetOutputStatus(handle, nullptr);
    } catch (const std::runtime_error&) {
        caughtNullOut = true;
    }
    Require(caughtNullOut);

    Require(sceVideoOutGetOutputStatusWithVrr(handle, nullptr) == INVALID_ADDRESS);

    bool caughtNullMode = false;
    try {
        sceVideoOutModeSetAny_(nullptr, 16);
    } catch (const std::runtime_error&) {
        caughtNullMode = true;
    }
    Require(caughtNullMode);

    std::uint64_t cap = 0;
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, &cap, sizeof(cap)) == 0);
    Require(cap == 1);
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, nullptr, sizeof(cap)) == INVALID_VALUE);
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, &cap, 4) == INVALID_VALUE);
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, &cap, 16) == INVALID_VALUE);

    Require(sceVideoOutIsOutputSupported(handle, MODE_DEFAULT, &options, &options, 0) == INVALID_VALUE);
    Require(sceVideoOutIsOutputSupported(handle, MODE_DEFAULT, &options, nullptr, 1) == INVALID_VALUE);
    Require(sceVideoOutConfigureOutput(handle, MODE_DEFAULT, &options, &options, 0) == INVALID_VALUE);
    Require(sceVideoOutConfigureOutput(handle, MODE_DEFAULT, &options, nullptr, 1) == INVALID_VALUE);

    Require(sceVideoOutSetFlipRate(handle, 0) == 0);
    Require(sceVideoOutSetFlipRate(handle, 1) == 0);
    Require(sceVideoOutSetFlipRate(handle, 2) == 0);
    Require(sceVideoOutSetFlipRate(handle, -1) == INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(handle, 3) == INVALID_VALUE);

    Require(sceVideoOutSetWindowModeMargins(handle, 0, 0) == 0);
    Require(sceVideoOutSetWindowModeMargins(handle, 10, 20) == 0);
    Require(sceVideoOutLatencyControlWaitBeforeInput(handle) == 0);
    Require(sceVideoOutLatencyMeasureSetStartPoint(handle, 0) == 0);
    Require(sceVideoOutAddVrrStatusFlagsPrivilege() == 0);
    Require(sceVideoOutVrrPegToFixedRate(handle, 0, 0) == 0);
    Require(sceVideoOutVrrUnpegFromFixedRate(handle) == 0);

    Require(sceVideoOutClose(handle) == 0);
    Require(sceVideoOutClose(handle) == INVALID_HANDLE);

    bool caughtPostCloseRes = false;
    try {
        sceVideoOutGetResolutionStatus(handle, &resStatus);
    } catch (const std::runtime_error&) {
        caughtPostCloseRes = true;
    }
    Require(caughtPostCloseRes);

    Require(sceVideoOutIsOutputSupported(handle, MODE_DEFAULT, &options, nullptr, 0) == INVALID_HANDLE);
    Require(sceVideoOutConfigureOutput(handle, MODE_DEFAULT, &options, nullptr, 0) == INVALID_HANDLE);
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, &cap, sizeof(cap)) == INVALID_HANDLE);

    LibcRunShutdown_nid_postfix();
    return 0;
}
