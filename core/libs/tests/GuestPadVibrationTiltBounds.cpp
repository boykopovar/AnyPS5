#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <source_location>

namespace {

void Require(bool condition, const char* message, const std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "guest_pad_vibration_tilt_bounds failed at line %u: %s\n", static_cast<unsigned>(location.line()), message);
    std::fflush(stderr);
    std::abort();
}

PadOutputState FetchOutput(int port, std::uint32_t& seen) {
    PadOutputState out{};
    Require(PadFetchOutputForPort_nid_postfix(port, &seen, &out), "expected output state update");
    return out;
}

void CheckVibration(const PadOutputState& out, std::uint8_t expectedLarge, std::uint8_t expectedSmall) {
    Require(out.vibrationLarge == expectedLarge, "large motor speed mismatch");
    Require(out.vibrationSmall == expectedSmall, "small motor speed mismatch");
}

void TestPreInitializationBounds() {
    const PadVibrationParam param{255, 128};
    for (int handle : {-100, -1, 0, 1, 2, 16, 32, 100, 9999}) {
        Require(scePadSetVibration(handle, &param) == PAD_ERROR_INVALID_HANDLE, "pre-init set vibration must return INVALID_HANDLE");
        Require(scePadSetVibration(handle, nullptr) == PAD_ERROR_INVALID_HANDLE, "pre-init set vibration null must return INVALID_HANDLE");
        Require(scePadSetTiltCorrectionState(handle, true) == PAD_ERROR_INVALID_HANDLE, "pre-init set tilt true must return INVALID_HANDLE");
        Require(scePadSetTiltCorrectionState(handle, false) == PAD_ERROR_INVALID_HANDLE, "pre-init set tilt false must return INVALID_HANDLE");
        Require(scePadDisableVibration(handle, true) == PAD_ERROR_INVALID_HANDLE, "pre-init disable vibration must return INVALID_HANDLE");
        Require(scePadDisableVibration(handle, false) == PAD_ERROR_INVALID_HANDLE, "pre-init disable vibration false must return INVALID_HANDLE");
        Require(scePadSetVibrationMode(handle, 1) == PAD_ERROR_NOT_INITIALIZED, "pre-init vibration mode must return NOT_INITIALIZED");
    }
}

void TestPostInitializationUnopenedHandleBounds() {
    Require(scePadInit_nid_postfix() == PAD_OK, "scePadInit must return PAD_OK");
    Require(scePadInit_nid_postfix() == PAD_OK, "repeated scePadInit must return PAD_OK");

    const PadVibrationParam param{100, 200};
    for (int handle : {-500, -1, 0, 1, 2, 3, 4, 16, 32, 47, 500}) {
        Require(scePadSetVibration(handle, &param) == PAD_ERROR_INVALID_HANDLE, "unopened handle set vibration must return INVALID_HANDLE");
        Require(scePadSetVibration(handle, nullptr) == PAD_ERROR_INVALID_HANDLE, "unopened handle set vibration null must return INVALID_HANDLE");
        Require(scePadSetTiltCorrectionState(handle, true) == PAD_ERROR_INVALID_HANDLE, "unopened handle set tilt true must return INVALID_HANDLE");
        Require(scePadSetTiltCorrectionState(handle, false) == PAD_ERROR_INVALID_HANDLE, "unopened handle set tilt false must return INVALID_HANDLE");
        Require(scePadDisableVibration(handle, true) == PAD_ERROR_INVALID_HANDLE, "unopened handle disable vibration must return INVALID_HANDLE");
        Require(scePadDisableVibration(handle, false) == PAD_ERROR_INVALID_HANDLE, "unopened handle disable vibration false must return INVALID_HANDLE");
        Require(scePadSetVibrationMode(handle, 1) == PAD_ERROR_INVALID_HANDLE, "unopened handle vibration mode must return INVALID_HANDLE");
        Require(scePadSetVibrationMode(handle, 0) == PAD_ERROR_INVALID_ARG, "mode 0 must return INVALID_ARG");
        Require(scePadSetVibrationMode(handle, -1) == PAD_ERROR_INVALID_ARG, "mode -1 must return INVALID_ARG");
    }
}

