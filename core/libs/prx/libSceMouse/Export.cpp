#include "prx/libSceMouse/include/MouseState.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceMouseInit() {
    return Mouse::Initialize();
}

int APS5_VABI sceMouseOpen(int userId, std::int32_t type, std::int32_t index, const void* param) {
    return Mouse::Open(userId, type, index, static_cast<const MouseOpenParam*>(param));
}

int APS5_VABI sceMouseClose(std::int32_t handle) {
    return Mouse::Close(handle);
}

int APS5_VABI sceMouseRead(std::int32_t handle, MouseData* data, std::int32_t num) {
    return Mouse::Read(handle, data, num);
}

void MousePublishInput_nid_no_patch(const MouseInputEvent& event) {
    Mouse::Publish(event);
}

int APS5_VABI sceMouseResetAbsolutePosition(std::int32_t handle, std::uint16_t x, std::uint16_t y) {
    return Mouse::ResetAbsolutePosition(handle, x, y);
}

bool MouseIsOpen_nid_no_patch() {
    return Mouse::IsOpen();
}

int APS5_VABI sceMouseConnectPort() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMouseDebugGetDeviceId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMouseDeviceOpen() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMouseDisconnectDevice() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMouseDisconnectPort() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMouseGetDeviceInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMouseMbusInit() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMouseSetHandType() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMouseSetPointerSpeed() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMouseSetProcessPrivilege() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
