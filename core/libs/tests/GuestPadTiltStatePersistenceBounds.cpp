#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <source_location>

namespace {

void Require(bool condition, const std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Requirement failed at %s:%u in %s\n", location.file_name(), location.line(), location.function_name());
        std::abort();
    }
}

void TestPreInitializationBounds() {
    for (int handle : {-1000, -100, -2, -1, 0, 1, 2, 3, 4, 10, 32, 33, 100, PAD_REMOTE_HANDLE, PAD_SPECIAL_HANDLE, 9999}) {
        for (bool enable : {false, true}) {
            Require(scePadSetTiltCorrectionState(handle, enable) == PAD_ERROR_INVALID_HANDLE);
        }
        Require(scePadResetOrientation(handle) == PAD_ERROR_INVALID_HANDLE);
    }
}

void TestPostInitializationUnopenedHandleBounds() {
    Require(scePadInit_nid_postfix() == PAD_OK);

    for (int handle : {-1000, -100, -2, -1, 0, 1, 2, 3, 4, 10, 32, 33, 100, PAD_REMOTE_HANDLE, PAD_SPECIAL_HANDLE, 9999}) {
        for (bool enable : {false, true}) {
            Require(scePadSetTiltCorrectionState(handle, enable) == PAD_ERROR_INVALID_HANDLE);
        }
        Require(scePadResetOrientation(handle) == PAD_ERROR_INVALID_HANDLE);
    }
}

void TestStandardHandleTiltPersistenceAndOrientationReset() {
    const int handle0 = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle0 == 1);

    Require(scePadSetTiltCorrectionState(handle0, true) == PAD_OK);
    Require(scePadResetOrientation(handle0) == PAD_OK);

    PadData data{};
    Require(scePadReadState(handle0, &data) == PAD_OK);
    Require(data.orientation_w == 1.0f);
    Require(data.orientation_x == 0.0f);
    Require(data.orientation_y == 0.0f);
    Require(data.orientation_z == 0.0f);

    PadInputState motionInput{};
    motionInput.hasMotion = true;
    motionInput.accel = {9.80665f, 0.0f, 0.0f};
    motionInput.gyro = {0.0f, 0.0f, 0.0f};
    PadPublishInputForPort_nid_postfix(0, motionInput);

    Require(scePadReadState(handle0, &data) == PAD_OK);

    Require(scePadResetOrientation(handle0) == PAD_OK);
    Require(scePadReadState(handle0, &data) == PAD_OK);
    Require(data.orientation_w == 1.0f);

    PadPublishInputForPort_nid_postfix(0, motionInput);
    Require(scePadReadState(handle0, &data) == PAD_OK);

    Require(scePadSetTiltCorrectionState(handle0, false) == PAD_OK);
    Require(scePadResetOrientation(handle0) == PAD_OK);
    Require(scePadReadState(handle0, &data) == PAD_OK);
    Require(data.orientation_w == 1.0f);
    Require(data.orientation_x == 0.0f);
    Require(data.orientation_y == 0.0f);
    Require(data.orientation_z == 0.0f);

    for (int invalidHandle : {-100, -1, 0, 2, 3, 4, 32, 33, PAD_REMOTE_HANDLE, PAD_SPECIAL_HANDLE, 9999}) {
        Require(scePadSetTiltCorrectionState(invalidHandle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetTiltCorrectionState(invalidHandle, false) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadResetOrientation(invalidHandle) == PAD_ERROR_INVALID_HANDLE);
    }
}

