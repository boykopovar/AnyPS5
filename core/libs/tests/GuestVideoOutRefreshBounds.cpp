#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <stdexcept>

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;
static constexpr int HMD2_MAIN_BUS = 0x20;
static constexpr int UNOPENED_HANDLE = 2;

static constexpr int INVALID_VALUE = static_cast<int>(0x80290001);
static constexpr int INVALID_ADDRESS = static_cast<int>(0x80290002);
static constexpr int RESOURCE_BUSY = static_cast<int>(0x80290009);
static constexpr int INVALID_HANDLE = static_cast<int>(0x8029000B);
static constexpr int UNSUPPORTED_OUTPUT_MODE = static_cast<int>(0x80290016);
static constexpr int UNAVAILABLE_OUTPUT_MODE = static_cast<int>(0x80290019);
static constexpr int UNKNOWN_OUTPUT_MODE = static_cast<int>(0x8029001E);

static constexpr uint64_t DEFAULT_OUTPUT_MODE = 1;
static constexpr uint64_t HIGH_FRAME_RATE_OUTPUT_MODE = 15;
static constexpr uint64_t REFRESH_RATE_59_94HZ = 3;
static constexpr uint64_t REFRESH_RATE_119_88HZ = 13;

static void Require(bool value) {
    if (!value) std::abort();
}

