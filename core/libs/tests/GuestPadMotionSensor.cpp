#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include "prx/libScePad/libScePad.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {

constexpr int SCE_PAD_ERROR_INVALID_ARG = static_cast<int>(0x80920001);
constexpr int SCE_PAD_ERROR_INVALID_HANDLE = static_cast<int>(0x80920003);
constexpr int SCE_PAD_ERROR_NOT_INITIALIZED = static_cast<int>(0x80920005);
constexpr int SCE_PAD_ERROR_NOT_CONNECTED = static_cast<int>(0x80920007);
constexpr float kStandardGravity = 9.80665f;

int handle = -1;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "guest_pad_motion_sensor failed: %s\n", message);
    std::exit(1);
}

bool FloatNear(float a, float b, float epsilon = 1e-4f) {
    return std::abs(a - b) <= epsilon;
}

void TestUninitializedState() {
    Require(PAD_ERROR_INVALID_ARG == SCE_PAD_ERROR_INVALID_ARG, "PAD_ERROR_INVALID_ARG equality");
    Require(PAD_ERROR_INVALID_HANDLE == SCE_PAD_ERROR_INVALID_HANDLE, "PAD_ERROR_INVALID_HANDLE equality");
    Require(PAD_ERROR_NOT_INITIALIZED == SCE_PAD_ERROR_NOT_INITIALIZED, "PAD_ERROR_NOT_INITIALIZED equality");
    Require(PAD_ERROR_DEVICE_NOT_CONNECTED == SCE_PAD_ERROR_NOT_CONNECTED, "PAD_ERROR_DEVICE_NOT_CONNECTED equality");

    PadData data{};
    PadMotionSensorPosition position{};
    float timerUnit = -1.0f;

    for (int testHandle : {-1, 0, 1, 99}) {
        Require(scePadReadState(testHandle, nullptr) == SCE_PAD_ERROR_INVALID_ARG, "read state null uninit");
        Require(scePadReadState(testHandle, &data) == SCE_PAD_ERROR_NOT_INITIALIZED, "read state uninit");
        Require(scePadRead_nid_postfix(testHandle, &data, 1) == SCE_PAD_ERROR_NOT_INITIALIZED, "read uninit");
        Require(scePadRead_nid_postfix(testHandle, nullptr, 1) == SCE_PAD_ERROR_NOT_INITIALIZED, "read null uninit");
        Require(scePadGetMotionSensorPosition(testHandle, nullptr) == SCE_PAD_ERROR_NOT_INITIALIZED, "motion sensor pos null uninit");
        Require(scePadGetMotionSensorPosition(testHandle, &position) == SCE_PAD_ERROR_NOT_INITIALIZED, "motion sensor pos uninit");
        Require(scePadGetMotionTimerUnit(testHandle, nullptr) == SCE_PAD_ERROR_NOT_INITIALIZED, "motion timer unit null uninit");
        Require(scePadGetMotionTimerUnit(testHandle, &timerUnit) == SCE_PAD_ERROR_NOT_INITIALIZED, "motion timer unit uninit");
    }

    Require(scePadInit_nid_postfix() == PAD_OK, "scePadInit succeeded");
    handle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle > 0, "scePadOpen standard handle succeeded");
}

void TestInvalidHandleRejection() {
    PadData data{};
    PadMotionSensorPosition position{};
    float timerUnit = -1.0f;

    for (int invalidHandle : {-9999, -100, -2, -1, 0, handle + 10, 99, 9999}) {
        Require(scePadReadState(invalidHandle, &data) == SCE_PAD_ERROR_INVALID_HANDLE, "read state invalid handle");
        Require(scePadReadState(invalidHandle, nullptr) == SCE_PAD_ERROR_INVALID_ARG, "read state invalid handle null ptr");
        Require(scePadRead_nid_postfix(invalidHandle, &data, 1) == SCE_PAD_ERROR_INVALID_HANDLE, "read invalid handle");
        Require(scePadRead_nid_postfix(invalidHandle, nullptr, 1) == SCE_PAD_ERROR_INVALID_HANDLE, "read invalid handle null ptr");
        Require(scePadSetMotionSensorState(invalidHandle, true) == SCE_PAD_ERROR_INVALID_HANDLE, "set motion sensor invalid handle true");
        Require(scePadSetMotionSensorState(invalidHandle, false) == SCE_PAD_ERROR_INVALID_HANDLE, "set motion sensor invalid handle false");
        Require(scePadResetOrientation(invalidHandle) == SCE_PAD_ERROR_INVALID_HANDLE, "reset orientation invalid handle");
        Require(scePadSetTiltCorrectionState(invalidHandle, true) == SCE_PAD_ERROR_INVALID_HANDLE, "set tilt correction invalid handle true");
        Require(scePadSetTiltCorrectionState(invalidHandle, false) == SCE_PAD_ERROR_INVALID_HANDLE, "set tilt correction invalid handle false");
        Require(scePadSetAngularVelocityDeadbandState(invalidHandle, true) == SCE_PAD_ERROR_INVALID_HANDLE, "set angular deadband invalid handle true");
        Require(scePadSetAngularVelocityDeadbandState(invalidHandle, false) == SCE_PAD_ERROR_INVALID_HANDLE, "set angular deadband invalid handle false");
        Require(scePadSetAngularVelocityBiasCorrectionState(invalidHandle, false) == SCE_PAD_ERROR_INVALID_HANDLE, "set bias correction invalid handle false");
        Require(scePadGetMotionSensorPosition(invalidHandle, &position) == SCE_PAD_ERROR_INVALID_HANDLE, "get motion pos invalid handle");
        Require(scePadGetMotionSensorPosition(invalidHandle, nullptr) == SCE_PAD_ERROR_INVALID_ARG, "get motion pos invalid handle null ptr");
        Require(scePadGetMotionTimerUnit(invalidHandle, &timerUnit) == SCE_PAD_ERROR_INVALID_HANDLE, "get motion timer invalid handle");
        Require(scePadGetMotionTimerUnit(invalidHandle, nullptr) == SCE_PAD_ERROR_INVALID_ARG, "get motion timer invalid handle null ptr");
    }
}

