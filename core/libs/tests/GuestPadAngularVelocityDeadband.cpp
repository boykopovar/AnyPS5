#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <cstdlib>
#include <initializer_list>

static void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

int main() {
    for (const int handle : {-9999, -1, 0, 1, 100, 12345}) {
        Require(scePadSetAngularVelocityDeadbandState(handle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadbandState(handle, false) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadInit_nid_postfix() == PAD_OK);

    for (const int handle : {-9999, -100, -1, 0, 100, 12345}) {
        Require(scePadSetAngularVelocityDeadbandState(handle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadbandState(handle, false) == PAD_ERROR_INVALID_HANDLE);
    }

    const int handle = scePadOpen_nid_postfix(0x10000000, 0, 0, nullptr);
    Require(handle > 0);

    Require(scePadSetAngularVelocityDeadbandState(handle, true) == PAD_OK);
    Require(scePadSetAngularVelocityDeadbandState(handle, false) == PAD_OK);

    for (const int invalidHandle : {-100, -1, 0, handle + 10, handle + 100, 9999}) {
        Require(scePadSetAngularVelocityDeadbandState(invalidHandle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadbandState(invalidHandle, false) == PAD_ERROR_INVALID_HANDLE);
    }

    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);
    Require(scePadSetAngularVelocityDeadbandState(special, true) == PAD_OK);
    Require(scePadSetAngularVelocityDeadbandState(special, false) == PAD_OK);
    Require(scePadClose_nid_postfix(special) == PAD_OK);
    Require(scePadSetAngularVelocityDeadbandState(special, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetAngularVelocityDeadbandState(special, false) == PAD_ERROR_INVALID_HANDLE);

    PadInputState moving{};
    moving.hasMotion = true;
    moving.accel = {0.0f, 9.80665f, 0.0f};

    moving.gyro = {0.010f, 0.025f, -0.015f};
    PadPublishInput_nid_postfix(moving);

    PadData data{};
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.010f);
    Require(data.angular_velocity_y == 0.025f);
    Require(data.angular_velocity_z == -0.015f);

    Require(scePadSetAngularVelocityDeadbandState(handle, true) == PAD_OK);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.0f);
    Require(data.angular_velocity_y == 0.025f);
    Require(data.angular_velocity_z == 0.0f);

    moving.gyro = {0.0174540f, -0.0174540f, 0.0174530f};
    PadPublishInput_nid_postfix(moving);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.0174540f);
    Require(data.angular_velocity_y == -0.0174540f);
    Require(data.angular_velocity_z == 0.0f);

    moving.gyro = {-0.0174530f, 0.500f, -0.500f};
    PadPublishInput_nid_postfix(moving);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.angular_velocity_x == 0.0f);
    Require(data.angular_velocity_y == 0.500f);
    Require(data.angular_velocity_z == -0.500f);

    Require(scePadSetAngularVelocityDeadbandState(handle, false) == PAD_OK);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.angular_velocity_x == -0.0174530f);
    Require(data.angular_velocity_y == 0.500f);
    Require(data.angular_velocity_z == -0.500f);

    PadPublishInput_nid_postfix(PadInputState{});
    Require(scePadClose_nid_postfix(handle) == PAD_OK);

    return 0;
}
