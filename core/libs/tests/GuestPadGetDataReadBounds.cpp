#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "guest_pad_get_data_read_bounds: %s\n", message);
    std::abort();
}

void TestPreInitializationBounds() {
    PadData data{};

    Require(scePadReadState(1, nullptr) == PAD_ERROR_INVALID_ARG, "null data check in scePadReadState must precede NOT_INITIALIZED");
    Require(scePadReadState(-1, nullptr) == PAD_ERROR_INVALID_ARG, "null data check in scePadReadState must precede invalid handle");

    for (int handle : {-100, -1, 0, 1, 2, 16, 32, 100, 9999}) {
        Require(scePadReadState(handle, &data) == PAD_ERROR_NOT_INITIALIZED, "pre-init scePadReadState must return NOT_INITIALIZED");
        Require(scePadRead_nid_postfix(handle, &data, 1) == PAD_ERROR_NOT_INITIALIZED, "pre-init scePadRead must return NOT_INITIALIZED");
        Require(scePadRead_nid_postfix(handle, nullptr, 1) == PAD_ERROR_NOT_INITIALIZED, "pre-init scePadRead null must return NOT_INITIALIZED");
        Require(scePadRead_nid_postfix(handle, &data, 0) == PAD_ERROR_NOT_INITIALIZED, "pre-init scePadRead count 0 must return NOT_INITIALIZED");
        Require(scePadRead_nid_postfix(handle, nullptr, -1) == PAD_ERROR_NOT_INITIALIZED, "pre-init scePadRead count -1 must return NOT_INITIALIZED");
    }
}

void TestPostInitializationUnopenedHandleBounds() {
    Require(scePadInit_nid_postfix() == PAD_OK, "scePadInit must return PAD_OK");
    Require(scePadInit_nid_postfix() == PAD_OK, "repeat scePadInit must return PAD_OK");

    PadData data{};
    for (int handle : {-500, -1, 0, 50, 500}) {
        Require(scePadReadState(handle, nullptr) == PAD_ERROR_INVALID_ARG, "null check in scePadReadState precedes invalid handle");
        Require(scePadReadState(handle, &data) == PAD_ERROR_INVALID_HANDLE, "invalid handle scePadReadState must return INVALID_HANDLE");

        Require(scePadRead_nid_postfix(handle, &data, 1) == PAD_ERROR_INVALID_HANDLE, "invalid handle scePadRead must return INVALID_HANDLE");
        Require(scePadRead_nid_postfix(handle, nullptr, 1) == PAD_ERROR_INVALID_ARG, "null buffer in scePadRead precedes handle validation");
        Require(scePadRead_nid_postfix(handle, &data, 0) == PAD_ERROR_INVALID_ARG, "count 0 in scePadRead precedes handle validation");
        Require(scePadRead_nid_postfix(handle, nullptr, -1) == PAD_ERROR_INVALID_ARG, "count -1 in scePadRead precedes handle validation");
    }
}

void TestStandardHandleReadBounds() {
    const int handle0 = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle0 > 0, "open port 0 failed");

    PadData data{};
    Require(scePadReadState(handle0, nullptr) == PAD_ERROR_INVALID_ARG, "open handle scePadReadState null data must return INVALID_ARG");
    Require(scePadRead_nid_postfix(handle0, nullptr, 1) == PAD_ERROR_INVALID_ARG, "open handle scePadRead null buffer must return INVALID_ARG");
    Require(scePadRead_nid_postfix(handle0, &data, 0) == PAD_ERROR_INVALID_ARG, "open handle scePadRead count 0 must return INVALID_ARG");
    Require(scePadRead_nid_postfix(handle0, &data, -1) == PAD_ERROR_INVALID_ARG, "open handle scePadRead count -1 must return INVALID_ARG");
    Require(scePadRead_nid_postfix(handle0, nullptr, 0) == PAD_ERROR_INVALID_ARG, "open handle scePadRead null and count 0 must return INVALID_ARG");

    Require(scePadReadState(handle0, &data) == 0, "open handle scePadReadState failed");
    Require(data.left_stick_x == 128 && data.left_stick_y == 128 && data.right_stick_x == 128 && data.right_stick_y == 128, "default sticks neutral");
    Require(data.analog_buttons_l2 == 0 && data.analog_buttons_r2 == 0, "default triggers zero");
    Require(data.buttons == 0, "default buttons zero");
    Require(data.orientation_w == 1.0f && data.orientation_x == 0.0f && data.orientation_y == 0.0f && data.orientation_z == 0.0f, "default orientation identity");
    Require(data.touch_data_touch_num == 0, "default touch count zero");

    PadData buffer[4]{};
    const int readCount = scePadRead_nid_postfix(handle0, buffer, 4);
    Require(readCount >= 1, "scePadRead must return at least 1 sample");
    Require(buffer[0].left_stick_x == 128, "scePadRead sample stick neutral");
}

