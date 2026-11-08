#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <cmath>
#include <cstdlib>
#include <initializer_list>

namespace {

void Require(bool condition) {
    if (!condition) std::abort();
}

void CheckUninitializedState() {
    PadData data{};
    Require(scePadReadState(1, &data) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadRead_nid_postfix(1, &data, 1) == PAD_ERROR_NOT_INITIALIZED);
    for (int handle : {-1, 0, 1, 100}) {
        const int deadbandResult = scePadSetAngularVelocityDeadbandState(handle, true);
        Require(deadbandResult == PAD_ERROR_NOT_INITIALIZED || deadbandResult == PAD_ERROR_INVALID_HANDLE);
        const int biasResult = scePadSetAngularVelocityBiasCorrectionState(handle, false);
        Require(biasResult == PAD_ERROR_NOT_INITIALIZED || biasResult == PAD_ERROR_INVALID_HANDLE);
    }
}

void CheckInvalidHandlesAndNullPointers(int validHandle) {
    PadData data{};
    PadDeviceClassData classData{};

    Require(scePadReadState(validHandle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadReadState(validHandle, nullptr) == static_cast<int>(0x80920001));
    Require(scePadRead_nid_postfix(validHandle, nullptr, 1) == PAD_ERROR_INVALID_ARG);
    Require(scePadRead_nid_postfix(validHandle, nullptr, 1) == static_cast<int>(0x80920001));
    Require(scePadRead_nid_postfix(validHandle, &data, 0) == PAD_ERROR_INVALID_ARG);
    Require(scePadRead_nid_postfix(validHandle, &data, -1) == PAD_ERROR_INVALID_ARG);
    Require(scePadDeviceClassParseData(validHandle, nullptr, &classData) == PAD_ERROR_INVALID_ARG);
    Require(scePadDeviceClassParseData(validHandle, &data, nullptr) == PAD_ERROR_INVALID_ARG);

    for (int invalidHandle : {-9999, -100, -1, 0, 100, validHandle + 10, 9999}) {
        Require(scePadReadState(invalidHandle, nullptr) == static_cast<int>(0x80920001));
        Require(scePadReadState(invalidHandle, &data) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadRead_nid_postfix(invalidHandle, &data, 1) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadbandState(invalidHandle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadbandState(invalidHandle, false) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityBiasCorrectionState(invalidHandle, false) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityBiasCorrectionState(invalidHandle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetMotionSensorState(invalidHandle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetMotionSensorState(invalidHandle, false) == PAD_ERROR_INVALID_HANDLE);
    }
}

void CheckAngularVelocityStructureFields(int handle) {
    PadInputState input{};
    PadPublishInput_nid_postfix(input);

    PadData data{};
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.0f);
    Require(data.angular_velocity_y == 0.0f);
    Require(data.angular_velocity_z == 0.0f);

    input.hasMotion = true;
    input.accel = {0.0f, 9.80665f, 0.0f};
    input.gyro = {0.125f, -0.25f, 0.5f};
    PadPublishInput_nid_postfix(input);

    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(std::fabs(data.angular_velocity_x - 0.125f) < 1e-6f);
    Require(std::fabs(data.angular_velocity_y - (-0.25f)) < 1e-6f);
    Require(std::fabs(data.angular_velocity_z - 0.5f) < 1e-6f);

    PadData dataRead{};
    Require(scePadRead_nid_postfix(handle, &dataRead, 1) == 1);
    Require(std::fabs(dataRead.angular_velocity_x - 0.125f) < 1e-6f);
    Require(std::fabs(dataRead.angular_velocity_y - (-0.25f)) < 1e-6f);
    Require(std::fabs(dataRead.angular_velocity_z - 0.5f) < 1e-6f);

    input.gyro = {0.75f, 0.0f, 0.0f};
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(std::fabs(data.angular_velocity_x - 0.75f) < 1e-6f);
    Require(data.angular_velocity_y == 0.0f);
    Require(data.angular_velocity_z == 0.0f);

    input.gyro = {0.0f, -0.45f, 0.0f};
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.0f);
    Require(std::fabs(data.angular_velocity_y - (-0.45f)) < 1e-6f);
    Require(data.angular_velocity_z == 0.0f);

    input.gyro = {0.0f, 0.0f, 1.25f};
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.0f);
    Require(data.angular_velocity_y == 0.0f);
    Require(std::fabs(data.angular_velocity_z - 1.25f) < 1e-6f);

    PadPublishInput_nid_postfix(PadInputState{});
}

void CheckDeadbandThresholdFiltering(int handle) {
    Require(scePadSetAngularVelocityDeadbandState(handle, false) == PAD_OK);

    PadInputState input{};
    input.hasMotion = true;
    input.accel = {0.0f, 9.80665f, 0.0f};
    input.gyro = {0.010f, -0.012f, 0.015f};
    PadPublishInput_nid_postfix(input);

    PadData data{};
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(std::fabs(data.angular_velocity_x - 0.010f) < 1e-6f);
    Require(std::fabs(data.angular_velocity_y - (-0.012f)) < 1e-6f);
    Require(std::fabs(data.angular_velocity_z - 0.015f) < 1e-6f);

    Require(scePadSetAngularVelocityDeadbandState(handle, true) == PAD_OK);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.0f);
    Require(data.angular_velocity_y == 0.0f);
    Require(data.angular_velocity_z == 0.0f);

    input.gyro = {0.010f, 0.025f, -0.030f};
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.0f);
    Require(std::fabs(data.angular_velocity_y - 0.025f) < 1e-6f);
    Require(std::fabs(data.angular_velocity_z - (-0.030f)) < 1e-6f);

