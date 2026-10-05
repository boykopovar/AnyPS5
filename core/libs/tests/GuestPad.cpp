#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdlib>
#include <thread>

extern "C" {
int APS5_VABI scePadOpen_nid_postfix(int, int, int, const void*);
int APS5_VABI scePadClose_nid_postfix(int);
int APS5_VABI scePadGetHandle(int, int, int);
int APS5_VABI scePadSetVibrationTriggerEffectWeakWhileEmbeddedMicInUse(bool);
int APS5_VABI scePadInit_nid_postfix(void);
int APS5_VABI scePadSetTiltCorrectionState(int, bool);
int APS5_VABI scePadResetOrientation(int);
int APS5_VABI scePadSetAngularVelocityDeadbandState(int, bool);
}

static void Require(bool value) { if (!value) std::abort(); }

static float SettleOrientationW() {
    float w = 1.0f;
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        w = Pad::ReadState().orientation_w;
    }
    return w;
}

static void CheckTiltCorrection(int handle) {
    Require(scePadInit_nid_postfix() == 0);
    PadInputState tilted;
    tilted.hasMotion = true;
    tilted.accel = {9.80665f, 0.0f, 0.0f};
    PadPublishInput_nid_postfix(tilted);
    Require(scePadSetTiltCorrectionState(handle + 1, false) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetTiltCorrectionState(handle, false) == PAD_OK);
    Require(scePadResetOrientation(handle) == PAD_OK);
    Require(SettleOrientationW() == 1.0f);
    Require(scePadSetTiltCorrectionState(handle, true) == PAD_OK);
    Require(SettleOrientationW() < 0.999f);
    PadPublishInput_nid_postfix(PadInputState{});
}

int main() {
    constexpr int noHandle = static_cast<int>(0x80920008);
    constexpr int user = 0x10000000;

    Require(scePadGetHandle(user, 0, 0) == noHandle);
    Require(scePadOpen_nid_postfix(user, 1, 0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadOpen_nid_postfix(user, 0, 1, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetHandle(user, 0, 0) == noHandle);
    const int handle = scePadOpen_nid_postfix(user, 0, 0, nullptr);
    Require(handle > 0);
    Require(scePadGetHandle(user, 0, 0) == handle);
    Require(scePadGetHandle(user, 2, 0) == handle);
    CheckTiltCorrection(handle);
    Require(scePadGetHandle(0xff, 16, 0) == handle);
    Require(scePadGetHandle(user, 16, 0) == noHandle);
    Require(scePadGetHandle(user, 0, 1) == noHandle);
    Require(scePadClose_nid_postfix(handle) == 0);
    Require(scePadGetHandle(user, 0, 0) == noHandle);
    Require(scePadSetVibrationTriggerEffectWeakWhileEmbeddedMicInUse(true) == 0);
    Require(scePadSetAngularVelocityDeadbandState(handle, false) == 0);
    Require(scePadSetAngularVelocityDeadbandState(handle + 1, false) == PAD_ERROR_INVALID_HANDLE);
}