void TestNullPointerValidation() {
    Require(scePadReadState(handle, nullptr) == SCE_PAD_ERROR_INVALID_ARG, "read state null pointer");
    Require(scePadRead_nid_postfix(handle, nullptr, 1) == SCE_PAD_ERROR_INVALID_ARG, "read null pointer");
    PadData data{};
    Require(scePadRead_nid_postfix(handle, &data, 0) == SCE_PAD_ERROR_INVALID_ARG, "read count zero");
    Require(scePadRead_nid_postfix(handle, &data, -1) == SCE_PAD_ERROR_INVALID_ARG, "read count negative");
    Require(scePadGetMotionSensorPosition(handle, nullptr) == SCE_PAD_ERROR_INVALID_ARG, "motion sensor pos null pointer");
    Require(scePadGetMotionTimerUnit(handle, nullptr) == SCE_PAD_ERROR_INVALID_ARG, "motion timer unit null pointer");
}

void TestRestStateAndGravityNormalization() {
    PadPublishInput_nid_postfix(PadInputState{});

    PadData data{};
    Require(scePadReadState(handle, &data) == PAD_OK, "read state rest pad ok");
    Require(data.connected, "rest controller connected");
    Require(data.connected_count >= 1, "rest controller connected count");

    Require(FloatNear(data.orientation_x, 0.0f), "rest orientation x zero");
    Require(FloatNear(data.orientation_y, 0.0f), "rest orientation y zero");
    Require(FloatNear(data.orientation_z, 0.0f), "rest orientation z zero");
    Require(FloatNear(data.orientation_w, 1.0f), "rest orientation w unity");

    Require(FloatNear(data.acceleration_x, 0.0f), "rest acceleration x zero");
    Require(FloatNear(data.acceleration_y, 1.0f), "rest acceleration y 1g");
    Require(FloatNear(data.acceleration_z, 0.0f), "rest acceleration z zero");

    const float gravityMagnitude = std::sqrt(
        data.acceleration_x * data.acceleration_x +
        data.acceleration_y * data.acceleration_y +
        data.acceleration_z * data.acceleration_z
    );
    Require(FloatNear(gravityMagnitude, 1.0f), "rest gravity magnitude normalized to 1g");

    Require(FloatNear(data.angular_velocity_x, 0.0f), "rest angular velocity x zero");
    Require(FloatNear(data.angular_velocity_y, 0.0f), "rest angular velocity y zero");
    Require(FloatNear(data.angular_velocity_z, 0.0f), "rest angular velocity z zero");
}

void TestAccelerometerDynamicRange() {
    Require(scePadSetMotionSensorState(handle, true) == PAD_OK, "enable motion sensor");

    PadInputState input{};
    input.hasMotion = true;
    PadData data{};

    input.accel = {2.0f * kStandardGravity, -2.0f * kStandardGravity, 0.0f};
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK, "read state 2g range");
    Require(FloatNear(data.acceleration_x, 2.0f), "accel x positive 2g");
    Require(FloatNear(data.acceleration_y, -2.0f), "accel y negative 2g");
    Require(FloatNear(data.acceleration_z, 0.0f), "accel z zero");

    input.accel = {-4.0f * kStandardGravity, 4.0f * kStandardGravity, -4.0f * kStandardGravity};
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK, "read state 4g range");
    Require(FloatNear(data.acceleration_x, -4.0f), "accel x negative 4g");
    Require(FloatNear(data.acceleration_y, 4.0f), "accel y positive 4g");
    Require(FloatNear(data.acceleration_z, -4.0f), "accel z negative 4g");

    input.accel = {8.0f * kStandardGravity, -8.0f * kStandardGravity, 8.0f * kStandardGravity};
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK, "read state 8g range");
    Require(FloatNear(data.acceleration_x, 8.0f), "accel x positive 8g");
    Require(FloatNear(data.acceleration_y, -8.0f), "accel y negative 8g");
    Require(FloatNear(data.acceleration_z, 8.0f), "accel z positive 8g");

    input.accel = {-16.0f * kStandardGravity, 16.0f * kStandardGravity, -16.0f * kStandardGravity};
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK, "read state 16g dynamic range limit");
    Require(FloatNear(data.acceleration_x, -16.0f), "accel x negative 16g boundary");
    Require(FloatNear(data.acceleration_y, 16.0f), "accel y positive 16g boundary");
    Require(FloatNear(data.acceleration_z, -16.0f), "accel z negative 16g boundary");

    const float unitNorm = 1.0f / std::sqrt(3.0f);
    input.accel = {unitNorm * kStandardGravity, unitNorm * kStandardGravity, unitNorm * kStandardGravity};
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK, "read state 1g spatial normalization");
    const float measuredMagnitude = std::sqrt(
        data.acceleration_x * data.acceleration_x +
        data.acceleration_y * data.acceleration_y +
        data.acceleration_z * data.acceleration_z
    );
    Require(FloatNear(measuredMagnitude, 1.0f), "spatial orientation gravity normalized to 1g");
}