    input.gyro = {-0.015f, 0.008f, 0.050f};
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.0f);
    Require(data.angular_velocity_y == 0.0f);
    Require(std::fabs(data.angular_velocity_z - 0.050f) < 1e-6f);

    input.gyro = {0.0174f, -0.0174f, 0.0175f};
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.0f);
    Require(data.angular_velocity_y == 0.0f);
    Require(std::fabs(data.angular_velocity_z - 0.0175f) < 1e-6f);

    input.gyro = {-0.0175f, 0.0174f, -0.0174f};
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(std::fabs(data.angular_velocity_x - (-0.0175f)) < 1e-6f);
    Require(data.angular_velocity_y == 0.0f);
    Require(data.angular_velocity_z == 0.0f);

    Require(scePadSetAngularVelocityDeadbandState(handle, false) == PAD_OK);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(std::fabs(data.angular_velocity_x - (-0.0175f)) < 1e-6f);
    Require(std::fabs(data.angular_velocity_y - 0.0174f) < 1e-6f);
    Require(std::fabs(data.angular_velocity_z - (-0.0174f)) < 1e-6f);

    PadPublishInput_nid_postfix(PadInputState{});
}

void CheckSensitivityAndDynamicRange(int handle) {
    PadData data{};
    PadInputState input{};
    input.hasMotion = true;
    input.accel = {0.0f, 9.80665f, 0.0f};

    const float dynamicRates[] = {
        0.05f, 0.1f, 0.5f, 1.0f, 2.5f, 5.0f, 10.0f, 20.0f, 34.9f
    };

    for (float rate : dynamicRates) {
        input.gyro = {rate, -rate, rate * 0.5f};
        PadPublishInput_nid_postfix(input);
        Require(scePadReadState(handle, &data) == PAD_OK);
        Require(std::fabs(data.angular_velocity_x - rate) < 1e-5f);
        Require(std::fabs(data.angular_velocity_y - (-rate)) < 1e-5f);
        Require(std::fabs(data.angular_velocity_z - (rate * 0.5f)) < 1e-5f);
    }

    input.gyro = {5.0f, -5.0f, 10.0f};
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(std::fabs(data.angular_velocity_x - 5.0f) < 1e-5f);

    Require(scePadSetMotionSensorState(handle, false) == PAD_OK);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.0f);
    Require(data.angular_velocity_y == 0.0f);
    Require(data.angular_velocity_z == 0.0f);

    Require(scePadSetMotionSensorState(handle, true) == PAD_OK);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(std::fabs(data.angular_velocity_x - 5.0f) < 1e-5f);
    Require(std::fabs(data.angular_velocity_y - (-5.0f)) < 1e-5f);
    Require(std::fabs(data.angular_velocity_z - 10.0f) < 1e-5f);

    Require(scePadSetTiltCorrectionState(handle, true) == PAD_OK);
    Require(scePadSetTiltCorrectionState(handle, false) == PAD_OK);
    Require(scePadSetAngularVelocityBiasCorrectionState(handle, false) == PAD_OK);
    Require(scePadSetAngularVelocityBiasCorrectionState(handle, true) == PAD_OK);

    PadPublishInput_nid_postfix(PadInputState{});
}

void CheckSpecialHandleAndCleanup(int validHandle) {
    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);
    Require(scePadSetAngularVelocityDeadbandState(special, true) == PAD_OK);
    Require(scePadSetAngularVelocityDeadbandState(special, false) == PAD_OK);
    Require(scePadSetMotionSensorState(special, true) == PAD_OK);
    Require(scePadSetTiltCorrectionState(special, true) == PAD_OK);
    Require(scePadClose_nid_postfix(special) == PAD_OK);

    PadPublishInput_nid_postfix(PadInputState{});
    Require(scePadClose_nid_postfix(validHandle) == PAD_OK);

    PadData data{};
    Require(scePadSetAngularVelocityDeadbandState(validHandle, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadReadState(validHandle, &data) == PAD_ERROR_INVALID_HANDLE);
}

} // namespace

int main() {
    CheckUninitializedState();

    Require(scePadInit_nid_postfix() == PAD_OK);
    const int handle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle > 0);

    CheckInvalidHandlesAndNullPointers(handle);
    CheckAngularVelocityStructureFields(handle);
    CheckDeadbandThresholdFiltering(handle);
    CheckSensitivityAndDynamicRange(handle);
    CheckSpecialHandleAndCleanup(handle);

    return 0;
}
