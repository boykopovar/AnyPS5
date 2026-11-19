#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <array>
#include <cstdlib>
#include <initializer_list>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestPreInitializationBounds() {
    for (int handle : {-100, -1, 0, 1, 2, 3, 4, 5, 10, PAD_REMOTE_HANDLE, PAD_SPECIAL_HANDLE, 12345}) {
        for (bool enable : {false, true}) {
            Require(scePadSetTiltCorrectionState(handle, enable) == PAD_ERROR_INVALID_HANDLE);
        }
        Require(scePadResetOrientation(handle) == PAD_ERROR_INVALID_HANDLE);
    }
}

void TestPostInitializationUnopenedHandleBounds() {
    Require(scePadInit_nid_postfix() == PAD_OK);

    for (int handle : {-100, -1, 0, 1, 2, 3, 4, 5, 10, PAD_REMOTE_HANDLE, PAD_SPECIAL_HANDLE, 12345}) {
        for (bool enable : {false, true}) {
            Require(scePadSetTiltCorrectionState(handle, enable) == PAD_ERROR_INVALID_HANDLE);
        }
        Require(scePadResetOrientation(handle) == PAD_ERROR_INVALID_HANDLE);
    }
}

void TestStandardHandleTiltCalibrationBounds() {
    const int handle0 = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle0 == 1);

    Require(scePadSetTiltCorrectionState(handle0, true) == PAD_OK);
    Require(scePadSetTiltCorrectionState(handle0, false) == PAD_OK);
    Require(scePadSetTiltCorrectionState(handle0, true) == PAD_OK);

    for (int invalidHandle : {-1, 0, 2, 3, 4, 5, PAD_REMOTE_HANDLE, PAD_SPECIAL_HANDLE, 999}) {
        Require(scePadSetTiltCorrectionState(invalidHandle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetTiltCorrectionState(invalidHandle, false) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadResetOrientation(invalidHandle) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadResetOrientation(handle0) == PAD_OK);
    PadData data{};
    Require(scePadReadState(handle0, &data) == PAD_OK);
    Require(data.orientation_w == 1.0f);
    Require(data.orientation_x == 0.0f);
    Require(data.orientation_y == 0.0f);
    Require(data.orientation_z == 0.0f);
}

void TestTiltCorrectionWithDriftAndCorrection() {
    const int handle0 = 1;

    Require(scePadResetOrientation(handle0) == PAD_OK);
    Require(scePadSetTiltCorrectionState(handle0, false) == PAD_OK);

    PadInputState tilted{};
    tilted.hasMotion = true;
    tilted.accel = {9.80665f, 0.0f, 0.0f};
    tilted.gyro = {0.0f, 0.0f, 0.0f};
    PadPublishInputForPort_nid_postfix(0, tilted);

    PadData data{};
    Require(scePadReadState(handle0, &data) == PAD_OK);
    Require(data.orientation_w == 1.0f);
    Require(data.orientation_x == 0.0f);
    Require(data.orientation_y == 0.0f);
    Require(data.orientation_z == 0.0f);

    Require(scePadSetTiltCorrectionState(handle0, true) == PAD_OK);
    Require(scePadResetOrientation(handle0) == PAD_OK);
    PadPublishInputForPort_nid_postfix(0, tilted);
    Require(scePadReadState(handle0, &data) == PAD_OK);

    Require(scePadSetTiltCorrectionState(handle0, false) == PAD_OK);
    Require(scePadResetOrientation(handle0) == PAD_OK);
    Require(scePadReadState(handle0, &data) == PAD_OK);
    Require(data.orientation_w == 1.0f);
    Require(data.orientation_x == 0.0f);
    Require(data.orientation_y == 0.0f);
    Require(data.orientation_z == 0.0f);
}

void TestSensorErrorStates() {
    const int handle0 = 1;
    Require(scePadSetTiltCorrectionState(handle0, true) == PAD_OK);
    Require(scePadResetOrientation(handle0) == PAD_OK);

    PadInputState zeroAccel{};
    zeroAccel.hasMotion = true;
    zeroAccel.accel = {0.0f, 0.0f, 0.0f};
    zeroAccel.gyro = {0.0f, 0.0f, 0.0f};
    PadPublishInputForPort_nid_postfix(0, zeroAccel);

    PadData data{};
    Require(scePadReadState(handle0, &data) == PAD_OK);
    Require(data.orientation_w == 1.0f);
    Require(data.orientation_x == 0.0f);
    Require(data.orientation_y == 0.0f);
    Require(data.orientation_z == 0.0f);

    PadInputState subThresholdAccel{};
    subThresholdAccel.hasMotion = true;
    subThresholdAccel.accel = {0.0f, 0.99f, 0.0f};
    subThresholdAccel.gyro = {0.0f, 0.0f, 0.0f};
    PadPublishInputForPort_nid_postfix(0, subThresholdAccel);
    Require(scePadReadState(handle0, &data) == PAD_OK);
    Require(data.orientation_w == 1.0f);

    PadInputState aboveThresholdAccel{};
    aboveThresholdAccel.hasMotion = true;
    aboveThresholdAccel.accel = {0.0f, 1.01f, 0.0f};
    aboveThresholdAccel.gyro = {0.0f, 0.0f, 0.0f};
    PadPublishInputForPort_nid_postfix(0, aboveThresholdAccel);
    Require(scePadReadState(handle0, &data) == PAD_OK);

    Require(scePadSetMotionSensorState(handle0, false) == PAD_OK);
    Require(scePadReadState(handle0, &data) == PAD_OK);
    Require(data.orientation_w == 1.0f);
    Require(data.orientation_x == 0.0f);
    Require(data.orientation_y == 0.0f);
    Require(data.orientation_z == 0.0f);

    Require(scePadSetMotionSensorState(handle0, true) == PAD_OK);
    Require(scePadResetOrientation(handle0) == PAD_OK);
}

void TestMultiControllerPortIsolation() {
    const int handle0 = 1;
    const int handle1 = scePadOpen_nid_postfix(0x10000001, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle1 == 2);

    Require(scePadSetTiltCorrectionState(handle0, true) == PAD_OK);
    Require(scePadSetTiltCorrectionState(handle1, false) == PAD_OK);
    Require(scePadResetOrientation(handle0) == PAD_OK);
    Require(scePadResetOrientation(handle1) == PAD_OK);

    PadInputState tilted{};
    tilted.hasMotion = true;
    tilted.accel = {9.80665f, 0.0f, 0.0f};
    tilted.gyro = {0.0f, 0.0f, 0.0f};
    PadPublishInputForPort_nid_postfix(0, tilted);
    PadPublishInputForPort_nid_postfix(1, tilted);

    PadData data1{};
    Require(scePadReadState(handle1, &data1) == PAD_OK);
    Require(data1.orientation_w == 1.0f);
    Require(data1.orientation_x == 0.0f);
    Require(data1.orientation_y == 0.0f);
    Require(data1.orientation_z == 0.0f);

    Require(scePadSetTiltCorrectionState(handle0, false) == PAD_OK);
    Require(scePadSetTiltCorrectionState(handle1, true) == PAD_OK);
    Require(scePadResetOrientation(handle0) == PAD_OK);
    Require(scePadResetOrientation(handle1) == PAD_OK);

    PadPublishInputForPort_nid_postfix(0, tilted);
    PadData data0{};
    Require(scePadReadState(handle0, &data0) == PAD_OK);
    Require(data0.orientation_w == 1.0f);
    Require(data0.orientation_x == 0.0f);
    Require(data0.orientation_y == 0.0f);
    Require(data0.orientation_z == 0.0f);
}

void TestSpecialAndRemotePortBehavior() {
    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special == PAD_SPECIAL_HANDLE);
    Require(scePadSetTiltCorrectionState(special, true) == PAD_OK);
    Require(scePadSetTiltCorrectionState(special, false) == PAD_OK);
    Require(scePadResetOrientation(special) == PAD_OK);
    Require(scePadClose_nid_postfix(special) == PAD_OK);
    Require(scePadSetTiltCorrectionState(special, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadResetOrientation(special) == PAD_ERROR_INVALID_HANDLE);

    const int remote = scePadOpen_nid_postfix(PAD_USER_ID_SYSTEM, PAD_PORT_TYPE_REMOTE, 0, nullptr);
    Require(remote == PAD_REMOTE_HANDLE);
    Require(scePadSetTiltCorrectionState(remote, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetTiltCorrectionState(remote, false) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadResetOrientation(remote) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadClose_nid_postfix(remote) == PAD_OK);
}

void TestClosedStandardHandleBehavior() {
    const int handle0 = 1;
    const int handle1 = 2;

    Require(scePadClose_nid_postfix(handle1) == PAD_OK);
    Require(scePadSetTiltCorrectionState(handle1, true) == PAD_OK);
    Require(scePadSetTiltCorrectionState(handle1, false) == PAD_OK);
    Require(scePadResetOrientation(handle1) == PAD_OK);

    Require(scePadSetTiltCorrectionState(3, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadResetOrientation(3) == PAD_ERROR_INVALID_HANDLE);

    PadPublishInputForPort_nid_postfix(0, PadInputState{});
    PadPublishInputForPort_nid_postfix(1, PadInputState{});
    Require(scePadClose_nid_postfix(handle0) == PAD_OK);
}

} // namespace

int main() {
    TestPreInitializationBounds();
    TestPostInitializationUnopenedHandleBounds();
    TestStandardHandleTiltCalibrationBounds();
    TestTiltCorrectionWithDriftAndCorrection();
    TestSensorErrorStates();
    TestMultiControllerPortIsolation();
    TestSpecialAndRemotePortBehavior();
    TestClosedStandardHandleBehavior();
    return 0;
}
