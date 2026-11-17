#include "prx/libSceVideoOut/include/InventoryExports.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

static int ModeIsVr(std::uint32_t flags, std::uint8_t fill) {
    std::array<std::uint8_t, 0x40> mode;
    mode.fill(fill);
    std::memcpy(mode.data() + 0x20, &flags, sizeof(flags));
    return sceVideoOutSysModeIsVr(mode.data());
}

static std::array<std::uint8_t, 0x40> Mode(std::uint32_t flags, std::uint8_t fill) {
    std::array<std::uint8_t, 0x40> mode;
    mode.fill(fill);
    std::memcpy(mode.data() + 0x20, &flags, sizeof(flags));
    return mode;
}

static std::array<std::uint8_t, 0x40> ModeWithByte6(std::uint8_t value, std::uint8_t fill) {
    std::array<std::uint8_t, 0x40> mode;
    mode.fill(fill);
    mode[6] = value;
    return mode;
}

static void CheckInitMode() {
    std::array<std::uint8_t, 0x40> mode;
    mode.fill(0x5A);
    sceVideoOutSysInitMode(mode.data());
    std::uint32_t size;
    std::memcpy(&size, mode.data(), sizeof(size));
    Require(size == 0x20, "sceVideoOutSysInitMode did not store 0x20 in the first dword");
    for (std::size_t i = 4; i < 0x20; ++i) Require(mode[i] == 0xFF, "sceVideoOutSysInitMode did not fill bytes 4 to 0x1F with 0xFF");
    for (std::size_t i = 0x20; i < mode.size(); ++i) Require(mode[i] == 0x5A, "sceVideoOutSysInitMode wrote past 0x20 bytes");
}

static void CheckModeFlags() {
    for (std::uint8_t fill : {std::uint8_t{0}, std::uint8_t{0xFF}}) {
        for (std::uint32_t flags = 0; flags < 8; ++flags) {
            for (std::uint32_t high : {0u, 0xFFFFFFF8u}) {
                const auto mode = Mode(flags | high, fill);
                Require(sceVideoOutSysModeIsSystemDefault(mode.data()) == ((flags & 1u) ? 0 : 1), "sceVideoOutSysModeIsSystemDefault did not report bit 0 of the dword at 0x20 clear");
                Require(sceVideoOutSysModeIsVrNative(mode.data()) == ((flags & 6u) == 2u ? 1 : 0), "sceVideoOutSysModeIsVrNative did not report bits 1 and 2 of the dword at 0x20 as 1 and 0");
                Require(sceVideoOutSysModeIsVrLegacy(mode.data()) == ((flags & 6u) == 6u ? 1 : 0), "sceVideoOutSysModeIsVrLegacy did not report bits 1 and 2 of the dword at 0x20 both set");
            }
        }
    }
}

static void CheckModeIsHdr() {
    for (unsigned value = 0; value < 256; ++value) {
        const int expected = value >= 12 && value <= 17 ? 1 : 0;
        for (std::uint8_t fill : {std::uint8_t{0}, std::uint8_t{0xFF}}) {
            const auto mode = ModeWithByte6(static_cast<std::uint8_t>(value), fill);
            Require(sceVideoOutSysModeIsHdr(mode.data()) == expected, "sceVideoOutSysModeIsHdr did not report byte 6 from 12 to 17");
        }
    }
}

static void CheckIsModeS3d() {
    constexpr std::uint64_t Values[] = {
        0, 1, 0x9F, 0xA0, 0xA1, 0xA2, 0xFF, 0x1A0, 0x100000000A0ull, 0x100000000A1ull,
        0xFFFFFFFF81FFFFFEull, 0xFFFFFFFF81FFFFFFull, 0xFFFFFFFF82000000ull, 0x81FFFFFFull, 0xFFFFFFFFFFFFFFFFull,
    };
    for (const std::uint64_t value : Values) {
        const int expected = value == 0xA0 || value == 0xA1 || value == 0xFFFFFFFF81FFFFFFull ? 1 : 0;
        for (std::uint8_t fill : {std::uint8_t{0}, std::uint8_t{0xFF}}) {
            std::array<std::uint8_t, 0x40> mode;
            mode.fill(fill);
            std::memcpy(mode.data() + 0x10, &value, sizeof(value));
            Require(sceVideoOutSysIsModeS3d(mode.data()) == expected, "sceVideoOutSysIsModeS3d did not report the qword at 0x10 equal to 0xA0, 0xA1 or 0xFFFFFFFF81FFFFFF");
        }
    }
}

int main() {
    Require(sceVideoOutAddBufferHdrPrivilege() == 0, "sceVideoOutAddBufferHdrPrivilege did not return 0");
    Require(sceVideoOutRazorRecallCallbacks() == 0, "sceVideoOutRazorRecallCallbacks did not return 0");
    Require(sceVideoOutSysUpdateRenderingMode() == 0, "sceVideoOutSysUpdateRenderingMode did not return 0");
    Require(sceVideoOutCurveSamplePointsCalc_() == static_cast<int>(0x8029001Cu), "sceVideoOutCurveSamplePointsCalc_ did not return 0x8029001C");
    Require(sceVideoOutDriverGetLatestEopHandle() == static_cast<int>(0x802900FFu), "sceVideoOutDriverGetLatestEopHandle did not return 0x802900FF");
    Require(sceVideoOutSysGetMonitorInfo_() == -1, "sceVideoOutSysGetMonitorInfo_ did not return -1");
    Require(ModeIsVr(0x2u, 0) == 1, "bit 1 of the dword at 0x20 did not report VR");
    Require(ModeIsVr(0xFFFFFFFFu, 0xFF) == 1, "a mode with every bit set did not report VR");
    Require(ModeIsVr(0xFFFFFFFDu, 0xFF) == 0, "the other bits of the dword at 0x20 reported VR");
    Require(ModeIsVr(0u, 0xFF) == 0, "bytes outside the dword at 0x20 reported VR");
    CheckInitMode();
    CheckModeFlags();
    CheckModeIsHdr();
    CheckIsModeS3d();
    return 0;
}
