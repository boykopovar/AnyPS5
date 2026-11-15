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

    for (int handle : {-100, -1, 0, 1, 2, 3, 4, 5, 10, PAD_REMOTE_HANDLE, PAD_SPECIAL_HANDLE, 12345}) {
        for (bool enable : {false, true}) {
            Require(scePadSetAngularVelocityDeadbandState(handle, enable) == PAD_ERROR_INVALID_HANDLE);
            Require(scePadSetAngularVelocityBiasCorrectionState(handle, enable) == PAD_ERROR_INVALID_HANDLE);
        }
    }

    PadData data{};
    Require(scePadReadState(1, &data) == PAD_ERROR_INVALID_HANDLE);
}

void TestStandardHandleDeadbandAndThresholds() {
    const int handle0 = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle0 == 1);

    Require(scePadSetAngularVelocityDeadbandState(handle0, false) == PAD_OK);
    Require(scePadSetAngularVelocityDeadbandState(handle0, true) == PAD_OK);
    Require(scePadSetAngularVelocityDeadbandState(handle0, false) == PAD_OK);

    for (int invalidHandle : {-1, 0, 2, 3, 4, 5, PAD_REMOTE_HANDLE, PAD_SPECIAL_HANDLE, 999}) {
        Require(scePadSetAngularVelocityDeadbandState(invalidHandle, true) == PAD_ERROR_INVALID_HANDLE);
    }

    PadInputState input{};
    input.hasMotion = true;
    input.accel = {0.0f, 9.80665f, 0.0f};
    input.gyro = {0.005f, 0.012f, -0.015f};
    PadPublishInputForPort_nid_postfix(0, input);

    PadData data{};
    Require(scePadReadState(handle0, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.005f);
    Require(data.angular_velocity_y == 0.012f);
    Require(data.angular_velocity_z == -0.015f);

    Require(scePadSetAngularVelocityDeadbandState(handle0, true) == PAD_OK);
    Require(scePadReadState(handle0, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.0f);
    Require(data.angular_velocity_y == 0.0f);
    Require(data.angular_velocity_z == 0.0f);

    input.gyro = {-0.001f, -0.010f, -0.0174f};
    PadPublishInputForPort_nid_postfix(0, input);
    Require(scePadReadState(handle0, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.0f);
    Require(data.angular_velocity_y == 0.0f);
    Require(data.angular_velocity_z == 0.0f);

    input.gyro = {0.017453290f, -0.017453290f, 0.0f};
    PadPublishInputForPort_nid_postfix(0, input);
    Require(scePadReadState(handle0, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.0f);
    Require(data.angular_velocity_y == 0.0f);
    Require(data.angular_velocity_z == 0.0f);

    input.gyro = {0.017453292f, -0.017453292f, 0.05f};
    PadPublishInputForPort_nid_postfix(0, input);
    Require(scePadReadState(handle0, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.017453292f);
    Require(data.angular_velocity_y == -0.017453292f);
    Require(data.angular_velocity_z == 0.05f);

    input.gyro = {1.0f, -5.5f, 10.0f};
    PadPublishInputForPort_nid_postfix(0, input);
    Require(scePadReadState(handle0, &data) == PAD_OK);
    Require(data.angular_velocity_x == 1.0f);
    Require(data.angular_velocity_y == -5.5f);
    Require(data.angular_velocity_z == 10.0f);

    input.gyro = {0.01f, 0.03f, -0.01f};
    PadPublishInputForPort_nid_postfix(0, input);
    Require(scePadReadState(handle0, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.0f);
    Require(data.angular_velocity_y == 0.03f);
    Require(data.angular_velocity_z == 0.0f);

    Require(scePadSetAngularVelocityDeadbandState(handle0, false) == PAD_OK);
    Require(scePadReadState(handle0, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.01f);
    Require(data.angular_velocity_y == 0.03f);
    Require(data.angular_velocity_z == -0.01f);
}

void TestMotionSensorInteraction() {
    const int handle0 = 1;
    Require(scePadSetMotionSensorState(handle0, false) == PAD_OK);

    PadData data{};
    Require(scePadReadState(handle0, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.0f);
    Require(data.angular_velocity_y == 0.0f);
    Require(data.angular_velocity_z == 0.0f);

    Require(scePadSetMotionSensorState(handle0, true) == PAD_OK);
    Require(scePadReadState(handle0, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.01f);
    Require(data.angular_velocity_y == 0.03f);
    Require(data.angular_velocity_z == -0.01f);
}

void TestMultiControllerPortIsolation() {
    const int handle0 = 1;
    const int handle1 = scePadOpen_nid_postfix(0x10000001, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle1 == 2);

    Require(scePadSetAngularVelocityDeadbandState(handle0, true) == PAD_OK);
    Require(scePadSetAngularVelocityDeadbandState(handle1, false) == PAD_OK);

    PadInputState subThreshold{};
    subThreshold.hasMotion = true;
    subThreshold.accel = {0.0f, 9.80665f, 0.0f};
    subThreshold.gyro = {0.012f, -0.014f, 0.008f};
    PadPublishInputForPort_nid_postfix(0, subThreshold);
    PadPublishInputForPort_nid_postfix(1, subThreshold);

    PadData data0{};
    PadData data1{};
    Require(scePadReadState(handle0, &data0) == PAD_OK);
    Require(data0.angular_velocity_x == 0.0f);
    Require(data0.angular_velocity_y == 0.0f);
    Require(data0.angular_velocity_z == 0.0f);

    Require(scePadReadState(handle1, &data1) == PAD_OK);
    Require(data1.angular_velocity_x == 0.012f);
    Require(data1.angular_velocity_y == -0.014f);
    Require(data1.angular_velocity_z == 0.008f);

    Require(scePadSetAngularVelocityDeadbandState(handle0, false) == PAD_OK);
    Require(scePadSetAngularVelocityDeadbandState(handle1, true) == PAD_OK);

    Require(scePadReadState(handle0, &data0) == PAD_OK);
    Require(data0.angular_velocity_x == 0.012f);
    Require(data0.angular_velocity_y == -0.014f);
    Require(data0.angular_velocity_z == 0.008f);

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

    const int remote = scePadOpen_nid_postfix(PAD_USER_ID_SYSTEM, PAD_PORT_TYPE_REMOTE, 0, nullptr);
    Require(remote == PAD_REMOTE_HANDLE);
    Require(scePadSetAngularVelocityDeadbandState(remote, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetAngularVelocityDeadbandState(remote, false) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadClose_nid_postfix(remote) == PAD_OK);
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

    Require(scePadSetAngularVelocityBiasCorrectionState(999, false) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetAngularVelocityBiasCorrectionState(999, true) == PAD_ERROR_INVALID_HANDLE);
}

void TestClosedStandardHandleBehavior() {
    const int handle0 = 1;
    const int handle1 = 2;

    Require(scePadClose_nid_postfix(handle1) == PAD_OK);
    Require(scePadSetAngularVelocityDeadbandState(handle1, true) == PAD_OK);
    Require(scePadSetAngularVelocityDeadbandState(handle1, false) == PAD_OK);

    Require(scePadSetAngularVelocityDeadbandState(3, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetAngularVelocityDeadbandState(3, false) == PAD_ERROR_INVALID_HANDLE);

    PadPublishInputForPort_nid_postfix(0, PadInputState{});
    PadPublishInputForPort_nid_postfix(1, PadInputState{});
    Require(scePadClose_nid_postfix(handle0) == PAD_OK);
}

} // namespace

int main() {
    TestPreInitializationBounds();
    TestPostInitializationUnopenedHandleBounds();
    TestStandardHandleDeadbandAndThresholds();
    TestMotionSensorInteraction();
    TestMultiControllerPortIsolation();
    TestSpecialAndRemotePortBehavior();
    TestBiasCorrectionBehavior();
    TestClosedStandardHandleBehavior();
    return 0;
}
