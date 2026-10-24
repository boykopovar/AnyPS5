#include "SceTypes.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

extern "C" {
int APS5_VABI sceVideoOutOpen(int userId, int busType, int index, const void* param);
int APS5_VABI sceVideoOutClose(int handle);
int APS5_VABI sceVideoOutGetDeviceCapabilityInfo_(int handle, void* info, std::uint64_t size);
void APS5_VABI sceVideoOutModeSetAny_(void* mode, std::uint32_t size);
}

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;
static constexpr int INVALID_VALUE = static_cast<int>(0x80290001);
static constexpr int INVALID_HANDLE = static_cast<int>(0x8029000B);

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    VideoOutTestEnvironment environment;
    std::array<std::uint8_t, 64> mode{};
    mode.fill(0xAA);
    sceVideoOutModeSetAny_(mode.data(), 16);
    Require(std::memcmp(mode.data(), "\x10\x00\x00\x00", 4) == 0);
    for (std::size_t index = 4; index < 16; ++index) Require(mode[index] == 0xFF);
    for (std::size_t index = 16; index < mode.size(); ++index) Require(mode[index] == 0xAA);
    mode.fill(0xAA);
    sceVideoOutModeSetAny_(mode.data(), 0);
    Require(std::memcmp(mode.data(), "\x00\x00\x00\x00", 4) == 0 && mode[4] == 0xAA);

    std::array<std::uint8_t, 16> info{};
    info.fill(0xAA);
    Require(sceVideoOutGetDeviceCapabilityInfo_(-1, info.data(), 8) == INVALID_HANDLE);
    Require(sceVideoOutGetDeviceCapabilityInfo_(0x7FFFFFFF, info.data(), 8) == INVALID_HANDLE);

    int handle = 0;
    try {
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(handle > 0);
    for (std::uint64_t size : {0ull, 4ull, 16ull}) {
        Require(sceVideoOutGetDeviceCapabilityInfo_(handle, info.data(), size) == INVALID_VALUE);
        Require(info[0] == 0xAA);
    }
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, nullptr, 8) == INVALID_VALUE);
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, nullptr, 0) == INVALID_VALUE);
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, info.data(), 8) == 0);
    Require(std::memcmp(info.data(), "\x01\x00\x00\x00\x00\x00\x00\x00", 8) == 0 && info[8] == 0xAA);
    Require(sceVideoOutClose(handle) == 0);
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, info.data(), 8) == INVALID_HANDLE);
    return 0;
}
