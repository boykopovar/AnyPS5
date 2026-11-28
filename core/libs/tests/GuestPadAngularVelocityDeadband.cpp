#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadRumbleIntensity.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <cstdint>
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

    const Pad::RumbleCommand fullZeroDuration = Pad::MakeRumbleCommand(255, 128, 100, 0);
    Require(fullZeroDuration == Pad::RumbleCommand{0, 0, 0});
    Require(!fullZeroDuration.Active());

    const Pad::RumbleCommand scaledZeroDuration = Pad::MakeRumbleCommand(255, 128, 50, 0);
    Require(scaledZeroDuration == Pad::RumbleCommand{0, 0, 0});
    Require(!scaledZeroDuration.Active());

    std::uint16_t sinkLow = 0xA5A5;
    std::uint16_t sinkHigh = 0xA5A5;
    std::uint32_t sinkDuration = 0xA5A5A5A5u;
    auto sink = [&](std::uint16_t low, std::uint16_t high, std::uint32_t durationMs) {
        sinkLow = low;
        sinkHigh = high;
        sinkDuration = durationMs;
    };

    Require(!Pad::SendRumble(sink, 255, 128, 100, 0));
    Require(sinkLow == 0 && sinkHigh == 0 && sinkDuration == 0);

    sinkLow = 0xA5A5;
    sinkHigh = 0xA5A5;
    sinkDuration = 0xA5A5A5A5u;
    Require(!Pad::SendRumble(sink, 255, 128, 50, 0));
    Require(sinkLow == 0 && sinkHigh == 0 && sinkDuration == 0);

    const Pad::RumbleCommand fullActive = Pad::MakeRumbleCommand(255, 128, 100, 500);
    Require(fullActive == Pad::RumbleCommand{65535, 32896, 500});
    Require(fullActive.Active());

    const Pad::RumbleCommand scaledActive = Pad::MakeRumbleCommand(255, 128, 50, 500);
    Require(scaledActive == Pad::RumbleCommand{32768, 16448, 500});
    Require(scaledActive.Active());

    const Pad::RumbleCommand zeroPercent = Pad::MakeRumbleCommand(255, 128, 0, 500);
    Require(zeroPercent == Pad::RumbleCommand{0, 0, 0});
    Require(!zeroPercent.Active());

    const Pad::RumbleCommand zeroMotors = Pad::MakeRumbleCommand(0, 0, 100, 500);
    Require(zeroMotors == Pad::RumbleCommand{0, 0, 0});
    Require(!zeroMotors.Active());

    Require(Pad::SendRumble(sink, 255, 128, 50, 500));
    Require(sinkLow == 32768 && sinkHigh == 16448 && sinkDuration == 500);

    return 0;
}
