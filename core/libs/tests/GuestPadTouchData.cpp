#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <cstdlib>
#include <cstring>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    Require(scePadInit_nid_postfix() == PAD_OK);
    const int handle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle > 0);

    PadInputState touch;
    touch.buttons = static_cast<std::uint32_t>(Pad::PadButton::TouchPad);

    touch.touch[0] = {true, 0, 0, 0};
    PadPublishInput_nid_postfix(touch);
    PadData stateData{};
    Require(scePadReadState(handle, &stateData) == PAD_OK);
    Require(stateData.touch_data_touch_num == 1);
    Require(stateData.touch_data_touch0_x == 0);
    Require(stateData.touch_data_touch0_y == 0);

    touch.touch[0] = {true, kTouchPadWidth, kTouchPadHeight, 1};
    PadPublishInput_nid_postfix(touch);
    Require(scePadReadState(handle, &stateData) == PAD_OK);
    Require(stateData.touch_data_touch_num == 1);
    Require(stateData.touch_data_touch0_x == kTouchPadWidth);
    Require(stateData.touch_data_touch0_y == kTouchPadHeight);

    touch.touch[0] = {true, kTouchPadWidth + 100, kTouchPadHeight + 100, 2};
    PadPublishInput_nid_postfix(touch);
    Require(scePadReadState(handle, &stateData) == PAD_OK);
    Require(stateData.touch_data_touch_num == 1);
    Require(stateData.touch_data_touch0_x == kTouchPadWidth + 100);
    Require(stateData.touch_data_touch0_y == kTouchPadHeight + 100);

    touch.touch[0] = {false, 0, 0, 0};
    touch.touch[1] = {true, 500, 500, 3};
    PadPublishInput_nid_postfix(touch);
    Require(scePadReadState(handle, &stateData) == PAD_OK);
    Require(stateData.touch_data_touch_num == 1);
    Require(stateData.touch_data_touch0_x == 500);
    Require(stateData.touch_data_touch0_y == 500);

    touch.touch[0] = {true, 100, 100, 0};
    touch.touch[1] = {true, 200, 200, 1};
    PadPublishInput_nid_postfix(touch);
    Require(scePadReadState(handle, &stateData) == PAD_OK);
    Require(stateData.touch_data_touch_num == 2);
    Require(stateData.touch_data_touch0_x == 100);
    Require(stateData.touch_data_touch0_y == 100);
    Require(stateData.touch_data_touch1_x == 200);
    Require(stateData.touch_data_touch1_y == 200);

    PadPublishInput_nid_postfix(PadInputState{});
    Require(scePadClose_nid_postfix(handle) == PAD_OK);
}
