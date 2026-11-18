#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestUninitializedAndInvalidHandleBounds() {
    PadData data{};
    PadControllerInformation info{};

    Require(scePadReadState(1, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadReadState(1, &data) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadRead_nid_postfix(1, &data, 1) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadRead_nid_postfix(1, nullptr, 1) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadGetControllerInformation(1, &info) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadGetControllerInformation(1, nullptr) == PAD_ERROR_NOT_INITIALIZED);

    Require(scePadInit_nid_postfix() == PAD_OK);

    for (int handle : {-9999, -1, 0, 1, 0x10, 0x20, 9999}) {
        Require(scePadReadState(handle, nullptr) == PAD_ERROR_INVALID_ARG);
        Require(scePadReadState(handle, &data) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadRead_nid_postfix(handle, &data, 1) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadRead_nid_postfix(handle, nullptr, 1) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadRead_nid_postfix(handle, &data, 0) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadRead_nid_postfix(handle, &data, -1) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadGetControllerInformation(handle, &info) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadGetControllerInformation(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
    }
}

void TestTouchResolutionAndInformationBounds(int handle) {
    PadControllerInformation info{};
    Require(scePadGetControllerInformation(handle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetControllerInformation(handle, &info) == PAD_OK);
    Require(info.touchPadInfo.pixelDensity == 44.86f);
    Require(info.touchPadInfo.resolution.x == kTouchPadWidth);
    Require(info.touchPadInfo.resolution.y == kTouchPadHeight);
    Require(kTouchPadWidth == 1920);
    Require(kTouchPadHeight == 1080);
    Require(info.deviceClass == PAD_DEVICE_CLASS_STANDARD);
}

void TestTouchCoordinateBoundaryExtremes(int handle) {
    PadData data{};
    PadInputState input{};

    input.touch[0] = {true, 0, 0, 0};
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 1);
    Require(data.touch_data_touch0_x == 0);
    Require(data.touch_data_touch0_y == 0);

    input.touch[0] = {true, 1919, 0, 0};
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 1);
    Require(data.touch_data_touch0_x == 1919);
    Require(data.touch_data_touch0_y == 0);

    input.touch[0] = {true, 0, 1079, 0};
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 1);
    Require(data.touch_data_touch0_x == 0);
    Require(data.touch_data_touch0_y == 1079);

    input.touch[0] = {true, 1919, 1079, 0};
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 1);
    Require(data.touch_data_touch0_x == 1919);
    Require(data.touch_data_touch0_y == 1079);

    input.touch[0] = {true, 960, 540, 0};
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 1);
    Require(data.touch_data_touch0_x == 960);
    Require(data.touch_data_touch0_y == 540);

    input.touch[0] = {true, kTouchPadWidth, kTouchPadHeight, 0};
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 1);
    Require(data.touch_data_touch0_x == 1920);
    Require(data.touch_data_touch0_y == 1080);

    input.touch[0] = {true, 0xffff, 0xffff, 0};
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 1);
    Require(data.touch_data_touch0_x == 0xffff);
    Require(data.touch_data_touch0_y == 0xffff);
}

void TestDualTouchCoordinatePermutations(int handle) {
    PadData data{};
    PadInputState input{};

    input.touch[0] = {true, 0, 0, 0};
    input.touch[1] = {true, 1919, 1079, 0};
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 2);
    Require(data.touch_data_touch0_x == 0);
    Require(data.touch_data_touch0_y == 0);
    Require(data.touch_data_touch1_x == 1919);
    Require(data.touch_data_touch1_y == 1079);

    input.touch[0] = {true, 1919, 0, 0};
    input.touch[1] = {true, 0, 1079, 0};
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 2);
    Require(data.touch_data_touch0_x == 1919);
    Require(data.touch_data_touch0_y == 0);
    Require(data.touch_data_touch1_x == 0);
    Require(data.touch_data_touch1_y == 1079);

    input.touch[0] = {true, 500, 500, 0};
    input.touch[1] = {true, 501, 501, 0};
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 2);
    Require(data.touch_data_touch0_x == 500);
    Require(data.touch_data_touch0_y == 500);
    Require(data.touch_data_touch1_x == 501);
    Require(data.touch_data_touch1_y == 501);

    input.touch[0] = {false, 0, 0, 0};
    input.touch[1] = {true, 1200, 600, 0};
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 1);
    Require(data.touch_data_touch0_x == 1200);
    Require(data.touch_data_touch0_y == 600);
}

void TestContinuousTrajectoriesAndIdPersistence(int handle) {
    PadData data{};
    PadInputState input{};

    input.touch[0] = {true, 100, 200, 0};
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 1);
    const std::uint8_t initialId = data.touch_data_touch0_id;

    for (std::uint16_t x = 200; x <= 1800; x += 200) {
        input.touch[0] = {true, x, 200, 0};
        PadPublishInput_nid_postfix(input);
        Require(scePadReadState(handle, &data) == PAD_OK);
        Require(data.touch_data_touch_num == 1);
        Require(data.touch_data_touch0_x == x);
        Require(data.touch_data_touch0_y == 200);
        Require(data.touch_data_touch0_id == initialId);
    }

    for (std::uint16_t y = 300; y <= 1000; y += 100) {
        input.touch[0] = {true, 1800, y, 0};
        PadPublishInput_nid_postfix(input);
        Require(scePadReadState(handle, &data) == PAD_OK);
        Require(data.touch_data_touch_num == 1);
        Require(data.touch_data_touch0_x == 1800);
        Require(data.touch_data_touch0_y == y);
        Require(data.touch_data_touch0_id == initialId);
    }

    input.touch[0] = {false, 0, 0, 0};
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 0);

    input.touch[0] = {true, 400, 400, 0};
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 1);
    Require(data.touch_data_touch0_x == 400);
    Require(data.touch_data_touch0_y == 400);
    Require(data.touch_data_touch0_id == static_cast<std::uint8_t>((initialId + 1) & 0x7f));
}