void TestTouchDataBounds() {
    const int handle0 = 1;
    PadData data{};

    PadInputState input{};
    input.touch[0].active = true;
    input.touch[0].x = 500;
    input.touch[0].y = 800;
    input.touch[0].id = 42;
    input.touch[1].active = false;
    PadPublishInputForPort_nid_postfix(0, input);

    Require(scePadReadState(handle0, &data) == 0, "read state single touch failed");
    Require(data.touch_data_touch_num == 1, "touch count must be 1");
    Require(data.touch_data_touch0_x == 500, "touch0 x mismatch");
    Require(data.touch_data_touch0_y == 800, "touch0 y mismatch");
    Require(data.touch_data_touch0_id == 42, "touch0 id mismatch");

    input.touch[0].active = true;
    input.touch[0].x = 100;
    input.touch[0].y = 200;
    input.touch[0].id = 1;
    input.touch[1].active = true;
    input.touch[1].x = 1800;
    input.touch[1].y = 950;
    input.touch[1].id = 2;
    PadPublishInputForPort_nid_postfix(0, input);

    Require(scePadReadState(handle0, &data) == 0, "read state dual touch failed");
    Require(data.touch_data_touch_num == 2, "touch count must be 2");
    Require(data.touch_data_touch0_x == 100 && data.touch_data_touch0_y == 200 && data.touch_data_touch0_id == 1, "touch0 dual values mismatch");
    Require(data.touch_data_touch1_x == 1800 && data.touch_data_touch1_y == 950 && data.touch_data_touch1_id == 2, "touch1 dual values mismatch");

    input.touch[0].x = 0;
    input.touch[0].y = 0;
    input.touch[0].id = 0;
    input.touch[1].x = 1920;
    input.touch[1].y = 1080;
    input.touch[1].id = 255;
    PadPublishInputForPort_nid_postfix(0, input);

    Require(scePadReadState(handle0, &data) == 0, "read state touch boundaries failed");
    Require(data.touch_data_touch_num == 2, "boundary touch count must be 2");
    Require(data.touch_data_touch0_x == 0 && data.touch_data_touch0_y == 0 && data.touch_data_touch0_id == 0, "touch 0 boundary mismatch");
    Require(data.touch_data_touch1_x == 1920 && data.touch_data_touch1_y == 1080 && data.touch_data_touch1_id == 255, "touch 1 boundary mismatch");

    input.touch[0].active = false;
    input.touch[1].active = false;
    PadPublishInputForPort_nid_postfix(0, input);

    Require(scePadReadState(handle0, &data) == 0, "read state released touch failed");
    Require(data.touch_data_touch_num == 0, "released touch count must be 0");
}

void TestMotionButtonsSticksBounds() {
    const int handle0 = 1;
    PadData data{};

    PadInputState input{};
    input.hasMotion = true;
    input.accel = {1.5f, -9.81f, 0.25f};
    input.gyro = {0.1f, -0.2f, 0.5f};
    input.buttons = 0x0000FFFFu;
    input.sticks = {0, 255, 255, 0};
    input.analogButtonsL2 = 255;
    input.analogButtonsR2 = 128;
    PadPublishInputForPort_nid_postfix(0, input);

    Require(scePadReadState(handle0, &data) == 0, "read state motion and controls failed");
    Require(std::fabs(data.acceleration_x - 1.5f) < 1e-4f, "acceleration x mismatch");
    Require(std::fabs(data.acceleration_y - (-9.81f)) < 1e-4f, "acceleration y mismatch");
    Require(std::fabs(data.acceleration_z - 0.25f) < 1e-4f, "acceleration z mismatch");
    Require(data.buttons == 0x0000FFFFu, "buttons mismatch");
    Require(data.left_stick_x == 0 && data.left_stick_y == 255, "left stick mismatch");
    Require(data.right_stick_x == 255 && data.right_stick_y == 0, "right stick mismatch");
    Require(data.analog_buttons_l2 == 255 && data.analog_buttons_r2 == 128, "analog triggers mismatch");

    input.buttons = 0xFFFFFFFFu;
    input.sticks = {128, 128, 128, 128};
    input.analogButtonsL2 = 0;
    input.analogButtonsR2 = 0;
    PadPublishInputForPort_nid_postfix(0, input);

    Require(scePadReadState(handle0, &data) == 0, "read state full buttons failed");
    Require(data.buttons == 0xFFFFFFFFu, "full buttons mismatch");
    Require(data.left_stick_x == 128 && data.left_stick_y == 128, "sticks neutral mismatch");
    Require(data.analog_buttons_l2 == 0 && data.analog_buttons_r2 == 0, "triggers zero mismatch");
}

