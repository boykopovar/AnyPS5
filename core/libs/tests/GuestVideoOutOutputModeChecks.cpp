#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/InventoryExports.hpp"
#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

constexpr int SystemUser = 255;
constexpr int MainBus = 0;
constexpr int InvalidValue = static_cast<int>(0x80290001u);
constexpr int InvalidAddress = static_cast<int>(0x80290002u);
constexpr int InvalidHandle = static_cast<int>(0x8029000Bu);
constexpr int UnsupportedOutputMode = static_cast<int>(0x80290016u);

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    std::abort();
}

bool ConfigureThrows(int handle, const std::array<std::uint8_t, 0x20>& mode, std::uint64_t setting) {
    const auto calls = NotImplementedCalls_nid_no_patch();
    bool threw = false;
    try {
        sceVideoOutConfigureOutputMode_(handle, 0, mode.data(), setting, 0x20, 0x10);
    } catch (const std::runtime_error& error) {
        threw = std::strcmp(error.what(), "sceVideoOutConfigureOutputMode_ not implemented") == 0;
    }
    return threw && NotImplementedCalls_nid_no_patch() == calls + 1;
}

std::array<std::uint8_t, 0x20> ModeRecord(std::uint32_t size) {
    std::array<std::uint8_t, 0x20> mode{};
    std::memcpy(mode.data(), &size, sizeof(size));
    return mode;
}

}

int main() {
    VideoOutTestEnvironment environment;
    std::array<std::uint8_t, 0x18> info{};
    const auto mode = ModeRecord(0x20);
    const int reserved = 0;

    Require(sceVideoOutGetDeviceInfoEx_(-1, nullptr, info.data(), 0x10) == InvalidValue, "a device info size other than 0x18 was not refused before the handle");
    Require(sceVideoOutGetDeviceInfoEx_(-1, &reserved, nullptr, 0x17) == InvalidValue, "a short device info size was not refused first");
    Require(sceVideoOutConfigureOutputMode_(-1, 0, nullptr, 0, 0, 0x10) == InvalidValue, "a null mode record was not refused before the handle");
    Require(sceVideoOutConfigureOutputMode_(-1, 1, mode.data(), 0, 0, 0x10) == InvalidValue, "a non-zero second argument was not refused before the handle");
    Require(sceVideoOutConfigureOutputMode_(-1, 0, mode.data(), 0, 0, 0x18) == InvalidValue, "a size other than 0x10 was not refused before the handle");

    int handle = 0;
    try {
        handle = sceVideoOutOpen(SystemUser, MainBus, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device for the open-port rows: %s\n", error.what());
        return 77;
    }
    Require(handle > 0, "sceVideoOutOpen failed");

    Require(sceVideoOutGetDeviceInfoEx_(handle + 1, nullptr, info.data(), 0x18) == InvalidHandle, "device info was not refused for a port that is not open");
    Require(sceVideoOutGetDeviceInfoEx_(handle + 1, &reserved, nullptr, 0x18) == InvalidHandle, "the handle was not checked before the other device info arguments");
    Require(sceVideoOutGetDeviceInfoEx_(handle, &reserved, info.data(), 0x18) == InvalidValue, "a non-null second device info argument was accepted");
    Require(sceVideoOutGetDeviceInfoEx_(handle, &reserved, nullptr, 0x18) == InvalidValue, "the second device info argument was not checked before the output");
    Require(sceVideoOutGetDeviceInfoEx_(handle, nullptr, nullptr, 0x18) == InvalidAddress, "a null device info output was accepted");

    Require(sceVideoOutConfigureOutputMode_(handle + 1, 0, mode.data(), 0, 0, 0x10) == InvalidHandle, "an output mode was not refused for a port that is not open");
    const auto shortMode = ModeRecord(0x1f);
    Require(sceVideoOutConfigureOutputMode_(handle, 0, shortMode.data(), 0, 0, 0x10) == UnsupportedOutputMode, "a mode record whose first word is not 0x20 was accepted");
    const auto emptyMode = ModeRecord(0);
    Require(sceVideoOutConfigureOutputMode_(handle, 0, emptyMode.data(), 0, 0, 0x10) == UnsupportedOutputMode, "a mode record whose first word is 0 was accepted");
    std::array<std::uint8_t, 0x20> anyMode{};
    sceVideoOutModeSetAny_(anyMode.data(), 0x20);
    Require(sceVideoOutConfigureOutputMode_(handle, 0, anyMode.data(), 0, 0x20, 0x10) == 0, "a mode record left at any in every field was refused");
    auto shortAnyMode = anyMode;
    shortAnyMode[0] = 0x1f;
    Require(sceVideoOutConfigureOutputMode_(handle, 0, shortAnyMode.data(), 0, 0x20, 0x10) == UnsupportedOutputMode, "a record left at any whose first word is not 0x20 was accepted");
    for (std::size_t offset = 4; offset < anyMode.size(); ++offset) {
        auto changedMode = anyMode;
        changedMode[offset] = 0;
        Require(ConfigureThrows(handle, changedMode, 0), "a mode record with one of bytes 4 to 0x1f not 0xff did not throw");
    }
    auto colorimetryMode = anyMode;
    colorimetryMode[6] = 0x0c;
    Require(ConfigureThrows(handle, colorimetryMode, 0), "a mode record with colorimetry 0x0c did not throw");
    const std::array<std::uint16_t, 8> setting{4};
    Require(ConfigureThrows(handle, anyMode, reinterpret_cast<std::uint64_t>(setting.data())), "a non-null fourth argument did not throw");

    Require(sceVideoOutClose(handle) == 0, "sceVideoOutClose failed");
    Require(sceVideoOutGetDeviceInfoEx_(handle, nullptr, info.data(), 0x18) == InvalidHandle, "device info was not refused for a closed port");
    std::printf("guest VideoOut output mode check tests passed\n");
    LibcRunShutdown_nid_postfix();
    return 0;
}
