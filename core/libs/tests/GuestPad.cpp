#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadInputTypes.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdlib>
#include <thread>

extern "C" {
int APS5_VABI scePadOpen_nid_postfix(int, int, int, const void*);
int APS5_VABI scePadOpenExt(int, int, int, const void*);
int APS5_VABI scePadClose_nid_postfix(int);
int APS5_VABI scePadGetHandle(int, int, int);
int APS5_VABI scePadSetVibrationTriggerEffectWeakWhileEmbeddedMicInUse(bool);
int APS5_VABI scePadSetVibrationMode(int, int);
int APS5_VABI scePadInit_nid_postfix(void);
int APS5_VABI scePadRead_nid_postfix(int, PadData*, int);
int APS5_VABI scePadReadState(int, PadData*);
int APS5_VABI scePadSetTiltCorrectionState(int, bool);
int APS5_VABI scePadResetOrientation(int);
int APS5_VABI scePadSetAngularVelocityDeadbandState(int, bool);
int APS5_VABI scePadSetAngularVelocityBiasCorrectionState(int, bool);
int APS5_VABI scePadIsRemoteController(int, bool*);
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

static void CheckTouchContact() {
    PadInputState touch;
    touch.buttons = static_cast<std::uint32_t>(Pad::PadButton::TouchPad);
    touch.touch[0] = {true, 960, 471, 0};
    PadPublishInput_nid_postfix(touch);
    const auto data = Pad::ReadState();
    Require((data.buttons & static_cast<std::uint32_t>(Pad::PadButton::TouchPad)) != 0);
    Require(data.touch_data_touch_num == 1);
    Require(data.touch_data_touch0_x == 960);
    Require(data.touch_data_touch0_y == 471);
    PadPublishInput_nid_postfix(PadInputState{});
    Pad::ReadState();
}

static void CheckReadQueue(int handle) {
    PadData drained[64]{};
    scePadRead_nid_postfix(handle, drained, 64);
    PadInputState pressed;
    pressed.buttons = static_cast<std::uint32_t>(Pad::PadButton::Cross);
    PadPublishInput_nid_postfix(pressed);
    PadPublishInput_nid_postfix(PadInputState{});
    PadData data[4]{};
    Require(scePadRead_nid_postfix(handle, data, 4) == 2);
    Require((data[0].buttons & pressed.buttons) != 0);
    Require((data[1].buttons & pressed.buttons) == 0);
    Require(data[0].timestamp <= data[1].timestamp);
    Require(scePadRead_nid_postfix(handle, data, 4) == 1);
    Require(data[0].buttons == 0);
}

static void CheckReadStateHandle(int handle) {
    PadData data{};
    Require(scePadReadState(0, &data) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadReadState(handle + 1, &data) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadRead_nid_postfix(0, &data, 1) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadReadState(handle, &data) == PAD_OK);
}

static void CheckRemoteController(int handle) {
    bool remote = true;
    Require(scePadIsRemoteController(handle + 1, &remote) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadIsRemoteController(handle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(remote);
    Require(scePadIsRemoteController(handle, &remote) == PAD_OK);
    Require(!remote);
}

int main() {
    constexpr int noHandle = static_cast<int>(0x80920008);
    constexpr int user = 0x10000000;

    Require(scePadGetHandle(user, 0, 0) == noHandle);
    Require(scePadOpen_nid_postfix(user, 1, 0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadOpen_nid_postfix(user, 0, 1, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetHandle(user, 0, 0) == noHandle);
    const unsigned char wheel[16]{0xb7, 0x0e, 0x04, 0x0e, 0x08, 0x6e, 0x01};
    Require(scePadOpenExt(user, PAD_PORT_TYPE_SPECIAL, 0, wheel) == static_cast<int>(0x80920007));
    Require(scePadOpenExt(user, PAD_PORT_TYPE_SPECIAL, 1, wheel) == PAD_ERROR_INVALID_ARG);
    Require(scePadOpenExt(user, PAD_PORT_TYPE_SPECIAL, 0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetHandle(user, 0, 0) == noHandle);
    const int handle = scePadOpen_nid_postfix(user, 0, 0, nullptr);
    Require(handle > 0);
    Require(scePadGetHandle(user, 0, 0) == handle);
    Require(scePadGetHandle(user, 2, 0) == handle);
    CheckTiltCorrection(handle);
    CheckTouchContact();
    CheckReadQueue(handle);
    CheckReadStateHandle(handle);
    CheckRemoteController(handle);
    Require(scePadSetVibrationMode(handle, 1) == 0);
    Require(scePadSetVibrationMode(handle, 2) == 0);
    Require(scePadSetVibrationMode(handle, 3) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetVibrationMode(handle, -1) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetHandle(0xff, 16, 0) == handle);
    Require(scePadGetHandle(user, 16, 0) == noHandle);
    Require(scePadGetHandle(user, 0, 1) == noHandle);
    Require(scePadClose_nid_postfix(handle) == 0);
    Require(scePadGetHandle(user, 0, 0) == noHandle);
    Require(scePadSetVibrationTriggerEffectWeakWhileEmbeddedMicInUse(true) == 0);
    Require(scePadSetAngularVelocityDeadbandState(handle, false) == 0);
    Require(scePadSetAngularVelocityDeadbandState(handle + 1, false) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetAngularVelocityBiasCorrectionState(handle, false) == PAD_OK);
    Require(scePadSetAngularVelocityBiasCorrectionState(handle + 1, false) == PAD_ERROR_INVALID_HANDLE);
}
