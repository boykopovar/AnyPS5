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

void TestUninitializedAndHandleBounds() {
    PadData data{};
    Require(scePadRead_nid_postfix(1, &data, 1) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadReadState(1, &data) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadReadState(1, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadRead_nid_postfix(1, nullptr, 1) == PAD_ERROR_NOT_INITIALIZED);

    Require(scePadInit_nid_postfix() == PAD_OK);

    for (int handle : {-9999, -1, 0, 1, 0x10, 0x20, 9999}) {
        Require(scePadRead_nid_postfix(handle, &data, 1) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadRead_nid_postfix(handle, nullptr, 1) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadRead_nid_postfix(handle, &data, 0) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadRead_nid_postfix(handle, &data, -1) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadReadState(handle, &data) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadReadState(handle, nullptr) == PAD_ERROR_INVALID_ARG);
    }
}

void TestTouchpadInformationBounds(int handle) {
    PadControllerInformation info{};
    Require(scePadGetControllerInformation(handle, &info) == PAD_OK);
    Require(info.touchPadInfo.pixelDensity == 44.86f);
    Require(info.touchPadInfo.resolution.x == kTouchPadWidth);
    Require(info.touchPadInfo.resolution.y == kTouchPadHeight);
    Require(kTouchPadWidth == 1920);
    Require(kTouchPadHeight == 1080);
}

void TestDefaultTouchStateAndInvalidArgs(int handle) {
    PadData data{};
    Require(scePadRead_nid_postfix(handle, nullptr, 1) == PAD_ERROR_INVALID_ARG);
    Require(scePadRead_nid_postfix(handle, &data, 0) == PAD_ERROR_INVALID_ARG);
    Require(scePadRead_nid_postfix(handle, &data, -1) == PAD_ERROR_INVALID_ARG);
    Require(scePadReadState(handle, nullptr) == PAD_ERROR_INVALID_ARG);

    std::memset(&data, 0xaa, sizeof(data));
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 0);
    Require(scePadRead_nid_postfix(handle, &data, 1) == 1);
    Require(data.touch_data_touch_num == 0);
}

void TestSingleTouchContactBounds(int handle) {
    PadData data{};
    PadInputState touch{};

    touch.touch[0] = {true, 0, 0, 0};
    PadPublishInput_nid_postfix(touch);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 1);
    Require(data.touch_data_touch0_x == 0);
    Require(data.touch_data_touch0_y == 0);
    Require(data.touch_data_touch0_id == 0);

    touch.touch[0] = {true, kTouchPadWidth, kTouchPadHeight, 0};
    PadPublishInput_nid_postfix(touch);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 1);
    Require(data.touch_data_touch0_x == kTouchPadWidth);
    Require(data.touch_data_touch0_y == kTouchPadHeight);

    touch.touch[0] = {true, static_cast<std::uint16_t>(kTouchPadWidth + 200), static_cast<std::uint16_t>(kTouchPadHeight + 100), 0};
    PadPublishInput_nid_postfix(touch);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 1);
    Require(data.touch_data_touch0_x == kTouchPadWidth + 200);
    Require(data.touch_data_touch0_y == kTouchPadHeight + 100);

    touch.touch[0] = {true, 0xffff, 0xffff, 0};
    PadPublishInput_nid_postfix(touch);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 1);
    Require(data.touch_data_touch0_x == 0xffff);
    Require(data.touch_data_touch0_y == 0xffff);
}

void TestSecondTouchSlotOnly(int handle) {
    PadData data{};
    PadInputState touch{};
    touch.touch[0] = {false, 0, 0, 0};
    touch.touch[1] = {true, 960, 540, 0};
    PadPublishInput_nid_postfix(touch);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 1);
    Require(data.touch_data_touch0_x == 960);
    Require(data.touch_data_touch0_y == 540);
}

void TestDualTouchContactBounds(int handle) {
    PadData data{};
    PadInputState touch{};
    touch.touch[0] = {true, 120, 240, 0};
    touch.touch[1] = {true, 1750, 890, 0};
    PadPublishInput_nid_postfix(touch);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 2);
    Require(data.touch_data_touch0_x == 120);
    Require(data.touch_data_touch0_y == 240);
    Require(data.touch_data_touch1_x == 1750);
    Require(data.touch_data_touch1_y == 890);
}

