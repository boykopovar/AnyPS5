#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;
static constexpr int NEVER_OPENED_HANDLE = 2;
static constexpr int NO_BUFFERS_REGISTERED = static_cast<int>(0x8029001Cu);
static constexpr std::uint64_t TILING_MODE_LINEAR = 0;
static constexpr std::uint64_t PIXEL_FORMAT = 0x8000000022000000ull;

static void Require(bool value) { if (!value) std::abort(); }

static bool Rejects(int handle, VideoOutResolutionStatus* status) {
    try {
        sceVideoOutGetResolutionStatus(handle, status);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool RejectsOutputStatus(int handle, VideoOutOutputStatus* status) {
    try {
        sceVideoOutGetOutputStatus(handle, status);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
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

    std::array<std::byte, 96> fresh;
    fresh.fill(std::byte{0xff});
    Require(sceVideoOutGetOutputStatusWithVrr(handle, fresh.data()) == NO_BUFFERS_REGISTERED);
    for (const auto byte : fresh) Require(byte == std::byte{0xff});
    std::vector<std::byte> allocation(16u * 1024u * 1024u);
    const auto aligned = (reinterpret_cast<std::uintptr_t>(allocation.data()) + 65535u) & ~static_cast<std::uintptr_t>(65535u);
    VideoOutBuffers buffers{reinterpret_cast<void*>(aligned), nullptr, {nullptr, nullptr}};
    VideoOutBufferAttribute2 attribute{};
    attribute.width = 1920;
    attribute.height = 1080;
    attribute.pixel_format = PIXEL_FORMAT;
    attribute.tiling_mode = TILING_MODE_LINEAR;
    Require(sceVideoOutRegisterBuffers2(handle, 0, 0, &buffers, 1, &attribute, 0, nullptr) == 0);

    VideoOutResolutionStatus status;
    std::memset(&status, 0xff, sizeof(status));
    Require(sceVideoOutGetResolutionStatus(handle, &status) == 0);
    Require(status.fullWidth == 1920 && status.fullHeight == 1080);
    Require(status.paneWidth == status.fullWidth && status.paneHeight == status.fullHeight);
    Require(status.screenSizeInInch == 0.0f && status.flags == 0 && status.reserved0 == 0);
    Require(status.reserved1[0] == 0 && status.reserved1[1] == 0 && status.reserved1[2] == 0);

    VideoOutOutputStatus output{};
    Require(sceVideoOutGetOutputStatus(handle, &output) == 0);
    Require(status.refreshRate == output.refreshRate);
    Require(status.refreshRate == 3 || status.refreshRate == 13);
    Require(output.refreshRate == 3 || output.refreshRate == 13);
    Require(output.resolution == 1 || output.resolution == 2);
    Require(output.dynamicRange == 1 || output.dynamicRange == 2);
    Require(output.flags == 0);
    Require(output.reserved[0] == 0 && output.reserved[1] == 0 && output.reserved[2] == 0);

    Require(sceVideoOutConfigureOutput(handle, 1, nullptr, nullptr, 0) == 0);
    Require(sceVideoOutGetResolutionStatus(handle, &status) == 0);
    Require(status.fullWidth == 1920 && status.fullHeight == 1080);
    Require(status.paneWidth == status.fullWidth && status.paneHeight == status.fullHeight);
    Require(status.refreshRate == 3);
    Require(sceVideoOutGetOutputStatus(handle, &output) == 0);
    Require(output.refreshRate == 3);

    std::array<std::byte, 96> vrr;
    vrr.fill(std::byte{0xff});
    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrr.data()) == 0);
    Require(std::memcmp(vrr.data(), &output, sizeof(output)) == 0);
    for (std::size_t offset = sizeof(output); offset < 80; ++offset) Require(vrr[offset] == std::byte{0});
    for (std::size_t offset = 80; offset < vrr.size(); ++offset) Require(vrr[offset] == std::byte{0xff});
    vrr.fill(std::byte{0xff});
    Require(sceVideoOutGetOutputStatusWithVrr(handle, nullptr) == static_cast<int>(0x80290002u));
    Require(sceVideoOutGetOutputStatusWithVrr(NEVER_OPENED_HANDLE, vrr.data()) == static_cast<int>(0x8029000Bu));
    Require(sceVideoOutGetOutputStatusWithVrr(-1, vrr.data()) == static_cast<int>(0x8029000Bu));
    for (const auto byte : vrr) Require(byte == std::byte{0xff});

    Require(Rejects(handle, nullptr));
    Require(Rejects(-1, nullptr));
    Require(Rejects(0, &status));
    Require(Rejects(-1, &status));
    Require(Rejects(NEVER_OPENED_HANDLE, &status));

    Require(RejectsOutputStatus(handle, nullptr));
    Require(RejectsOutputStatus(-1, nullptr));
    Require(RejectsOutputStatus(0, &output));
    Require(RejectsOutputStatus(-1, &output));
    Require(RejectsOutputStatus(NEVER_OPENED_HANDLE, &output));

    Require(sceVideoOutClose(handle) == 0);
    Require(Rejects(handle, &status));
    Require(RejectsOutputStatus(handle, &output));
    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrr.data()) == static_cast<int>(0x8029000Bu));
    LibcRunShutdown_nid_postfix();
}
