#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/InventoryExports.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <vector>

static_assert(sizeof(VideoOutResolutionStatus) == 48);
static_assert(offsetof(VideoOutResolutionStatus, fullWidth) == 0);
static_assert(offsetof(VideoOutResolutionStatus, fullHeight) == 4);
static_assert(offsetof(VideoOutResolutionStatus, paneWidth) == 8);
static_assert(offsetof(VideoOutResolutionStatus, paneHeight) == 12);
static_assert(offsetof(VideoOutResolutionStatus, refreshRate) == 16);
static_assert(offsetof(VideoOutResolutionStatus, screenSizeInInch) == 24);
static_assert(offsetof(VideoOutResolutionStatus, flags) == 28);
static_assert(offsetof(VideoOutResolutionStatus, reserved0) == 30);
static_assert(offsetof(VideoOutResolutionStatus, reserved1) == 32);

static_assert(sizeof(VideoOutOutputStatus) == 48);
static_assert(offsetof(VideoOutOutputStatus, resolution) == 0);
static_assert(offsetof(VideoOutOutputStatus, dynamicRange) == 4);
static_assert(offsetof(VideoOutOutputStatus, refreshRate) == 8);
static_assert(offsetof(VideoOutOutputStatus, flags) == 16);
static_assert(offsetof(VideoOutOutputStatus, reserved) == 24);

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;
static constexpr int VIDEO_OUT_ERROR_INVALID_ADDRESS = static_cast<int>(0x80290002u);
static constexpr int VIDEO_OUT_ERROR_INVALID_HANDLE = static_cast<int>(0x8029000Bu);
static constexpr int VIDEO_OUT_ERROR_NO_BUFFERS_REGISTERED = static_cast<int>(0x8029001Cu);
static constexpr std::uint64_t TILING_MODE_LINEAR = 0;
static constexpr std::uint64_t PIXEL_FORMAT = 0x8000000022000000ull;

static void Require(bool condition, std::source_location loc = std::source_location::current()) {
    if (!condition) {
        std::printf("Require failed at %s:%u\n", loc.file_name(), loc.line());
        std::abort();
    }
}

