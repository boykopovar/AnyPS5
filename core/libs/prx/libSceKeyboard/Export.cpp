#include "prx/libc/include/General.hpp"
#include "prx/libSceKeyboard/include/KeyboardState.hpp"

extern "C" {

int APS5_VABI sceKeyboardInit(void) {
    return Keyboard::Initialize();
}

int APS5_VABI sceKeyboardOpen(int user_id, int32_t type, int32_t index, const void* param) {
    (void)param;
    return Keyboard::Open(user_id, type, index);
}

int APS5_VABI sceKeyboardClose(int32_t handle) {
    return Keyboard::Close(handle);
}

int APS5_VABI sceKeyboardRead(int32_t handle, KeyboardData* data, int32_t num) {
    return Keyboard::Read(handle, data, num);
}

int APS5_VABI sceKeyboardReadState(int32_t handle, KeyboardData* data) {
    return Keyboard::ReadState(handle, data);
}

int APS5_VABI sceKeyboardGetKey2Char(int32_t handle, int32_t arrange, uint32_t led, uint32_t modifier_key, uint16_t key_code, KeyboardCharData* char_data) {
    return Keyboard::GetKey2Char(handle, arrange, led, modifier_key, key_code, char_data);
}

int APS5_VABI sceKeyboardConnectPort(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKeyboardDebugGetDeviceId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKeyboardDeviceOpen(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKeyboardDisconnectAll(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKeyboardDisconnectDevice(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKeyboardDisconnectPort(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKeyboardGetConnection(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKeyboardGetDeviceInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKeyboardGetHandle(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKeyboardMbusInit(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKeyboardPadEmulateClose(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKeyboardPadEmulateOpen(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKeyboardSetProcessFocus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKeyboardSetProcessPrivilege(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void KeyboardPublishInput_nid_postfix(const KeyboardInputEvent& event) {
    Keyboard::Publish(event);
}

}