void TestSynthesizedVirtualTouchCoordinates(int handle) {
    PadData data{};
    PadInputState input{};

    input.touchLeft = true;
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 1);
    Require((data.buttons & 0x00100000u) != 0);
    Require(data.touch_data_touch0_x == kTouchPadWidth / 4);
    Require(data.touch_data_touch0_y == kTouchPadHeight / 2);
    Require(data.touch_data_touch0_id == 0);

    input.touchLeft = false;
    input.touchRight = true;
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 1);
    Require((data.buttons & 0x00100000u) != 0);
    Require(data.touch_data_touch0_x == (kTouchPadWidth * 3) / 4);
    Require(data.touch_data_touch0_y == kTouchPadHeight / 2);
    Require(data.touch_data_touch0_id == 0);

    input.touch[0] = {true, 333, 444, 0};
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 1);
    Require(data.touch_data_touch0_x == 333);
    Require(data.touch_data_touch0_y == 444);

    input.touch[0] = {false, 0, 0, 0};
    input.touchRight = false;
    PadPublishInput_nid_postfix(input);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 0);
}

void TestMultiPortCoordinateIsolation(int handle0) {
    const int handle1 = scePadOpen_nid_postfix(0x10000001, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle1 > 0);

    PadInputState input0{};
    input0.touch[0] = {true, 250, 350, 0};
    PadPublishInputForPort_nid_postfix(0, input0);

    PadInputState input1{};
    input1.touch[0] = {true, 1600, 700, 0};
    PadPublishInputForPort_nid_postfix(1, input1);

    PadData data0{};
    PadData data1{};
    Require(scePadReadState(handle0, &data0) == PAD_OK);
    Require(scePadReadState(handle1, &data1) == PAD_OK);
    Require(data0.touch_data_touch_num == 1);
    Require(data0.touch_data_touch0_x == 250);
    Require(data0.touch_data_touch0_y == 350);

    Require(data1.touch_data_touch_num == 1);
    Require(data1.touch_data_touch0_x == 1600);
    Require(data1.touch_data_touch0_y == 700);

    input0.touch[0] = {false, 0, 0, 0};
    PadPublishInputForPort_nid_postfix(0, input0);
    Require(scePadReadState(handle0, &data0) == PAD_OK);
    Require(scePadReadState(handle1, &data1) == PAD_OK);
    Require(data0.touch_data_touch_num == 0);
    Require(data1.touch_data_touch_num == 1);
    Require(data1.touch_data_touch0_x == 1600);
    Require(data1.touch_data_touch0_y == 700);

    input1.touch[0] = {false, 0, 0, 0};
    PadPublishInputForPort_nid_postfix(1, input1);
    Require(scePadClose_nid_postfix(handle1) == PAD_OK);
}

void TestSpecialHandleCoordinateBehavior() {
    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);

    PadData data{};
    Require(scePadReadState(special, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 0);
    Require(data.touch_data_touch0_x == 0);
    Require(data.touch_data_touch0_y == 0);
    Require(data.touch_data_touch1_x == 0);
    Require(data.touch_data_touch1_y == 0);

    Require(scePadRead_nid_postfix(special, &data, 1) == 1);
    Require(data.touch_data_touch_num == 0);

    Require(scePadClose_nid_postfix(special) == PAD_OK);
}

void TestDeviceClassParseDataBounds(int handle) {
    PadData data{};
    PadDeviceClassData classData{};

    Require(scePadDeviceClassParseData(handle, nullptr, &classData) == PAD_ERROR_INVALID_ARG);
    Require(scePadDeviceClassParseData(handle, &data, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadDeviceClassParseData(-1, &data, &classData) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadDeviceClassParseData(0, &data, &classData) == PAD_ERROR_INVALID_HANDLE);

    data.connected = true;
    Require(scePadDeviceClassParseData(handle, &data, &classData) == PAD_OK);
    Require(classData.deviceClass == PAD_DEVICE_CLASS_STANDARD);
    Require(classData.dataValid == true);
}

void TestHandleClosureAndPostCloseBounds(int handle) {
    PadData data{};
    Require(scePadClose_nid_postfix(handle) == PAD_OK);
    Require(scePadClose_nid_postfix(handle) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadReadState(handle, &data) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadRead_nid_postfix(handle, &data, 1) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadClose_nid_postfix(-1) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadClose_nid_postfix(0) == PAD_ERROR_INVALID_HANDLE);
}

} // namespace

int main() {
    TestUninitializedAndInvalidHandleBounds();

    const int handle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle > 0);

    TestTouchResolutionAndInformationBounds(handle);
    TestTouchCoordinateBoundaryExtremes(handle);
    TestDualTouchCoordinatePermutations(handle);
    TestContinuousTrajectoriesAndIdPersistence(handle);
    TestSynthesizedVirtualTouchCoordinates(handle);
    TestMultiPortCoordinateIsolation(handle);
    TestSpecialHandleCoordinateBehavior();
    TestDeviceClassParseDataBounds(handle);
    TestHandleClosureAndPostCloseBounds(handle);

    return 0;
}