static bool ThrowsInvalidResolutionAddress(int handle) {
    try {
        sceVideoOutGetResolutionStatus(handle, nullptr);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool ThrowsInvalidResolutionHandle(int handle, VideoOutResolutionStatus* status) {
    try {
        sceVideoOutGetResolutionStatus(handle, status);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool ThrowsInvalidOutputAddress(int handle) {
    try {
        sceVideoOutGetOutputStatus(handle, nullptr);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool ThrowsInvalidOutputHandle(int handle, VideoOutOutputStatus* status) {
    try {
        sceVideoOutGetOutputStatus(handle, status);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool ThrowsNotImplementedSysResolutionStatus() {
    try {
        sceVideoOutSysGetResolutionStatus();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool ThrowsNotImplementedSysResolutionStatus2() {
    try {
        sceVideoOutSysGetResolutionStatus2();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int main() {
    VideoOutTestEnvironment environment;

    Require(ThrowsNotImplementedSysResolutionStatus());
    Require(ThrowsNotImplementedSysResolutionStatus2());

    VideoOutResolutionStatus unopenedStatus{};
    for (int unopened = 0; unopened <= 8; ++unopened) {
        Require(ThrowsInvalidResolutionHandle(unopened, &unopenedStatus));
    }
    Require(ThrowsInvalidResolutionHandle(-1, &unopenedStatus));
    Require(ThrowsInvalidResolutionHandle(9999, &unopenedStatus));
    Require(ThrowsInvalidResolutionAddress(0));
    Require(ThrowsInvalidResolutionAddress(-1));
    Require(ThrowsInvalidResolutionAddress(42));

    VideoOutOutputStatus unopenedOutput{};
    for (int unopened = 0; unopened <= 8; ++unopened) {
        Require(ThrowsInvalidOutputHandle(unopened, &unopenedOutput));
    }
    Require(ThrowsInvalidOutputHandle(-1, &unopenedOutput));
    Require(ThrowsInvalidOutputHandle(9999, &unopenedOutput));
    Require(ThrowsInvalidOutputAddress(0));
    Require(ThrowsInvalidOutputAddress(-1));
    Require(ThrowsInvalidOutputAddress(42));

    std::array<std::byte, 128> vrrCanaryBuffer{};
    vrrCanaryBuffer.fill(std::byte{0x7A});
    for (int unopened = 0; unopened <= 8; ++unopened) {
        Require(sceVideoOutGetOutputStatusWithVrr(unopened, vrrCanaryBuffer.data()) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    }
    Require(sceVideoOutGetOutputStatusWithVrr(-1, vrrCanaryBuffer.data()) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutGetOutputStatusWithVrr(9999, vrrCanaryBuffer.data()) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    for (const auto byte : vrrCanaryBuffer) {
        Require(byte == std::byte{0x7A});
    }

    Require(sceVideoOutGetOutputStatusWithVrr(0, nullptr) == VIDEO_OUT_ERROR_INVALID_ADDRESS);
    Require(sceVideoOutGetOutputStatusWithVrr(-1, nullptr) == VIDEO_OUT_ERROR_INVALID_ADDRESS);
    Require(sceVideoOutGetOutputStatusWithVrr(42, nullptr) == VIDEO_OUT_ERROR_INVALID_ADDRESS);

    int handle = 0;
    try {
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(handle > 0);

    Require(ThrowsInvalidResolutionAddress(handle));
    Require(ThrowsInvalidOutputAddress(handle));
    Require(sceVideoOutGetOutputStatusWithVrr(handle, nullptr) == VIDEO_OUT_ERROR_INVALID_ADDRESS);

    std::array<std::byte, 128> freshVrrBuffer{};
    freshVrrBuffer.fill(std::byte{0xDE});
    Require(sceVideoOutGetOutputStatusWithVrr(handle, freshVrrBuffer.data()) == VIDEO_OUT_ERROR_NO_BUFFERS_REGISTERED);
    for (const auto byte : freshVrrBuffer) {
        Require(byte == std::byte{0xDE});
    }

    std::vector<std::byte> allocation(16u * 1024u * 1024u);
    const auto aligned = (reinterpret_cast<std::uintptr_t>(allocation.data()) + 65535u) & ~static_cast<std::uintptr_t>(65535u);
    VideoOutBuffers buffers{reinterpret_cast<void*>(aligned), nullptr, {nullptr, nullptr}};
    VideoOutBufferAttribute2 attribute{};
    attribute.width = 1920;
    attribute.height = 1080;
    attribute.pixel_format = PIXEL_FORMAT;
    attribute.tiling_mode = TILING_MODE_LINEAR;
    Require(sceVideoOutRegisterBuffers2(handle, 0, 0, &buffers, 1, &attribute, 0, nullptr) == 0);

    struct ResolutionCanaryEnvelope {
        VideoOutResolutionStatus status;
        std::uint8_t canary[32];
    };
    ResolutionCanaryEnvelope resEnvelope{};
    std::memset(&resEnvelope, 0x5C, sizeof(resEnvelope));
    Require(sceVideoOutGetResolutionStatus(handle, &resEnvelope.status) == 0);

    Require(resEnvelope.status.fullWidth > 0);
    Require(resEnvelope.status.fullHeight > 0);
    Require(resEnvelope.status.paneWidth == resEnvelope.status.fullWidth);
    Require(resEnvelope.status.paneHeight == resEnvelope.status.fullHeight);
    Require(resEnvelope.status.screenSizeInInch == 0.0f);
    Require(resEnvelope.status.flags == 0);
    Require(resEnvelope.status.reserved0 == 0);
    Require(resEnvelope.status.reserved1[0] == 0);
    Require(resEnvelope.status.reserved1[1] == 0);
    Require(resEnvelope.status.reserved1[2] == 0);

    for (std::size_t i = 0; i < sizeof(resEnvelope.canary); ++i) {
        Require(resEnvelope.canary[i] == 0x5C);
    }

    struct OutputCanaryEnvelope {
        VideoOutOutputStatus output;
        std::uint8_t canary[32];
    };
    OutputCanaryEnvelope outEnvelope{};
    std::memset(&outEnvelope, 0x6D, sizeof(outEnvelope));
    Require(sceVideoOutGetOutputStatus(handle, &outEnvelope.output) == 0);

    Require(resEnvelope.status.refreshRate == outEnvelope.output.refreshRate);
    Require(outEnvelope.output.flags == 0);
    Require(outEnvelope.output.reserved[0] == 0);
    Require(outEnvelope.output.reserved[1] == 0);
    Require(outEnvelope.output.reserved[2] == 0);

    for (std::size_t i = 0; i < sizeof(outEnvelope.canary); ++i) {
        Require(outEnvelope.canary[i] == 0x6D);
    }

    std::array<std::byte, 128> vrrBuffer{};
    vrrBuffer.fill(std::byte{0xDE});
    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrrBuffer.data()) == 0);

    Require(std::memcmp(vrrBuffer.data(), &outEnvelope.output, sizeof(outEnvelope.output)) == 0);

    for (std::size_t offset = sizeof(VideoOutOutputStatus); offset < 80; ++offset) {
        Require(vrrBuffer[offset] == std::byte{0});
    }

    for (std::size_t offset = 80; offset < vrrBuffer.size(); ++offset) {
        Require(vrrBuffer[offset] == std::byte{0xDE});
    }

    std::array<std::byte, 128> vrrPatternBuffer{};
    for (std::size_t i = 0; i < vrrPatternBuffer.size(); ++i) {
        vrrPatternBuffer[i] = static_cast<std::byte>(i & 0xFF);
    }
    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrrPatternBuffer.data()) == 0);
    Require(std::memcmp(vrrPatternBuffer.data(), &outEnvelope.output, sizeof(outEnvelope.output)) == 0);
    for (std::size_t offset = sizeof(VideoOutOutputStatus); offset < 80; ++offset) {
        Require(vrrPatternBuffer[offset] == std::byte{0});
    }
    for (std::size_t offset = 80; offset < vrrPatternBuffer.size(); ++offset) {
        Require(vrrPatternBuffer[offset] == static_cast<std::byte>(offset & 0xFF));
    }

    Require(sceVideoOutClose(handle) == 0);

    Require(ThrowsInvalidResolutionHandle(handle, &resEnvelope.status));
    Require(ThrowsInvalidOutputHandle(handle, &outEnvelope.output));

    vrrBuffer.fill(std::byte{0xEF});
    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrrBuffer.data()) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    for (const auto byte : vrrBuffer) {
        Require(byte == std::byte{0xEF});
    }

    LibcRunShutdown_nid_postfix();
    return 0;
}