void TestTouchIdLifecycleAndWrap(int handle) {
    PadData data{};
    PadInputState released{};
    PadPublishInput_nid_postfix(released);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 0);

    PadInputState press1{};
    press1.touch[0] = {true, 300, 300, 0};
    PadPublishInput_nid_postfix(press1);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 1);
    const std::uint8_t id1 = data.touch_data_touch0_id;

    press1.touch[0] = {true, 350, 350, 0};
    PadPublishInput_nid_postfix(press1);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 1);
    Require(data.touch_data_touch0_id == id1);
    Require(data.touch_data_touch0_x == 350);
    Require(data.touch_data_touch0_y == 350);

    PadPublishInput_nid_postfix(released);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 0);

    PadInputState press2{};
    press2.touch[0] = {true, 400, 400, 0};
    PadPublishInput_nid_postfix(press2);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 1);
    const std::uint8_t id2 = data.touch_data_touch0_id;
    Require(id2 == static_cast<std::uint8_t>((id1 + 1) & 0x7f));
}

void TestSynthesizedVirtualTouchButtons(int handle) {
    PadData data{};
    PadInputState st{};
    st.touchLeft = true;
    PadPublishInput_nid_postfix(st);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 1);
    Require((data.buttons & 0x00100000u) != 0);
    Require(data.touch_data_touch0_x == kTouchPadWidth / 4);
    Require(data.touch_data_touch0_y == kTouchPadHeight / 2);
    Require(data.touch_data_touch0_id == 0);

    st.touchLeft = false;
    st.touchRight = true;
    PadPublishInput_nid_postfix(st);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 1);
    Require((data.buttons & 0x00100000u) != 0);
    Require(data.touch_data_touch0_x == (kTouchPadWidth * 3) / 4);
    Require(data.touch_data_touch0_y == kTouchPadHeight / 2);
    Require(data.touch_data_touch0_id == 0);

    st.touch[0] = {true, 100, 100, 0};
    PadPublishInput_nid_postfix(st);
    Require(scePadReadState(handle, &data) == PAD_OK);
    Require(data.touch_data_touch_num == 1);
    Require(data.touch_data_touch0_x == 100);
    Require(data.touch_data_touch0_y == 100);
}

void TestPortTouchIsolationAndSpecialHandle(int handle) {
    const int handle1 = scePadOpen_nid_postfix(0x10000001, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle1 > 0);

    PadInputState port0Input{};
    port0Input.touch[0] = {true, 50, 60, 0};
    PadPublishInputForPort_nid_postfix(0, port0Input);

    PadInputState port1Input{};
    port1Input.touch[0] = {true, 750, 850, 0};
    PadPublishInputForPort_nid_postfix(1, port1Input);

    PadData data0{};
    PadData data1{};
    Require(scePadReadState(handle, &data0) == PAD_OK);
    Require(scePadReadState(handle1, &data1) == PAD_OK);
    Require(data0.touch_data_touch_num == 1);
    Require(data0.touch_data_touch0_x == 50);
    Require(data0.touch_data_touch0_y == 60);

    Require(data1.touch_data_touch_num == 1);
    Require(data1.touch_data_touch0_x == 750);
    Require(data1.touch_data_touch0_y == 850);

    Require(scePadClose_nid_postfix(handle1) == PAD_OK);

    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);
    PadData specialData{};
    Require(scePadReadState(special, &specialData) == PAD_OK);
    Require(specialData.touch_data_touch_num == 0);
    Require(scePadRead_nid_postfix(special, &specialData, 1) == 1);
    Require(specialData.touch_data_touch_num == 0);
    Require(scePadClose_nid_postfix(special) == PAD_OK);
}

void TestQueueReadingAndHandleClosure(int handle) {
    PadData data{};
    PadInputState q1{};
    q1.touch[0] = {true, 10, 20, 0};
    PadPublishInput_nid_postfix(q1);

    PadInputState q2{};
    q2.touch[0] = {true, 30, 40, 0};
    PadPublishInput_nid_postfix(q2);

    std::array<PadData, 4> readBuffer{};
    const int readCount = scePadRead_nid_postfix(handle, readBuffer.data(), static_cast<int>(readBuffer.size()));
    Require(readCount >= 1);

    Require(scePadClose_nid_postfix(handle) == PAD_OK);
    Require(scePadClose_nid_postfix(handle) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadRead_nid_postfix(handle, &data, 1) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadReadState(handle, &data) == PAD_ERROR_INVALID_HANDLE);

    PadPublishInput_nid_postfix(PadInputState{});
    PadPublishInputForPort_nid_postfix(1, PadInputState{});
}

} // namespace

int main() {
    TestUninitializedAndHandleBounds();

    const int handle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle > 0);

    TestTouchpadInformationBounds(handle);
    TestDefaultTouchStateAndInvalidArgs(handle);
    TestSingleTouchContactBounds(handle);
    TestSecondTouchSlotOnly(handle);
    TestDualTouchContactBounds(handle);
    TestTouchIdLifecycleAndWrap(handle);
    TestSynthesizedVirtualTouchButtons(handle);
    TestPortTouchIsolationAndSpecialHandle(handle);
    TestQueueReadingAndHandleClosure(handle);

    return 0;
}