void TestStandardHandleVibrationMotorRanges() {
    const int handle0 = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle0 > 0, "scePadOpen port 0 failed");

    Require(scePadSetVibration(handle0, nullptr) == PAD_ERROR_INVALID_ARG, "null param on open handle must return INVALID_ARG");

    std::uint32_t seen0 = 0xffffffffu;
    PadOutputState out0{};
    PadFetchOutputForPort_nid_postfix(0, &seen0, &out0);
    CheckVibration(out0, 0, 0);

    const PadVibrationParam testSpeeds[] = {
        {255, 0},
        {0, 255},
        {255, 255},
        {1, 1},
        {128, 64},
        {64, 192},
        {254, 127},
        {0, 0}
    };

    for (const auto& speed : testSpeeds) {
        Require(scePadSetVibration(handle0, &speed) == PAD_OK, "scePadSetVibration failed");
        out0 = FetchOutput(0, seen0);
        CheckVibration(out0, speed.large_motor, speed.small_motor);
    }

    const PadVibrationParam currentSpeed{254, 127};
    Require(scePadSetVibration(handle0, &currentSpeed) == PAD_OK, "scePadSetVibration failed");
    out0 = FetchOutput(0, seen0);
    CheckVibration(out0, 254, 127);

    Require(scePadSetVibration(handle0, &currentSpeed) == PAD_OK, "idempotent scePadSetVibration failed");
    Require(!PadFetchOutputForPort_nid_postfix(0, &seen0, &out0), "duplicate motor speed must not trigger output sequence increment");

    Require(scePadDisableVibration(handle0, true) == PAD_OK, "scePadDisableVibration true failed");
    out0 = FetchOutput(0, seen0);
    CheckVibration(out0, 0, 0);

    Require(scePadDisableVibration(handle0, false) == PAD_OK, "scePadDisableVibration false failed");
    Require(!PadFetchOutputForPort_nid_postfix(0, &seen0, &out0), "disable vibration false must not modify output");

    const PadVibrationParam restoredSpeed{180, 90};
    Require(scePadSetVibration(handle0, &restoredSpeed) == PAD_OK, "restore vibration after disable failed");
    out0 = FetchOutput(0, seen0);
    CheckVibration(out0, 180, 90);

    Require(scePadDisableVibration(handle0, true) == PAD_OK, "clean up motors failed");
    out0 = FetchOutput(0, seen0);
    CheckVibration(out0, 0, 0);
}

void TestStandardHandleTiltCorrectionAndOrientation() {
    const int handle0 = 1;
    Require(scePadSetTiltCorrectionState(handle0, false) == PAD_OK, "disable tilt correction failed");
    Require(scePadSetTiltCorrectionState(handle0, true) == PAD_OK, "enable tilt correction failed");

    Require(scePadSetTiltCorrectionState(handle0, false) == PAD_OK, "disable tilt correction before test failed");
    Require(scePadResetOrientation(handle0) == PAD_OK, "scePadResetOrientation failed");

    PadData data{};
    Require(scePadReadState(handle0, &data) == 0, "scePadReadState failed");
    Require(data.orientation_w == 1.0f && data.orientation_x == 0.0f && data.orientation_y == 0.0f && data.orientation_z == 0.0f, "initial orientation must be identity");

    PadInputState input{};
    input.hasMotion = true;
    input.accel = {5.0f, 8.5f, 0.0f};
    input.gyro = {0.0f, 0.0f, 0.0f};
    PadPublishInputForPort_nid_postfix(0, input);

    Require(scePadReadState(handle0, &data) == 0, "scePadReadState after untilted motion failed");
    Require(data.orientation_w == 1.0f && data.orientation_x == 0.0f && data.orientation_y == 0.0f && data.orientation_z == 0.0f, "orientation must remain identity when tilt correction is disabled and gyro is zero");

    Require(scePadSetTiltCorrectionState(handle0, true) == PAD_OK, "enable tilt correction failed");
    PadPublishInputForPort_nid_postfix(0, input);

    Require(scePadReadState(handle0, &data) == 0, "scePadReadState after tilted motion failed");
    Require(data.orientation_w < 1.0f || data.orientation_z != 0.0f, "orientation must be adjusted by tilt correction");

    Require(scePadSetTiltCorrectionState(handle0, false) == PAD_OK, "disable tilt correction before reset failed");
    PadInputState neutralInput{};
    PadPublishInputForPort_nid_postfix(0, neutralInput);

    Require(scePadResetOrientation(handle0) == PAD_OK, "reset orientation failed");
    Require(scePadReadState(handle0, &data) == 0, "scePadReadState after reset failed");
    Require(data.orientation_w == 1.0f && data.orientation_x == 0.0f && data.orientation_y == 0.0f && data.orientation_z == 0.0f, "reset orientation must restore identity");
}

