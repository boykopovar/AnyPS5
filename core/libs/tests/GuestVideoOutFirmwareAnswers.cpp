#include "prx/libSceVideoOut/include/InventoryExports.hpp"
#include <array>
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
    return 0;
}
