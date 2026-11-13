#include "prx/libSceVisionManager/libSceVisionManager.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

} // namespace

int main() {
    constexpr int notOpen = static_cast<int>(0x80E70025u);
    for (int call = 0; call < 2; ++call) {
        Require(sceVisionManagerRequestCalibrateHmd() == notOpen);
        Require(sceVisionManagerRequestCalibrateStereoCamera() == notOpen);
        Require(sceVisionManagerRequestCalibrateTrackingLed() == notOpen);
        Require(sceVisionManagerRequestClose() == notOpen);
        Require(sceVisionManagerRequestRecogUtility() == notOpen);
    }
    constexpr int invalidArg = static_cast<int>(0x80E70000u);
    std::uint64_t version = 0;
    Require(sceVisionManagerGetLibraryVersion(&version) == 0 && version == 0x1020000214072421ull);
    errno = 71;
    Require(sceVisionManagerGetLibraryVersion(reinterpret_cast<std::uint64_t*>(static_cast<std::uintptr_t>(0x10))) == invalidArg && errno == 71);
    std::uint32_t count = 0;
    Require(sceVisionManagerGetNumOfRegisterUserDataSlot(version, &count) == 0 && count == 30);
    errno = 71;
    Require(sceVisionManagerGetNumOfRegisterUserDataSlot(version, reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(0x10))) == invalidArg && errno == 71);
    int ctx = 1;
    errno = 71;
    Require(sceVisionManagerRequestFindPad(&ctx, reinterpret_cast<const std::int32_t*>(static_cast<std::uintptr_t>(0x10))) == invalidArg && errno == 71);
    errno = 71;
    Require(sceVisionManagerRequestFindPadCorrespondRegion(&ctx, reinterpret_cast<const std::int32_t*>(static_cast<std::uintptr_t>(0x10)), 1, &ctx) == invalidArg && errno == 71);
    errno = 71;
    Require(sceVisionManagerRequestPadTracking(&ctx, reinterpret_cast<const std::int32_t*>(static_cast<std::uintptr_t>(0x10))) == invalidArg && errno == 71);
    return 0;
}