void TestMultiControllerVibrationTiltIsolation() {
    std::uint32_t seen0 = 0xffffffffu;
    PadOutputState out0{};
    PadFetchOutputForPort_nid_postfix(0, &seen0, &out0);

    const int handle1 = scePadOpen_nid_postfix(0x10000001, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle1 > 0, "open handle1 failed");
    std::uint32_t seen1 = 0xffffffffu;
    PadOutputState out1{};
    PadFetchOutputForPort_nid_postfix(1, &seen1, &out1);
    CheckVibration(out1, 0, 0);

    const int handle2 = scePadOpen_nid_postfix(0x10000002, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle2 > 0, "open handle2 failed");
    std::uint32_t seen2 = 0xffffffffu;
    PadOutputState out2{};
    PadFetchOutputForPort_nid_postfix(2, &seen2, &out2);
    CheckVibration(out2, 0, 0);

    const PadVibrationParam vib0{255, 50};
    const PadVibrationParam vib1{50, 255};
    const PadVibrationParam vib2{128, 128};

    Require(scePadSetVibration(1, &vib0) == PAD_OK, "set port 0 vibration failed");
    out0 = FetchOutput(0, seen0);
    CheckVibration(out0, 255, 50);
    out1 = FetchOutput(1, seen1);
    CheckVibration(out1, 0, 0);
    out2 = FetchOutput(2, seen2);
    CheckVibration(out2, 0, 0);

    Require(scePadSetVibration(handle1, &vib1) == PAD_OK, "set port 1 vibration failed");
    out1 = FetchOutput(1, seen1);
    CheckVibration(out1, 50, 255);
    out0 = FetchOutput(0, seen0);
    CheckVibration(out0, 255, 50);
    out2 = FetchOutput(2, seen2);
    CheckVibration(out2, 0, 0);

    Require(scePadSetVibration(handle2, &vib2) == PAD_OK, "set port 2 vibration failed");
    out2 = FetchOutput(2, seen2);
    CheckVibration(out2, 128, 128);
    out0 = FetchOutput(0, seen0);
    CheckVibration(out0, 255, 50);
    out1 = FetchOutput(1, seen1);
    CheckVibration(out1, 50, 255);

    Require(scePadSetTiltCorrectionState(handle1, false) == PAD_OK, "disable tilt correction on handle1 failed");
    Require(scePadSetTiltCorrectionState(handle1, true) == PAD_OK, "enable tilt correction on handle1 failed");

    Require(scePadDisableVibration(handle1, true) == PAD_OK, "disable vibration on handle1 failed");
    out1 = FetchOutput(1, seen1);
    CheckVibration(out1, 0, 0);
    out0 = FetchOutput(0, seen0);
    CheckVibration(out0, 255, 50);
    out2 = FetchOutput(2, seen2);
    CheckVibration(out2, 128, 128);

    Require(scePadDisableVibration(1, true) == PAD_OK, "disable vibration on handle0 failed");
    out0 = FetchOutput(0, seen0);
    CheckVibration(out0, 0, 0);

    Require(scePadDisableVibration(handle2, true) == PAD_OK, "disable vibration on handle2 failed");
    out2 = FetchOutput(2, seen2);
    CheckVibration(out2, 0, 0);

    Require(scePadClose_nid_postfix(handle2) == PAD_OK, "close handle2 failed");
    Require(scePadClose_nid_postfix(handle1) == PAD_OK, "close handle1 failed");
}