void TestMultiControllerIsolation() {
    const int handle1 = scePadOpen_nid_postfix(0x10000001, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle1 > 0, "open handle1 failed");

    PadInputState in0{};
    in0.buttons = 0x11111111u;
    in0.sticks = {10, 20, 30, 40};
    PadPublishInputForPort_nid_postfix(0, in0);

    PadInputState in1{};
    in1.buttons = 0x22222222u;
    in1.sticks = {50, 60, 70, 80};
    PadPublishInputForPort_nid_postfix(1, in1);

    PadData d0{};
    Require(scePadReadState(1, &d0) == 0, "read port 0 failed");
    Require(d0.buttons == 0x11111111u, "port 0 buttons mismatch");
    Require(d0.left_stick_x == 10 && d0.left_stick_y == 20, "port 0 left stick mismatch");

    PadData d1{};
    Require(scePadReadState(handle1, &d1) == 0, "read port 1 failed");
    Require(d1.buttons == 0x22222222u, "port 1 buttons mismatch");
    Require(d1.left_stick_x == 50 && d1.left_stick_y == 60, "port 1 left stick mismatch");

    Require(scePadClose_nid_postfix(handle1) == PAD_OK, "close handle1 failed");
}

void TestSpecialAndRemoteHandleRead() {
    const int specialHandle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(specialHandle >= PAD_SPECIAL_HANDLE, "open special failed");

    PadData data{};
    Require(scePadReadState(specialHandle, nullptr) == PAD_ERROR_INVALID_ARG, "special handle null readState must return INVALID_ARG");
    Require(scePadRead_nid_postfix(specialHandle, nullptr, 1) == PAD_ERROR_INVALID_ARG, "special handle null read must return INVALID_ARG");
    Require(scePadRead_nid_postfix(specialHandle, &data, 0) == PAD_ERROR_INVALID_ARG, "special handle count 0 must return INVALID_ARG");

    Require(scePadReadState(specialHandle, &data) == 0, "special readState failed");
    Require(data.left_stick_x == 0x80 && data.left_stick_y == 0x80 && data.right_stick_x == 0x80 && data.right_stick_y == 0x80, "special sticks neutral");
    Require(data.orientation_w == 1.0f && data.acceleration_y == 1.0f, "special orientation and acceleration mismatch");

    PadData specialBuffer[2]{};
    Require(scePadRead_nid_postfix(specialHandle, specialBuffer, 2) == 1, "special scePadRead must return 1 sample");
    Require(specialBuffer[0].orientation_w == 1.0f, "special buffer orientation mismatch");

    Require(scePadClose_nid_postfix(specialHandle) == PAD_OK, "close special failed");
    Require(scePadReadState(specialHandle, &data) == PAD_ERROR_INVALID_HANDLE, "closed special readState must return INVALID_HANDLE");
    Require(scePadRead_nid_postfix(specialHandle, &data, 1) == PAD_ERROR_INVALID_HANDLE, "closed special scePadRead must return INVALID_HANDLE");

    const int remoteHandle = scePadOpen_nid_postfix(PAD_USER_ID_SYSTEM, PAD_PORT_TYPE_REMOTE, 0, nullptr);
    Require(remoteHandle == PAD_REMOTE_HANDLE, "open remote failed");

    Require(scePadReadState(remoteHandle, nullptr) == PAD_ERROR_INVALID_ARG, "remote null readState must return INVALID_ARG");
    Require(scePadRead_nid_postfix(remoteHandle, nullptr, 1) == PAD_ERROR_INVALID_ARG, "remote null read must return INVALID_ARG");

    Require(scePadReadState(remoteHandle, &data) == 0, "remote readState failed");
    Require(data.buttons == 0 && !data.connected && data.left_stick_x == 0, "remote readState must return zeroed PadData");

    PadData remoteBuffer[2]{};
    Require(scePadRead_nid_postfix(remoteHandle, remoteBuffer, 2) == 1, "remote scePadRead must return 1 sample");
    Require(remoteBuffer[0].buttons == 0 && !remoteBuffer[0].connected, "remote buffer zeroed");

    Require(scePadClose_nid_postfix(remoteHandle) == PAD_OK, "close remote failed");
    Require(scePadReadState(remoteHandle, &data) == PAD_ERROR_INVALID_HANDLE, "closed remote readState must return INVALID_HANDLE");
    Require(scePadRead_nid_postfix(remoteHandle, &data, 1) == PAD_ERROR_INVALID_HANDLE, "closed remote scePadRead must return INVALID_HANDLE");
}

} // namespace

int main() {
    TestPreInitializationBounds();
    TestPostInitializationUnopenedHandleBounds();
    TestStandardHandleReadBounds();
    TestTouchDataBounds();
    TestMotionButtonsSticksBounds();
    TestMultiControllerIsolation();
    TestSpecialAndRemoteHandleRead();
    return 0;
}
