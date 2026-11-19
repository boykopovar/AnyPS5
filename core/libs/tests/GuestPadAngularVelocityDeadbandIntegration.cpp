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
    for (int handle : {-1000, -100, -2, -1, 0, 1, 2, 3, 4, 10, 32, 33, 100, PAD_REMOTE_HANDLE, PAD_SPECIAL_HANDLE, 9999}) {
        for (bool enable : {false, true}) {
            Require(scePadSetAngularVelocityDeadbandState(handle, enable) == PAD_ERROR_INVALID_HANDLE);
            Require(scePadSetAngularVelocityBiasCorrectionState(handle, enable) == PAD_ERROR_INVALID_HANDLE);
        }
    }

    PadData data{};
    Require(scePadReadState(1, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadReadState(1, &data) == PAD_ERROR_NOT_INITIALIZED);
}

void TestPostInitializationUnopenedHandleBounds() {
    Require(scePadInit_nid_postfix() == PAD_OK);

    for (int handle : {-1000, -100, -2, -1, 0, 1, 2, 3, 4, 10, 32, 33, 100, PAD_REMOTE_HANDLE, PAD_SPECIAL_HANDLE, 9999}) {
        for (bool enable : {false, true}) {
            Require(scePadSetAngularVelocityDeadbandState(handle, enable) == PAD_ERROR_INVALID_HANDLE);
            Require(scePadSetAngularVelocityBiasCorrectionState(handle, enable) == PAD_ERROR_INVALID_HANDLE);
        }
    }

    PadData data{};
    Require(scePadReadState(1, &data) == PAD_ERROR_INVALID_HANDLE);
}

