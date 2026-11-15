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
    unsigned char block[64] = {};
    Require(sceVisionManagerApplyCalibrateHmdResult(nullptr) == invalidArg);
    Require(sceVisionManagerGenerateRegisterUserData(nullptr, block) == invalidArg);
    Require(sceVisionManagerGenerateRegisterUserData(block, nullptr) == invalidArg);
    Require(sceVisionManagerGetCalibrateTrackingLedNecessity(nullptr, block) == invalidArg);
    Require(sceVisionManagerGetCalibrateTrackingLedNecessity(block, nullptr) == invalidArg);
    Require(sceVisionManagerGetHitCountOfRegisterUserData(nullptr, block, 0, block) == invalidArg);
    Require(sceVisionManagerGetHitCountOfRegisterUserData(block, nullptr, 0, block) == invalidArg);
    Require(sceVisionManagerGetHitCountOfRegisterUserData(block, block, 0, nullptr) == invalidArg);
    Require(sceVisionManagerGetRegisterUserDataVersion(nullptr, block, block) == invalidArg);
    Require(sceVisionManagerGetRegisterUserDataVersion(block, nullptr, block) == invalidArg);
    Require(sceVisionManagerGetRegisterUserDataVersion(block, block, nullptr) == invalidArg);
    Require(sceVisionManagerGetResultOfCalibrateStereoCamera(nullptr, block) == invalidArg);
    Require(sceVisionManagerGetResultOfCalibrateStereoCamera(block, nullptr) == invalidArg);
    Require(sceVisionManagerGetResultOfFindFace(nullptr, block, block, block) == invalidArg);
    Require(sceVisionManagerGetResultOfFindFace(block, nullptr, block, block) == invalidArg);
    Require(sceVisionManagerGetResultOfFindFace(block, block, nullptr, block) == invalidArg);
    Require(sceVisionManagerGetResultOfFindFace(block, block, block, nullptr) == invalidArg);
    Require(sceVisionManagerRequestInitializeRegisterUserData(nullptr) == invalidArg);
    Require(sceVisionManagerRequestOpen(nullptr) == invalidArg);
    Require(sceVisionManagerSetCallbackForUpdateCalibrateHmdResult(nullptr) == invalidArg);
    Require(sceVisionManagerSetCallbackForUpdateCalibrateStereoCameraResult(nullptr) == invalidArg);
    Require(sceVisionManagerSetCallbackForUpdateCalibrateTrackingLedResult(nullptr) == invalidArg);
    Require(sceVisionManagerSetCallbackForUpdateRunningStatus(nullptr, block) == invalidArg);
    Require(sceVisionManagerSetCallbackForUpdateRunningStatus(block, nullptr) == invalidArg);
    return 0;
}
