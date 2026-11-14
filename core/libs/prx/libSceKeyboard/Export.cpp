#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceKeyboard/include/InventoryExports.hpp"
#include "prx/libSceKeyboard/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceKeyboard/include/KeyboardState.hpp"

extern "C" {

int APS5_VABI sceDbgKeyboardGetHandle() {
    NotImplemented_nid_no_patch(__func__);
}

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

int APS5_VABI sceKeyboardGetConnection(std::int32_t handle, bool* connected) {
    return Keyboard::GetConnection(handle, connected);
}

int APS5_VABI sceKeyboardGetKey2Char(std::int32_t handle, std::int32_t arrange, std::uint8_t led, std::uint8_t modifierKey, std::uint16_t keyCode, KeyboardCharData* charData) {
    return Keyboard::GetKey2Char(handle, arrange, led, modifierKey, keyCode, charData);
}

int APS5_VABI sceKeyboardConnectPort(std::int32_t first, std::int32_t second) {
    if (!Keyboard::IsInitialized()) return KEYBOARD_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKeyboardDebugGetDeviceId(std::int32_t handle, std::int32_t* deviceId) {
    return Keyboard::DebugGetDeviceId(handle, deviceId);
}

int APS5_VABI sceKeyboardDeviceOpen(std::uint64_t deviceId, std::int32_t index, std::uint32_t options) {
    if (!Keyboard::IsInitialized()) return KEYBOARD_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKeyboardDisconnectAll(void) {
    return Keyboard::DisconnectAll();
}

int APS5_VABI sceKeyboardDisconnectDevice(std::int32_t deviceId) {
    if (!Keyboard::IsInitialized()) return KEYBOARD_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKeyboardDisconnectPort(std::int32_t first, std::int32_t second) {
    if (!Keyboard::IsInitialized()) return KEYBOARD_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKeyboardGetDeviceInfo(std::int32_t deviceId, KeyboardDeviceInfo* info) {
    return Keyboard::GetDeviceInfo(deviceId, info);
}

int APS5_VABI sceKeyboardGetHandle(std::int32_t userId, std::int32_t type, std::int32_t index) {
    return Keyboard::GetHandle(userId, type, index);
}

int APS5_VABI sceKeyboardMbusInit(void) {
    if (!Keyboard::IsInitialized()) return KEYBOARD_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKeyboardSetProcessFocus(const KeyboardProcessFocus* focus) {
    return Keyboard::SetProcessFocus(focus);
}

int APS5_VABI sceKeyboardSetProcessPrivilege(std::uint8_t privilege) {
    return Keyboard::SetProcessPrivilege(privilege);
}

int APS5_VABI sceDbgKeyboardClose(int32_t handle) {
    return sceKeyboardClose(handle);
}

int APS5_VABI sceDbgKeyboardGetKey2Char() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDbgKeyboardInit(void) {
    return sceKeyboardInit();
}

int APS5_VABI sceDbgKeyboardOpen(int userId, int32_t type, int32_t index, const void* param) {
    return sceKeyboardOpen(userId, type, index, param);
}

int APS5_VABI sceDbgKeyboardRead() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDbgKeyboardReadState(std::int32_t handle, KeyboardData* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void KeyboardPublishInput_nid_postfix(const KeyboardInputEvent& event) {
    Keyboard::Publish(event);
}

}

extern "C" {

int APS5_VABI libSceKeyboardUnknown_l76jptHfizM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("l76jptHfizM", libSceKeyboardUnknown_l76jptHfizM_nid_no_patch);

}
