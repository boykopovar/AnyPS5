#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

static void Require(bool value) { if (!value) std::abort(); }


int main() {
    std::int32_t value = 1;
    const auto* parameter = reinterpret_cast<const PadUpdateParameter*>(&value);
    Require(scePadBondUpdateInit() == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadBondUpdateTerm() == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadBondUpdateIsReady(0, &value) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadBondUpdateGetStatus(0, &value) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadBondUpdateGetReconnectStatus(&value) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadBondUpdateStart(-1, parameter) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadBondUpdateSendData(0, nullptr) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadBondUpdateResetBond(0) == PAD_ERROR_NOT_INITIALIZED);

    Require(scePadInit_nid_postfix() == PAD_OK);
    Require(scePadBondUpdateIsReady(0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadBondUpdateGetStatus(0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadBondUpdateGetReconnectStatus(nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadBondUpdateStart(-1, nullptr) == PAD_ERROR_INVALID_ARG);
    PadUpdateData wrong{};
    Require(scePadBondUpdateSendData(0, &wrong) == PAD_ERROR_INVALID_ARG);

    constexpr int kTestPort = 1;
    constexpr std::uint32_t kButtonR2 = 0x200u;
    constexpr std::uint32_t kButtonCross = 0x4000u;
    constexpr std::uint32_t kButtonIntercepted = 0x80000000u;
    const bool wasConnected = Pad::IsConnected(kTestPort);
    Pad::SetConnected(kTestPort, true);

    PadInputState interceptedInput{};
    interceptedInput.buttons = kButtonCross | kButtonR2;
    interceptedInput.touchLeft = true;
    interceptedInput.intercepted = true;
    PadPublishInputForPort_nid_postfix(kTestPort, interceptedInput);

    const PadData stateWhileIntercepted = Pad::ReadState(kTestPort);
    Require(stateWhileIntercepted.buttons == kButtonIntercepted);
    Require(stateWhileIntercepted.analog_buttons_r2 == 0);
    Require(stateWhileIntercepted.touch_data_touch_num == 0);

    PadData readWhileIntercepted{};
    std::memset(&readWhileIntercepted, 0xA5, sizeof(readWhileIntercepted));
    Require(Pad::Read(&readWhileIntercepted, 1, kTestPort) == 1);
    Require(readWhileIntercepted.buttons == kButtonIntercepted);
    Require(readWhileIntercepted.analog_buttons_r2 == 0);
    Require(readWhileIntercepted.touch_data_touch_num == 0);

    PadInputState interceptedReleased{};
    interceptedReleased.intercepted = true;
    PadPublishInputForPort_nid_postfix(kTestPort, interceptedReleased);

    PadInputState interceptedAnalogPress{};
    interceptedAnalogPress.buttons = kButtonCross | kButtonR2;
    interceptedAnalogPress.analogButtonsR2 = 200;
    interceptedAnalogPress.intercepted = true;
    PadPublishInputForPort_nid_postfix(kTestPort, interceptedAnalogPress);

    PadInputState released{};
    PadPublishInputForPort_nid_postfix(kTestPort, released);

    const PadData afterInterception = Pad::ReadState(kTestPort);
    Require(afterInterception.buttons == 0);
    Require(afterInterception.analog_buttons_r2 == 0);

    PadInputState transientPress{};
    transientPress.buttons = kButtonCross | kButtonR2;
    transientPress.analogButtonsR2 = 200;
    PadPublishInputForPort_nid_postfix(kTestPort, transientPress);
    PadPublishInputForPort_nid_postfix(kTestPort, released);

    const PadData latched = Pad::ReadState(kTestPort);
    Require(latched.buttons == (kButtonCross | kButtonR2));
    Require(latched.analog_buttons_r2 == 200);

    PadData drain[8]{};
    Pad::Read(drain, 8, kTestPort);
    Pad::SetConnected(kTestPort, wasConnected);
    return 0;
}