void TestThresholdPerAxisAndOrientationIntegration() {
    const int handle0 = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle0 == 1);

    Require(scePadSetAngularVelocityDeadbandState(handle0, false) == PAD_OK);
    Require(scePadResetOrientation(handle0) == PAD_OK);

    PadInputState subThreshold{};
    subThreshold.hasMotion = true;
    subThreshold.accel = {0.0f, 9.80665f, 0.0f};
    subThreshold.gyro = {0.010f, 0.012f, -0.014f};
    PadPublishInputForPort_nid_postfix(0, subThreshold);

    PadData data{};
    Require(scePadReadState(handle0, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.010f);
    Require(data.angular_velocity_y == 0.012f);
    Require(data.angular_velocity_z == -0.014f);

    Require(scePadSetAngularVelocityDeadbandState(handle0, true) == PAD_OK);
    Require(scePadResetOrientation(handle0) == PAD_OK);

    Require(scePadReadState(handle0, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.0f);
    Require(data.angular_velocity_y == 0.0f);
    Require(data.angular_velocity_z == 0.0f);
    Require(data.orientation_w == 1.0f);
    Require(data.orientation_x == 0.0f);
    Require(data.orientation_y == 0.0f);
    Require(data.orientation_z == 0.0f);

    subThreshold.gyro = {0.017453290f, -0.017453290f, 0.005f};
    PadPublishInputForPort_nid_postfix(0, subThreshold);
    Require(scePadReadState(handle0, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.0f);
    Require(data.angular_velocity_y == 0.0f);
    Require(data.angular_velocity_z == 0.0f);
    Require(data.orientation_w == 1.0f);

    PadInputState boundary{};
    boundary.hasMotion = true;
    boundary.accel = {0.0f, 9.80665f, 0.0f};
    boundary.gyro = {0.017453292f, -0.017453292f, 0.0f};
    PadPublishInputForPort_nid_postfix(0, boundary);
    Require(scePadReadState(handle0, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.017453292f);
    Require(data.angular_velocity_y == -0.017453292f);
    Require(data.angular_velocity_z == 0.0f);

    PadInputState mixedAxes{};
    mixedAxes.hasMotion = true;
    mixedAxes.accel = {0.0f, 9.80665f, 0.0f};
    mixedAxes.gyro = {0.005f, 0.080f, -0.008f};
    PadPublishInputForPort_nid_postfix(0, mixedAxes);
    Require(scePadReadState(handle0, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.0f);
    Require(data.angular_velocity_y == 0.080f);
    Require(data.angular_velocity_z == 0.0f);

    Require(scePadSetAngularVelocityDeadbandState(handle0, false) == PAD_OK);
    Require(scePadReadState(handle0, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.005f);
    Require(data.angular_velocity_y == 0.080f);
    Require(data.angular_velocity_z == -0.008f);

    for (int invalidHandle : {-100, -1, 0, 2, 3, 4, 32, 33, PAD_REMOTE_HANDLE, PAD_SPECIAL_HANDLE, 9999}) {
        Require(scePadSetAngularVelocityDeadbandState(invalidHandle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadbandState(invalidHandle, false) == PAD_ERROR_INVALID_HANDLE);
    }
}

void TestPortSlotDeadbandPersistenceAndMultiControllerIsolation() {
    const int handle0 = 1;
    const int handle1 = scePadOpen_nid_postfix(0x10000001, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle1 == 2);

    Require(scePadSetAngularVelocityDeadbandState(handle0, true) == PAD_OK);
    Require(scePadSetAngularVelocityDeadbandState(handle1, false) == PAD_OK);

    PadInputState subThreshold{};
    subThreshold.hasMotion = true;
    subThreshold.accel = {0.0f, 9.80665f, 0.0f};
    subThreshold.gyro = {0.011f, -0.013f, 0.009f};
    PadPublishInputForPort_nid_postfix(0, subThreshold);
    PadPublishInputForPort_nid_postfix(1, subThreshold);

    PadData data0{};
    PadData data1{};
    Require(scePadReadState(handle0, &data0) == PAD_OK);
    Require(data0.angular_velocity_x == 0.0f);
    Require(data0.angular_velocity_y == 0.0f);
    Require(data0.angular_velocity_z == 0.0f);

    Require(scePadReadState(handle1, &data1) == PAD_OK);
    Require(data1.angular_velocity_x == 0.011f);
    Require(data1.angular_velocity_y == -0.013f);
    Require(data1.angular_velocity_z == 0.009f);

    Require(scePadSetAngularVelocityDeadbandState(handle0, false) == PAD_OK);
    Require(scePadSetAngularVelocityDeadbandState(handle1, true) == PAD_OK);

    Require(scePadReadState(handle0, &data0) == PAD_OK);
    Require(data0.angular_velocity_x == 0.011f);
    Require(data0.angular_velocity_y == -0.013f);
    Require(data0.angular_velocity_z == 0.009f);

    Require(scePadReadState(handle1, &data1) == PAD_OK);
    Require(data1.angular_velocity_x == 0.0f);
    Require(data1.angular_velocity_y == 0.0f);
    Require(data1.angular_velocity_z == 0.0f);
}

void TestSpecialAndRemotePortBehavior() {
    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special == PAD_SPECIAL_HANDLE);

    Require(scePadSetAngularVelocityDeadbandState(special, true) == PAD_OK);
    Require(scePadSetAngularVelocityDeadbandState(special, false) == PAD_OK);

    Require(scePadClose_nid_postfix(special) == PAD_OK);

    Require(scePadSetAngularVelocityDeadbandState(special, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetAngularVelocityDeadbandState(special, false) == PAD_ERROR_INVALID_HANDLE);

    const int remote = scePadOpen_nid_postfix(PAD_USER_ID_SYSTEM, PAD_PORT_TYPE_REMOTE, 0, nullptr);
    Require(remote == PAD_REMOTE_HANDLE);

    Require(scePadSetAngularVelocityDeadbandState(remote, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetAngularVelocityDeadbandState(remote, false) == PAD_ERROR_INVALID_HANDLE);

    Require(scePadClose_nid_postfix(remote) == PAD_OK);

    Require(scePadSetAngularVelocityDeadbandState(remote, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetAngularVelocityDeadbandState(remote, false) == PAD_ERROR_INVALID_HANDLE);
}

void TestClosedStandardHandleBehavior() {
    const int handle0 = 1;
    const int handle1 = 2;

    Require(scePadClose_nid_postfix(handle1) == PAD_OK);
    Require(scePadSetAngularVelocityDeadbandState(handle1, true) == PAD_OK);
    Require(scePadSetAngularVelocityDeadbandState(handle1, false) == PAD_OK);

    for (int invalid : {-100, -1, 0, 3, 4, 10, 32, 33, 9999}) {
        Require(scePadSetAngularVelocityDeadbandState(invalid, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadbandState(invalid, false) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadClose_nid_postfix(handle0) == PAD_OK);
    Require(scePadSetAngularVelocityDeadbandState(handle0, true) == PAD_OK);
    Require(scePadSetAngularVelocityDeadbandState(handle0, false) == PAD_OK);
}

void TestBiasCorrectionBehavior() {
    const int handle0 = 1;
    Require(scePadSetAngularVelocityBiasCorrectionState(handle0, false) == PAD_OK);

    bool caught = false;
    try {
        scePadSetAngularVelocityBiasCorrectionState(handle0, true);
    } catch (...) {
        caught = true;
    }
    Require(caught);

    for (int invalid : {-100, -1, 0, 3, 4, 32, 33, 9999}) {
        Require(scePadSetAngularVelocityBiasCorrectionState(invalid, false) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityBiasCorrectionState(invalid, true) == PAD_ERROR_INVALID_HANDLE);
    }
}

} // namespace

int main() {
    TestPreInitializationBounds();
    TestPostInitializationUnopenedHandleBounds();
    TestThresholdPerAxisAndOrientationIntegration();
    TestPortSlotDeadbandPersistenceAndMultiControllerIsolation();
    TestSpecialAndRemotePortBehavior();
    TestClosedStandardHandleBehavior();
    TestBiasCorrectionBehavior();
    return 0;
}