void TestSensorCalibrationAndFilters() {
    Require(scePadSetTiltCorrectionState(handle, false) == PAD_OK, "tilt correction false");
    Require(scePadResetOrientation(handle) == PAD_OK, "reset orientation");
    Require(scePadSetTiltCorrectionState(handle, true) == PAD_OK, "tilt correction true");

    PadInputState movingInput{};
    movingInput.hasMotion = true;
    movingInput.accel = {0.0f, kStandardGravity, 0.0f};
    movingInput.gyro = {0.010f, 0.020f, -0.015f};
    PadPublishInput_nid_postfix(movingInput);

    PadData data{};
    Require(scePadReadState(handle, &data) == PAD_OK, "read moving input");
    Require(FloatNear(data.angular_velocity_x, 0.010f), "raw gyro x below deadband threshold");
    Require(FloatNear(data.angular_velocity_y, 0.020f), "raw gyro y above deadband threshold");
    Require(FloatNear(data.angular_velocity_z, -0.015f), "raw gyro z below deadband threshold");

    Require(scePadSetAngularVelocityDeadbandState(handle, true) == PAD_OK, "enable angular deadband");
    Require(scePadReadState(handle, &data) == PAD_OK, "read with deadband enabled");
    Require(FloatNear(data.angular_velocity_x, 0.0f), "deadband filtered x");
    Require(FloatNear(data.angular_velocity_y, 0.020f), "deadband preserved y");
    Require(FloatNear(data.angular_velocity_z, 0.0f), "deadband filtered z");

    Require(scePadSetAngularVelocityDeadbandState(handle, false) == PAD_OK, "disable angular deadband");
    Require(scePadReadState(handle, &data) == PAD_OK, "read with deadband disabled");
    Require(FloatNear(data.angular_velocity_x, 0.010f), "restored raw gyro x");
    Require(FloatNear(data.angular_velocity_z, -0.015f), "restored raw gyro z");

    Require(scePadSetAngularVelocityBiasCorrectionState(handle, false) == PAD_OK, "bias correction disable ok");

    Require(scePadSetMotionSensorState(handle, false) == PAD_OK, "disable motion sensor state");
    Require(scePadReadState(handle, &data) == PAD_OK, "read disabled motion sensor");
    Require(FloatNear(data.acceleration_x, 0.0f), "disabled sensor defaults accel x zero");
    Require(FloatNear(data.acceleration_y, 1.0f), "disabled sensor defaults accel y 1g");
    Require(FloatNear(data.acceleration_z, 0.0f), "disabled sensor defaults accel z zero");
    Require(FloatNear(data.angular_velocity_x, 0.0f), "disabled sensor defaults gyro x zero");
    Require(FloatNear(data.angular_velocity_y, 0.0f), "disabled sensor defaults gyro y zero");
    Require(FloatNear(data.angular_velocity_z, 0.0f), "disabled sensor defaults gyro z zero");

    Require(scePadSetMotionSensorState(handle, true) == PAD_OK, "re-enable motion sensor state");
    Require(scePadReadState(handle, &data) == PAD_OK, "read re-enabled motion sensor");
    Require(FloatNear(data.acceleration_y, 1.0f), "re-enabled sensor accel y live");
}

void TestCloseAndPostCloseQueries() {
    Require(scePadClose_nid_postfix(handle) == PAD_OK, "scePadClose succeeded");
    Require(!scePadIsValidHandle(handle, PAD_PORT_TYPE_STANDARD), "closed handle not valid standard");

    PadMotionSensorPosition position{};
    float timerUnit = -1.0f;

    Require(scePadGetMotionSensorPosition(handle, &position) == SCE_PAD_ERROR_INVALID_HANDLE, "closed handle get motion pos");
    Require(scePadGetMotionTimerUnit(handle, &timerUnit) == SCE_PAD_ERROR_INVALID_HANDLE, "closed handle get motion timer");
}

} // namespace

int main() {
    TestUninitializedState();
    TestInvalidHandleRejection();
    TestNullPointerValidation();
    TestRestStateAndGravityNormalization();
    TestAccelerometerDynamicRange();
    TestSensorCalibrationAndFilters();
    TestCloseAndPostCloseQueries();
    return 0;
}