void TestMultiControllerPortIsolation() {
    const int handle0 = 1;
    const int handle1 = scePadOpen_nid_postfix(0x10000001, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle1 == 2);

    Require(scePadSetTiltCorrectionState(handle0, true) == PAD_OK);
    Require(scePadSetTiltCorrectionState(handle1, false) == PAD_OK);
    Require(scePadResetOrientation(handle0) == PAD_OK);
    Require(scePadResetOrientation(handle1) == PAD_OK);

    PadInputState input{};
    input.hasMotion = true;
    input.accel = {9.80665f, 0.0f, 0.0f};
    input.gyro = {0.0f, 0.0f, 0.0f};
    PadPublishInputForPort_nid_postfix(0, input);
    PadPublishInputForPort_nid_postfix(1, input);

    PadData data0{};
    PadData data1{};
    Require(scePadReadState(handle1, &data1) == PAD_OK);
    Require(data1.orientation_w == 1.0f);
    Require(data1.orientation_x == 0.0f);
    Require(data1.orientation_y == 0.0f);
    Require(data1.orientation_z == 0.0f);

    Require(scePadResetOrientation(handle0) == PAD_OK);
    Require(scePadReadState(handle0, &data0) == PAD_OK);
    Require(data0.orientation_w == 1.0f);

    Require(scePadSetTiltCorrectionState(handle0, false) == PAD_OK);
    Require(scePadSetTiltCorrectionState(handle1, true) == PAD_OK);
    Require(scePadResetOrientation(handle0) == PAD_OK);
    Require(scePadResetOrientation(handle1) == PAD_OK);

    PadPublishInputForPort_nid_postfix(0, input);
    PadPublishInputForPort_nid_postfix(1, input);

    Require(scePadReadState(handle0, &data0) == PAD_OK);
    Require(data0.orientation_w == 1.0f);
    Require(data0.orientation_x == 0.0f);
    Require(data0.orientation_y == 0.0f);
    Require(data0.orientation_z == 0.0f);

    Require(scePadResetOrientation(handle1) == PAD_OK);
    Require(scePadReadState(handle1, &data1) == PAD_OK);
    Require(data1.orientation_w == 1.0f);
}

void TestSpecialAndRemotePortBehavior() {
    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special == PAD_SPECIAL_HANDLE);

    Require(scePadSetTiltCorrectionState(special, true) == PAD_OK);
    Require(scePadSetTiltCorrectionState(special, false) == PAD_OK);
    Require(scePadResetOrientation(special) == PAD_OK);

    Require(scePadClose_nid_postfix(special) == PAD_OK);

    Require(scePadSetTiltCorrectionState(special, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetTiltCorrectionState(special, false) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadResetOrientation(special) == PAD_ERROR_INVALID_HANDLE);

    const int remote = scePadOpen_nid_postfix(PAD_USER_ID_SYSTEM, PAD_PORT_TYPE_REMOTE, 0, nullptr);
    Require(remote == PAD_REMOTE_HANDLE);

    Require(scePadSetTiltCorrectionState(remote, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetTiltCorrectionState(remote, false) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadResetOrientation(remote) == PAD_ERROR_INVALID_HANDLE);

    Require(scePadClose_nid_postfix(remote) == PAD_OK);

    Require(scePadSetTiltCorrectionState(remote, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadResetOrientation(remote) == PAD_ERROR_INVALID_HANDLE);
}

void TestClosedStandardHandleBehavior() {
    const int handle0 = 1;
    const int handle1 = 2;

    Require(scePadClose_nid_postfix(handle1) == PAD_OK);
    Require(scePadSetTiltCorrectionState(handle1, true) == PAD_OK);
    Require(scePadSetTiltCorrectionState(handle1, false) == PAD_OK);
    Require(scePadResetOrientation(handle1) == PAD_OK);

    for (int invalid : {-100, -1, 0, 3, 4, 10, 32, 33, 9999}) {
        Require(scePadSetTiltCorrectionState(invalid, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetTiltCorrectionState(invalid, false) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadResetOrientation(invalid) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadClose_nid_postfix(handle0) == PAD_OK);
    Require(scePadSetTiltCorrectionState(handle0, true) == PAD_OK);
    Require(scePadSetTiltCorrectionState(handle0, false) == PAD_OK);
    Require(scePadResetOrientation(handle0) == PAD_OK);
}

} // namespace

int main() {
    TestPreInitializationBounds();
    TestPostInitializationUnopenedHandleBounds();
    TestStandardHandleTiltPersistenceAndOrientationReset();
    TestMultiControllerPortIsolation();
    TestSpecialAndRemotePortBehavior();
    TestClosedStandardHandleBehavior();
    return 0;
}
