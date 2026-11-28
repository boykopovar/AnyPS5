#include "SceTypes.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;
static constexpr int INVALID_VALUE = static_cast<int>(0x80290001);
static constexpr int INVALID_HANDLE = static_cast<int>(0x8029000B);

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    VideoOutTestEnvironment environment;

    for (std::uint32_t sz : {0u, 1u, 4u, 8u, 16u, 64u}) {
        try {
            sceVideoOutModeSetAny_(nullptr, sz);
            Require(false);
        } catch (const std::runtime_error&) {}
    }

    std::array<std::uint8_t, 256> modeBuffer{};
    const std::uint8_t canaries[] = {0xAA, 0x55, 0x00, 0x77};
    for (const auto canary : canaries) {
        modeBuffer.fill(canary);
        sceVideoOutModeSetAny_(modeBuffer.data(), 0);
        Require(std::memcmp(modeBuffer.data(), "\x00\x00\x00\x00", 4) == 0);
        for (std::size_t i = 4; i < modeBuffer.size(); ++i) {
            Require(modeBuffer[i] == canary);
        }

        for (std::uint32_t sz = 1; sz <= 4; ++sz) {
            modeBuffer.fill(canary);
            sceVideoOutModeSetAny_(modeBuffer.data(), sz);
            std::uint32_t written = 0;
            std::memcpy(&written, modeBuffer.data(), sizeof(written));
            Require(written == sz);
            for (std::size_t i = 4; i < modeBuffer.size(); ++i) {
                Require(modeBuffer[i] == canary);
            }
        }

        for (std::uint32_t sz : {5u, 8u, 12u, 16u, 32u, 64u, 128u}) {
            modeBuffer.fill(canary);
            sceVideoOutModeSetAny_(modeBuffer.data(), sz);
            std::uint32_t written = 0;
            std::memcpy(&written, modeBuffer.data(), sizeof(written));
            Require(written == sz);
            for (std::size_t i = 4; i < sz; ++i) {
                Require(modeBuffer[i] == 0xFF);
            }
            for (std::size_t i = sz; i < modeBuffer.size(); ++i) {
                Require(modeBuffer[i] == canary);
            }
        }
    }

    std::array<std::uint8_t, 32> capBuffer{};
    capBuffer.fill(0x3C);
    const int unopenedHandles[] = {-1, 0, 1, 2, 3, 42, 9999, 0x7FFFFFFF};
    for (const int unopened : unopenedHandles) {
        Require(sceVideoOutGetDeviceCapabilityInfo_(unopened, capBuffer.data(), 8) == INVALID_HANDLE);
        Require(sceVideoOutGetDeviceCapabilityInfo_(unopened, capBuffer.data(), 0) == INVALID_HANDLE);
        Require(sceVideoOutGetDeviceCapabilityInfo_(unopened, capBuffer.data(), 4) == INVALID_HANDLE);
        Require(sceVideoOutGetDeviceCapabilityInfo_(unopened, capBuffer.data(), 16) == INVALID_HANDLE);
        Require(sceVideoOutGetDeviceCapabilityInfo_(unopened, nullptr, 8) == INVALID_HANDLE);
        Require(sceVideoOutGetDeviceCapabilityInfo_(unopened, nullptr, 0) == INVALID_HANDLE);
        for (const auto b : capBuffer) {
            Require(b == 0x3C);
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

    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, nullptr, 8) == INVALID_VALUE);
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, nullptr, 0) == INVALID_VALUE);
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, nullptr, 16) == INVALID_VALUE);
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, nullptr, ~0ull) == INVALID_VALUE);

    const std::uint64_t invalidSizes[] = {0ull, 1ull, 2ull, 3ull, 4ull, 5ull, 6ull, 7ull, 9ull, 10ull, 12ull, 16ull, 24ull, 32ull, 64ull, 128ull, ~0ull};
    for (const auto sz : invalidSizes) {
        capBuffer.fill(0x9E);
        Require(sceVideoOutGetDeviceCapabilityInfo_(handle, capBuffer.data(), sz) == INVALID_VALUE);
        for (const auto b : capBuffer) {
            Require(b == 0x9E);
        }
    }

    capBuffer.fill(0x9E);
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, capBuffer.data(), 8) == 0);
    Require(std::memcmp(capBuffer.data(), "\x01\x00\x00\x00\x00\x00\x00\x00", 8) == 0);
    for (std::size_t i = 8; i < capBuffer.size(); ++i) {
        Require(capBuffer[i] == 0x9E);
    }

    VideoOutColorSettings colorSettings{};
    for (uint32_t sz = 0; sz < sizeof(VideoOutColorSettings); ++sz) {
        try {
            sceVideoOutColorSettingsSetGamma_(&colorSettings, 1.2f, sz);
            Require(false);
        } catch (const std::runtime_error&) {}
    }
    Require(sceVideoOutColorSettingsSetGamma_(&colorSettings, 1.2f, sizeof(VideoOutColorSettings)) == 0);
    Require(colorSettings.gamma == 1.2f);

    VideoOutColorSpaceConversionSettings cscSettings{};
    for (uint32_t sz = 0; sz < sizeof(VideoOutColorSpaceConversionSettings); ++sz) {
        try {
            sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(&cscSettings, sz);
            Require(false);
        } catch (const std::runtime_error&) {}
        try {
            sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(&cscSettings, 1.0f, 0, sz);
            Require(false);
        } catch (const std::runtime_error&) {}
        try {
            sceVideoOutAdjustColorSpaceConversion_(handle, &cscSettings, sz);
            Require(false);
        } catch (const std::runtime_error&) {}
    }
    Require(sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(&cscSettings, sizeof(VideoOutColorSpaceConversionSettings)) == 0);
    Require(sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(&cscSettings, 1.0f, 0, sizeof(VideoOutColorSpaceConversionSettings)) == 0);
    Require(sceVideoOutAdjustColorSpaceConversion_(handle, &cscSettings, sizeof(VideoOutColorSpaceConversionSettings)) == 0);

    Require(sceVideoOutClose(handle) == 0);

    capBuffer.fill(0x7D);
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, capBuffer.data(), 8) == INVALID_HANDLE);
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, nullptr, 8) == INVALID_HANDLE);
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, capBuffer.data(), 0) == INVALID_HANDLE);
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, nullptr, 0) == INVALID_HANDLE);
    for (const auto b : capBuffer) {
        Require(b == 0x7D);
    }

    return 0;
}