void TestSpecialAndRemoteHandleBehavior() {
    const int specialHandle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(specialHandle >= PAD_SPECIAL_HANDLE, "open special port failed");

    Require(scePadSetVibration(specialHandle, nullptr) == PAD_ERROR_INVALID_ARG, "null param on special handle must return INVALID_ARG");

    const PadVibrationParam dummyVib{100, 200};
    Require(scePadSetVibration(specialHandle, &dummyVib) == PAD_OK, "special handle set vibration must return PAD_OK");
    Require(scePadSetTiltCorrectionState(specialHandle, false) == PAD_OK, "special handle set tilt false must return PAD_OK");
    Require(scePadSetTiltCorrectionState(specialHandle, true) == PAD_OK, "special handle set tilt true must return PAD_OK");
    Require(scePadDisableVibration(specialHandle, true) == PAD_OK, "special handle disable vibration must return PAD_OK");

    Require(scePadClose_nid_postfix(specialHandle) == PAD_OK, "close special handle failed");

    Require(scePadSetVibration(specialHandle, &dummyVib) == PAD_ERROR_INVALID_HANDLE, "closed special handle set vibration must return INVALID_HANDLE");
    Require(scePadSetTiltCorrectionState(specialHandle, false) == PAD_ERROR_INVALID_HANDLE, "closed special handle tilt must return INVALID_HANDLE");
    Require(scePadDisableVibration(specialHandle, true) == PAD_ERROR_INVALID_HANDLE, "closed special handle disable vibration must return INVALID_HANDLE");

    const int remoteHandle = scePadOpen_nid_postfix(PAD_USER_ID_SYSTEM, PAD_PORT_TYPE_REMOTE, 0, nullptr);
    Require(remoteHandle == PAD_REMOTE_HANDLE, "open remote port failed");

    Require(scePadSetVibration(remoteHandle, &dummyVib) == PAD_ERROR_INVALID_HANDLE, "remote handle set vibration must return INVALID_HANDLE");
    Require(scePadSetVibration(remoteHandle, nullptr) == PAD_ERROR_INVALID_HANDLE, "remote handle set vibration null must return INVALID_HANDLE");
    Require(scePadSetTiltCorrectionState(remoteHandle, false) == PAD_ERROR_INVALID_HANDLE, "remote handle set tilt must return INVALID_HANDLE");
    Require(scePadDisableVibration(remoteHandle, true) == PAD_ERROR_INVALID_HANDLE, "remote handle disable vibration must return INVALID_HANDLE");

    Require(scePadClose_nid_postfix(remoteHandle) == PAD_OK, "close remote handle failed");
    Require(scePadSetVibration(remoteHandle, &dummyVib) == PAD_ERROR_INVALID_HANDLE, "closed remote handle set vibration must return INVALID_HANDLE");
}

void TestClosedStandardHandleBehavior() {
    const int handle0 = 1;
    const PadVibrationParam vib{77, 88};
    std::uint32_t seen0 = 0xffffffffu;
    PadOutputState out0{};

    Require(scePadSetVibration(handle0, &vib) == PAD_OK, "set vibration before close failed");
    out0 = FetchOutput(0, seen0);
    CheckVibration(out0, 77, 88);

    Require(scePadClose_nid_postfix(handle0) == PAD_OK, "close handle0 failed");

    Require(scePadSetVibration(handle0, nullptr) == PAD_ERROR_INVALID_HANDLE, "null param on closed standard handle must return INVALID_HANDLE");

    const PadVibrationParam vibNew{33, 44};
    Require(scePadSetVibration(handle0, &vibNew) == PAD_ERROR_INVALID_HANDLE, "closed standard handle set vibration must return INVALID_HANDLE");

    Require(scePadDisableVibration(handle0, true) == PAD_ERROR_INVALID_HANDLE, "closed standard handle disable vibration must return INVALID_HANDLE");
    Require(scePadDisableVibration(handle0, false) == PAD_ERROR_INVALID_HANDLE, "closed standard handle disable vibration false must return INVALID_HANDLE");

    Require(scePadSetTiltCorrectionState(handle0, false) == PAD_ERROR_INVALID_HANDLE, "closed standard handle set tilt false must return INVALID_HANDLE");
    Require(scePadSetTiltCorrectionState(handle0, true) == PAD_ERROR_INVALID_HANDLE, "closed standard handle set tilt true must return INVALID_HANDLE");

    const int unopenedHandle = 4;
    Require(scePadSetVibration(unopenedHandle, &vibNew) == PAD_ERROR_INVALID_HANDLE, "unopened handle 4 set vibration must return INVALID_HANDLE");
    Require(scePadSetTiltCorrectionState(unopenedHandle, false) == PAD_ERROR_INVALID_HANDLE, "unopened handle 4 tilt must return INVALID_HANDLE");
}

} // namespace

int main() {
    TestPreInitializationBounds();
    TestPostInitializationUnopenedHandleBounds();
    TestStandardHandleVibrationMotorRanges();
    TestStandardHandleTiltCorrectionAndOrientation();
    TestMultiControllerVibrationTiltIsolation();
    TestSpecialAndRemoteHandleBehavior();
    TestClosedStandardHandleBehavior();
    return 0;
}