static bool Throws(auto&& function) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int main() {
    VideoOutTestEnvironment environment;
    {
        std::ofstream param("app0/sce_sys/param.json", std::ios::binary);
        param << R"({"titleId":"PPSA00000","localizedParameters":{"en-US":{"titleName":"RefreshBounds"}},"attribute3":64})";
        Require(static_cast<bool>(param));
    }

    VideoOutOutputOptions options{};
    Require(sceVideoOutInitializeOutputOptions(&options) == 0);
    Require(sceVideoOutInitializeOutputOptions(nullptr) == INVALID_ADDRESS);

    for (int rate : {0, 30, 50, 59, 60, 75, 90, 100, 119, 120, 144, 165, 240, 360}) {
        Require(ValidateVideoOutOutputMode_nid_no_patch(DEFAULT_OUTPUT_MODE, &options, rate) == 0);
        Require(VideoOutOutputSupport_nid_no_patch(DEFAULT_OUTPUT_MODE, &options, rate) == 1);
    }

    for (int rate : {0, 24, 30, 50, 59, 60, 75, 90, 100, 119}) {
        Require(ValidateVideoOutOutputMode_nid_no_patch(HIGH_FRAME_RATE_OUTPUT_MODE, &options, rate) == UNSUPPORTED_OUTPUT_MODE);
        Require(VideoOutOutputSupport_nid_no_patch(HIGH_FRAME_RATE_OUTPUT_MODE, &options, rate) == 0);
    }

    for (int rate : {120, 144, 165, 240, 360}) {
        Require(ValidateVideoOutOutputMode_nid_no_patch(HIGH_FRAME_RATE_OUTPUT_MODE, &options, rate) == 0);
        Require(VideoOutOutputSupport_nid_no_patch(HIGH_FRAME_RATE_OUTPUT_MODE, &options, rate) == 1);
    }

    for (uint64_t unsupportedMode : {0x4ull, 0x7ull, 0x8ull, 0xCull, 0xDull, 0xEull, 0x10ull, 0x11ull, 0x13ull}) {
        Require(ValidateVideoOutOutputMode_nid_no_patch(unsupportedMode, &options, 60) == UNSUPPORTED_OUTPUT_MODE);
        Require(ValidateVideoOutOutputMode_nid_no_patch(unsupportedMode, &options, 120) == UNSUPPORTED_OUTPUT_MODE);
        Require(VideoOutOutputSupport_nid_no_patch(unsupportedMode, &options, 60) == UNSUPPORTED_OUTPUT_MODE);
        Require(VideoOutOutputSupport_nid_no_patch(unsupportedMode, &options, 120) == UNSUPPORTED_OUTPUT_MODE);
    }

    for (uint64_t unknownMode : {0ull, 2ull, 3ull, 5ull, 6ull, 9ull, 10ull, 11ull, 20ull, 24ull, 30ull, 50ull, 60ull, 75ull, 90ull, 100ull, 144ull, 165ull, 240ull, 0x100ull, 0xD000000Aull, ~0ull}) {
        Require(ValidateVideoOutOutputMode_nid_no_patch(unknownMode, &options, 60) == UNKNOWN_OUTPUT_MODE);
        Require(ValidateVideoOutOutputMode_nid_no_patch(unknownMode, &options, 120) == UNKNOWN_OUTPUT_MODE);
        Require(VideoOutOutputSupport_nid_no_patch(unknownMode, &options, 60) == UNKNOWN_OUTPUT_MODE);
        Require(VideoOutOutputSupport_nid_no_patch(unknownMode, &options, 120) == UNKNOWN_OUTPUT_MODE);
    }

    Require(Throws([&] { ValidateVideoOutOutputMode_nid_no_patch(0x12, &options, 60); }));

    for (int invalidPort : {1, 2, 3, -1, 100}) {
        Require(Throws([&] { sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, invalidPort, nullptr); }));
        Require(Throws([&] { sceVideoOutOpen(0, MAIN_BUS, invalidPort, nullptr); }));
    }

    Require(sceVideoOutOpen(SYSTEM_USER, HMD2_MAIN_BUS, 0, nullptr) == INVALID_VALUE);
    for (int invalidBus : {-1, 3, 4, 10}) {
        Require(Throws([&] { sceVideoOutOpen(SYSTEM_USER, invalidBus, 0, nullptr); }));
    }

    for (int invalidUser : {-1, 1, 2, 42}) {
        Require(Throws([&] { sceVideoOutOpen(invalidUser, MAIN_BUS, 0, nullptr); }));
    }

    VideoOutResolutionStatus dummyResolution{};
    VideoOutOutputStatus dummyOutput{};
    std::array<std::byte, 96> dummyVrr{};
    std::uint64_t dummyCapability = 0;

    for (int unopened : {-1, 0, UNOPENED_HANDLE, 0x7FFFFFFF}) {
        Require(Throws([&] { sceVideoOutGetResolutionStatus(unopened, &dummyResolution); }));
        Require(Throws([&] { sceVideoOutGetOutputStatus(unopened, &dummyOutput); }));
        Require(sceVideoOutGetOutputStatusWithVrr(unopened, dummyVrr.data()) == INVALID_HANDLE);
        Require(sceVideoOutGetDeviceCapabilityInfo_(unopened, &dummyCapability, sizeof(dummyCapability)) == INVALID_HANDLE);
        Require(sceVideoOutIsOutputSupported(unopened, DEFAULT_OUTPUT_MODE, &options, nullptr, 0) == INVALID_HANDLE);
        Require(sceVideoOutConfigureOutput(unopened, DEFAULT_OUTPUT_MODE, &options, nullptr, 0) == INVALID_HANDLE);
        Require(Throws([&] { sceVideoOutSetFlipRate(unopened, 0); }));
        Require(Throws([&] { sceVideoOutSetWindowModeMargins(unopened, 0, 0); }));
        Require(Throws([&] { sceVideoOutAllowOutputResolutionWqhdDetection(unopened); }));
        Require(sceVideoOutClose(unopened) == INVALID_HANDLE);
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
    Require(sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr) == RESOURCE_BUSY);

    Require(sceVideoOutSetFlipRate(handle, 0) == 0);
    Require(sceVideoOutSetFlipRate(handle, 1) == 0);
    Require(sceVideoOutSetFlipRate(handle, 2) == 0);
    for (int invalidRate : {-60, -1, 3, 4, 30, 60, 75, 120, 144}) {
        Require(sceVideoOutSetFlipRate(handle, invalidRate) == INVALID_VALUE);
    }

    for (uint64_t mode : {0x4ull, 0x7ull, 0x8ull, 0xCull, 0xDull, 0xEull, 0x10ull, 0x11ull, 0x13ull}) {
        Require(sceVideoOutIsOutputSupported(handle, mode, &options, nullptr, 0) == UNSUPPORTED_OUTPUT_MODE);
        Require(sceVideoOutConfigureOutput(handle, mode, &options, nullptr, 0) == UNSUPPORTED_OUTPUT_MODE);
    }

    for (uint64_t mode : {0ull, 2ull, 3ull, 5ull, 6ull, 9ull, 10ull, 11ull, 14ull, 16ull, 20ull, 24ull, 30ull, 50ull, 60ull, 75ull, 90ull, 100ull, 144ull, 165ull, 240ull, 0x100ull, 0xD000000Aull, ~0ull}) {
        Require(sceVideoOutIsOutputSupported(handle, mode, &options, nullptr, 0) == UNKNOWN_OUTPUT_MODE);
        Require(sceVideoOutConfigureOutput(handle, mode, &options, nullptr, 0) == UNKNOWN_OUTPUT_MODE);
    }

    Require(sceVideoOutIsOutputSupported(handle, DEFAULT_OUTPUT_MODE, &options, &options, 0) == INVALID_VALUE);
    Require(sceVideoOutIsOutputSupported(handle, DEFAULT_OUTPUT_MODE, &options, nullptr, 1) == INVALID_VALUE);
    Require(sceVideoOutConfigureOutput(handle, DEFAULT_OUTPUT_MODE, &options, &options, 0) == INVALID_VALUE);
    Require(sceVideoOutConfigureOutput(handle, DEFAULT_OUTPUT_MODE, &options, nullptr, 1) == INVALID_VALUE);

    for (std::size_t word = 0; word < 16; ++word) {
        VideoOutOutputOptions corrupted = options;
        corrupted.internalData[word] ^= 1;
        Require(sceVideoOutIsOutputSupported(handle, DEFAULT_OUTPUT_MODE, &corrupted, nullptr, 0) == (word == 3 ? 1 : INVALID_VALUE));
        Require(sceVideoOutConfigureOutput(handle, DEFAULT_OUTPUT_MODE, &corrupted, nullptr, 0) == (word == 3 ? 0 : INVALID_VALUE));
    }

    Require(sceVideoOutIsOutputSupported(handle, DEFAULT_OUTPUT_MODE, &options, nullptr, 0) == 1);
    Require(sceVideoOutConfigureOutput(handle, DEFAULT_OUTPUT_MODE, &options, nullptr, 0) == 0);

    VideoOutOutputStatus output{};
    VideoOutResolutionStatus resolution{};
    Require(sceVideoOutGetOutputStatus(handle, &output) == 0);
    Require(sceVideoOutGetResolutionStatus(handle, &resolution) == 0);
    Require(resolution.fullWidth == 1920 && resolution.fullHeight == 1080);
    Require(resolution.paneWidth == resolution.fullWidth && resolution.paneHeight == resolution.fullHeight);
    Require(output.refreshRate == REFRESH_RATE_59_94HZ);
    Require(resolution.refreshRate == REFRESH_RATE_59_94HZ);

    std::array<std::byte, 96> vrr{};
    vrr.fill(std::byte{0xFF});
    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrr.data()) == 0);
    Require(std::memcmp(vrr.data(), &output, sizeof(output)) == 0);
    for (std::size_t offset = sizeof(output); offset < 80; ++offset) Require(vrr[offset] == std::byte{0});
    for (std::size_t offset = 80; offset < vrr.size(); ++offset) Require(vrr[offset] == std::byte{0xFF});
    Require(sceVideoOutGetOutputStatusWithVrr(handle, nullptr) == INVALID_ADDRESS);

    std::uint64_t capability = 0;
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, &capability, sizeof(capability)) == 0);
    Require(capability == 1);
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, nullptr, sizeof(capability)) == INVALID_VALUE);
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, &capability, 4) == INVALID_VALUE);

    Require(sceVideoOutAllowOutputResolutionWqhdDetection(handle) == 0);
    Require(sceVideoOutSetWindowModeMargins(handle, 0, 0) == 0);

    const int hfrSupported = sceVideoOutIsOutputSupported(handle, HIGH_FRAME_RATE_OUTPUT_MODE, &options, nullptr, 0);
    Require(hfrSupported == 0 || hfrSupported == 1);

    if (hfrSupported == 1) {
        Require(sceVideoOutConfigureOutput(handle, HIGH_FRAME_RATE_OUTPUT_MODE, &options, nullptr, 0) == 0);
        Require(sceVideoOutGetOutputStatus(handle, &output) == 0);
        Require(sceVideoOutGetResolutionStatus(handle, &resolution) == 0);
        Require(output.refreshRate == REFRESH_RATE_119_88HZ);
        Require(resolution.refreshRate == REFRESH_RATE_119_88HZ);

        Require(sceVideoOutConfigureOutput(handle, DEFAULT_OUTPUT_MODE, &options, nullptr, 0) == 0);
        Require(sceVideoOutGetOutputStatus(handle, &output) == 0);
        Require(sceVideoOutGetResolutionStatus(handle, &resolution) == 0);
        Require(output.refreshRate == REFRESH_RATE_59_94HZ);
        Require(resolution.refreshRate == REFRESH_RATE_59_94HZ);
    } else {
        Require(sceVideoOutConfigureOutput(handle, HIGH_FRAME_RATE_OUTPUT_MODE, &options, nullptr, 0) == UNAVAILABLE_OUTPUT_MODE);
    }

    Require(sceVideoOutClose(handle) == 0);
    Require(sceVideoOutClose(handle) == INVALID_HANDLE);

    Require(Throws([&] { sceVideoOutGetResolutionStatus(handle, &resolution); }));
    Require(Throws([&] { sceVideoOutGetOutputStatus(handle, &output); }));
    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrr.data()) == INVALID_HANDLE);
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, &capability, sizeof(capability)) == INVALID_HANDLE);
    Require(sceVideoOutIsOutputSupported(handle, DEFAULT_OUTPUT_MODE, &options, nullptr, 0) == INVALID_HANDLE);
    Require(sceVideoOutConfigureOutput(handle, DEFAULT_OUTPUT_MODE, &options, nullptr, 0) == INVALID_HANDLE);
    Require(Throws([&] { sceVideoOutSetFlipRate(handle, 0); }));
    Require(Throws([&] { sceVideoOutSetWindowModeMargins(handle, 0, 0); }));
    Require(Throws([&] { sceVideoOutAllowOutputResolutionWqhdDetection(handle); }));

    LibcRunShutdown_nid_postfix();
    return 0;
}
