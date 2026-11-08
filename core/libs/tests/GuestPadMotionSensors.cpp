#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <cmath>
#include <cstdlib>
#include <initializer_list>

static void Require(bool value) {
    if (!value) std::abort();
}

static void CheckUninitializedState() {
    float unit = 5.0f;
    Require(scePadGetMotionTimerUnit(PAD_HANDLE, &unit) == PAD_ERROR_NOT_INITIALIZED && unit == 5.0f);
    Require(scePadGetMotionTimerUnit(PAD_HANDLE, nullptr) == PAD_ERROR_NOT_INITIALIZED);

    PadMotionSensorPosition position{1.0f, 2.0f, 3.0f};
    Require(scePadGetMotionSensorPosition(PAD_HANDLE, &position) == PAD_ERROR_NOT_INITIALIZED);
    Require(position.x == 1.0f && position.y == 2.0f && position.z == 3.0f);
    Require(scePadGetMotionSensorPosition(PAD_HANDLE, nullptr) == PAD_ERROR_NOT_INITIALIZED);

    PadData data{};
    Require(scePadReadState(PAD_HANDLE, &data) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadRead_nid_postfix(PAD_HANDLE, &data, 1) == PAD_ERROR_NOT_INITIALIZED);

    for (int handle : {-1, 0, 1, 2, 100}) {
        Require(scePadSetMotionSensorState(handle, true) == PAD_ERROR_NOT_INITIALIZED || scePadSetMotionSensorState(handle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadResetOrientation(handle) == PAD_ERROR_NOT_INITIALIZED || scePadResetOrientation(handle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadbandState(handle, true) == PAD_ERROR_NOT_INITIALIZED || scePadSetAngularVelocityDeadbandState(handle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetTiltCorrectionState(handle, true) == PAD_ERROR_NOT_INITIALIZED || scePadSetTiltCorrectionState(handle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityBiasCorrectionState(handle, false) == PAD_ERROR_NOT_INITIALIZED || scePadSetAngularVelocityBiasCorrectionState(handle, false) == PAD_ERROR_INVALID_HANDLE);
    }
}

static void CheckInvalidHandlesAndArgs(int validHandle) {
    for (int handle : {-9999, -100, -1, 0, validHandle + 10, 9999}) {
        Require(scePadSetMotionSensorState(handle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetMotionSensorState(handle, false) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadResetOrientation(handle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadbandState(handle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadbandState(handle, false) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetTiltCorrectionState(handle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetTiltCorrectionState(handle, false) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityBiasCorrectionState(handle, false) == PAD_ERROR_INVALID_HANDLE);

        float unit = 0.0f;
        Require(scePadGetMotionTimerUnit(handle, &unit) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadGetMotionTimerUnit(handle, nullptr) == PAD_ERROR_INVALID_ARG);

        PadMotionSensorPosition position{};
        Require(scePadGetMotionSensorPosition(handle, &position) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadGetMotionSensorPosition(handle, nullptr) == PAD_ERROR_INVALID_ARG);

        PadData data{};
        Require(scePadReadState(handle, &data) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadReadState(handle, nullptr) == PAD_ERROR_INVALID_ARG);
    }

    PadData data{};
    Require(scePadReadState(validHandle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadRead_nid_postfix(validHandle, nullptr, 1) == PAD_ERROR_INVALID_ARG);
    Require(scePadRead_nid_postfix(validHandle, &data, 0) == PAD_ERROR_INVALID_ARG);
    Require(scePadRead_nid_postfix(validHandle, &data, -1) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetMotionTimerUnit(validHandle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetMotionSensorPosition(validHandle, nullptr) == PAD_ERROR_INVALID_ARG);
}

static void CheckMotionQueries(int handle) {
    constexpr int notConnected = static_cast<int>(0x80920007);
    float unit = 99.0f;
    Require(scePadGetMotionTimerUnit(handle, &unit) == notConnected);
    Require(unit == 0.0f);

    PadMotionSensorPosition position{5.0f, 6.0f, 7.0f};
    Require(scePadGetMotionSensorPosition(handle, &position) == notConnected);
}

static void CheckAccelerometerBounds(int handle) {
    Require(scePadSetMotionSensorState(handle, true) == PAD_OK);
    PadData data{};
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.acceleration_x == 0.0f && data.acceleration_y == 1.0f && data.acceleration_z == 0.0f);

    struct AccelVector {
        std::array<float, 3> host;
        std::array<float, 3> guestExpected;
    };

    const AccelVector testVectors[] = {
        {{0.0f, 9.80665f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
        {{0.0f, -9.80665f, 0.0f}, {0.0f, -1.0f, 0.0f}},
        {{9.80665f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{-9.80665f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
        {{0.0f, 0.0f, 9.80665f}, {0.0f, 0.0f, 1.0f}},
        {{0.0f, 0.0f, -9.80665f}, {0.0f, 0.0f, -1.0f}},
        {{19.6133f, -19.6133f, 19.6133f}, {2.0f, -2.0f, 2.0f}},
        {{-39.2266f, 39.2266f, -39.2266f}, {-4.0f, 4.0f, -4.0f}},
        {{78.4532f, -78.4532f, 78.4532f}, {8.0f, -8.0f, 8.0f}},
        {{-156.9064f, 156.9064f, -156.9064f}, {-16.0f, 16.0f, -16.0f}},
        {{0.0980665f, -0.4903325f, 0.24516625f}, {0.01f, -0.05f, 0.025f}}
    };

    for (const auto& vector : testVectors) {
        PadInputState input{};
        input.hasMotion = true;
        input.accel = vector.host;
        PadPublishInput_nid_postfix(input);

        Require(scePadReadState(handle, &data) == PAD_OK);
        Require(std::fabs(data.acceleration_x - vector.guestExpected[0]) < 1e-4f);
        Require(std::fabs(data.acceleration_y - vector.guestExpected[1]) < 1e-4f);
        Require(std::fabs(data.acceleration_z - vector.guestExpected[2]) < 1e-4f);
    }

    Require(scePadSetMotionSensorState(handle, false) == PAD_OK);
    PadInputState extremeInput{};
    extremeInput.hasMotion = true;
    extremeInput.accel = {78.4532f, -78.4532f, 156.9064f};
    extremeInput.gyro = {1.0f, 2.0f, 3.0f};
    PadPublishInput_nid_postfix(extremeInput);

    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.acceleration_x == 0.0f && data.acceleration_y == 1.0f && data.acceleration_z == 0.0f);
    Require(data.angular_velocity_x == 0.0f && data.angular_velocity_y == 0.0f && data.angular_velocity_z == 0.0f);
    Require(data.orientation_x == 0.0f && data.orientation_y == 0.0f && data.orientation_z == 0.0f && data.orientation_w == 1.0f);

    Require(scePadSetMotionSensorState(handle, true) == PAD_OK);
    PadPublishInput_nid_postfix(PadInputState{});
}

static void CheckGyroAngularVelocityUnits(int handle) {
    PadData data{};
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.0f && data.angular_velocity_y == 0.0f && data.angular_velocity_z == 0.0f);

    Require(scePadSetAngularVelocityDeadbandState(handle, false) == PAD_OK);

    PadInputState smallMovement{};
    smallMovement.hasMotion = true;
    smallMovement.accel = {0.0f, 9.80665f, 0.0f};
    smallMovement.gyro = {0.005f, -0.012f, 0.015f};
    PadPublishInput_nid_postfix(smallMovement);

    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.005f);
    Require(data.angular_velocity_y == -0.012f);
    Require(data.angular_velocity_z == 0.015f);

    Require(scePadSetAngularVelocityDeadbandState(handle, true) == PAD_OK);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.0f && data.angular_velocity_y == 0.0f && data.angular_velocity_z == 0.0f);

    PadInputState thresholdMovement{};
    thresholdMovement.hasMotion = true;
    thresholdMovement.accel = {0.0f, 9.80665f, 0.0f};
    thresholdMovement.gyro = {0.01745f, 0.01750f, -0.01745f};
    PadPublishInput_nid_postfix(thresholdMovement);

    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.0f);
    Require(data.angular_velocity_y == 0.01750f);
    Require(data.angular_velocity_z == 0.0f);

    PadInputState thresholdNegativeMovement{};
    thresholdNegativeMovement.hasMotion = true;
    thresholdNegativeMovement.accel = {0.0f, 9.80665f, 0.0f};
    thresholdNegativeMovement.gyro = {-0.01750f, 0.0f, 0.0f};
    PadPublishInput_nid_postfix(thresholdNegativeMovement);

    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.angular_velocity_x == -0.01750f);

    PadInputState dynamicMovement{};
    dynamicMovement.hasMotion = true;
    dynamicMovement.accel = {0.0f, 9.80665f, 0.0f};
    dynamicMovement.gyro = {0.5235988f, -1.0471976f, 3.1415927f};
    PadPublishInput_nid_postfix(dynamicMovement);

    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(std::fabs(data.angular_velocity_x - 0.5235988f) < 1e-5f);
    Require(std::fabs(data.angular_velocity_y - (-1.0471976f)) < 1e-5f);
    Require(std::fabs(data.angular_velocity_z - 3.1415927f) < 1e-5f);

    PadInputState highVelocityMovement{};
    highVelocityMovement.hasMotion = true;
    highVelocityMovement.accel = {0.0f, 9.80665f, 0.0f};
    highVelocityMovement.gyro = {-10.0f, 10.0f, -10.0f};
    PadPublishInput_nid_postfix(highVelocityMovement);

    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.angular_velocity_x == -10.0f && data.angular_velocity_y == 10.0f && data.angular_velocity_z == -10.0f);

    Require(scePadSetMotionSensorState(handle, false) == PAD_OK);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.0f && data.angular_velocity_y == 0.0f && data.angular_velocity_z == 0.0f);

    Require(scePadSetMotionSensorState(handle, true) == PAD_OK);
    PadPublishInput_nid_postfix(PadInputState{});
}

static void CheckCalibrationAndTiltCorrection(int handle) {
    PadData data{};
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.orientation_x == 0.0f && data.orientation_y == 0.0f && data.orientation_z == 0.0f && data.orientation_w == 1.0f);

    Require(scePadResetOrientation(handle) == PAD_OK);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.orientation_x == 0.0f && data.orientation_y == 0.0f && data.orientation_z == 0.0f && data.orientation_w == 1.0f);

    Require(scePadSetAngularVelocityBiasCorrectionState(handle, false) == PAD_OK);

    Require(scePadSetTiltCorrectionState(handle, false) == PAD_OK);
    Require(scePadResetOrientation(handle) == PAD_OK);

    PadInputState tilted{};
    tilted.hasMotion = true;
    tilted.accel = {9.80665f, 0.0f, 0.0f};
    PadPublishInput_nid_postfix(tilted);

    float settledW = 0.0f;
    for (int i = 0; i < 10; ++i) {
        settledW = Pad::ReadState(0).orientation_w;
    }
    Require(settledW == 1.0f);

    Require(scePadSetTiltCorrectionState(handle, true) == PAD_OK);
    for (int i = 0; i < 30; ++i) {
        settledW = Pad::ReadState(0).orientation_w;
    }
    Require(settledW < 0.999f);

    Require(scePadResetOrientation(handle) == PAD_OK);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.orientation_w == 1.0f && data.orientation_x == 0.0f);

    PadPublishInput_nid_postfix(PadInputState{});
}

static void CheckSpecialHandleMotion(int special) {
    Require(special > 0);
    Require(scePadSetMotionSensorState(special, true) == PAD_OK);
    Require(scePadSetMotionSensorState(special, false) == PAD_OK);
    Require(scePadResetOrientation(special) == PAD_OK);
    Require(scePadSetAngularVelocityDeadbandState(special, true) == PAD_OK);
    Require(scePadSetAngularVelocityDeadbandState(special, false) == PAD_OK);
    Require(scePadSetTiltCorrectionState(special, true) == PAD_OK);
    Require(scePadSetTiltCorrectionState(special, false) == PAD_OK);
    Require(scePadSetAngularVelocityBiasCorrectionState(special, false) == PAD_OK);

    float unit = 99.0f;
    constexpr int notConnected = static_cast<int>(0x80920007);
    Require(scePadGetMotionTimerUnit(special, &unit) == notConnected && unit == 0.0f);

    PadMotionSensorPosition position{};
    Require(scePadGetMotionSensorPosition(special, &position) == notConnected);
}

static void CheckMultiPortMotionIsolation(int handle0) {
    const int handle1 = scePadOpen_nid_postfix(0x10000001, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle1 > 0);

    Require(scePadSetMotionSensorState(handle0, true) == PAD_OK);
    Require(scePadSetMotionSensorState(handle1, true) == PAD_OK);

    Require(scePadSetAngularVelocityDeadbandState(handle0, false) == PAD_OK);
    Require(scePadSetAngularVelocityDeadbandState(handle1, true) == PAD_OK);

    PadInputState input0{};
    input0.hasMotion = true;
    input0.accel = {4.903325f, 9.80665f, 0.0f};
    input0.gyro = {0.010f, 0.20f, -0.30f};
    PadPublishInputForPort_nid_postfix(0, input0);

    PadInputState input1{};
    input1.hasMotion = true;
    input1.accel = {0.0f, -9.80665f, 4.903325f};
    input1.gyro = {0.010f, -0.50f, 0.80f};
    PadPublishInputForPort_nid_postfix(1, input1);

    PadData data0{};
    Require(scePadReadState(handle0, &data0) == PAD_OK);
    Require(std::fabs(data0.acceleration_x - 0.5f) < 1e-4f);
    Require(std::fabs(data0.acceleration_y - 1.0f) < 1e-4f);
    Require(data0.angular_velocity_x == 0.010f);
    Require(data0.angular_velocity_y == 0.20f);
    Require(data0.angular_velocity_z == -0.30f);

    PadData data1{};
    Require(scePadReadState(handle1, &data1) == PAD_OK);
    Require(data1.acceleration_x == 0.0f);
    Require(std::fabs(data1.acceleration_y - (-1.0f)) < 1e-4f);
    Require(std::fabs(data1.acceleration_z - 0.5f) < 1e-4f);
    Require(data1.angular_velocity_x == 0.0f);
    Require(data1.angular_velocity_y == -0.50f);
    Require(data1.angular_velocity_z == 0.80f);

    Require(scePadSetMotionSensorState(handle1, false) == PAD_OK);
    Require(scePadReadState(handle1, &data1) == PAD_OK);
    Require(data1.acceleration_x == 0.0f && data1.acceleration_y == 1.0f && data1.acceleration_z == 0.0f);
    Require(data1.angular_velocity_x == 0.0f && data1.angular_velocity_y == 0.0f && data1.angular_velocity_z == 0.0f);

    Require(scePadReadState(handle0, &data0) == PAD_OK);
    Require(std::fabs(data0.acceleration_x - 0.5f) < 1e-4f);
    Require(data0.angular_velocity_x == 0.010f);

    PadPublishInputForPort_nid_postfix(0, PadInputState{});
    PadPublishInputForPort_nid_postfix(1, PadInputState{});
    Require(scePadClose_nid_postfix(handle1) == PAD_OK);
}

int main() {
    CheckUninitializedState();

    Require(scePadInit_nid_postfix() == PAD_OK);

    const int handle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle > 0);

    CheckInvalidHandlesAndArgs(handle);
    CheckMotionQueries(handle);
    CheckAccelerometerBounds(handle);
    CheckGyroAngularVelocityUnits(handle);
    CheckCalibrationAndTiltCorrection(handle);

    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    CheckSpecialHandleMotion(special);
    Require(scePadClose_nid_postfix(special) == PAD_OK);

    CheckMultiPortMotionIsolation(handle);

    Require(scePadClose_nid_postfix(handle) == PAD_OK);

    Require(scePadSetMotionSensorState(handle, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadResetOrientation(handle) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetAngularVelocityDeadbandState(handle, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetTiltCorrectionState(handle, true) == PAD_ERROR_INVALID_HANDLE);

    return 0;
}
