#include "prx/libSceMouse/include/Api.hpp"
#include "prx/libSceMouse/include/MouseState.hpp"
#include "prx/libc/include/General.hpp"

#include <cstdint>
#include <cstring>

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

int APS5_VABI sceMouseGetDeviceInfo(std::int32_t device, void* info) {
    static_cast<void>(device);
    if (!Mouse::IsInitialized()) return MOUSE_ERROR_NOT_INITIALIZED;
    if (info == nullptr) return MOUSE_ERROR_INVALID_ARG;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMouseMbusInit() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMouseSetHandType(std::int32_t handle, std::uint32_t type) {
    if (!Mouse::IsInitialized()) return MOUSE_ERROR_NOT_INITIALIZED;
    if (type > MOUSE_MAX_HAND_TYPE) return MOUSE_ERROR_INVALID_ARG;
    static_cast<void>(handle);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMouseSetPointerSpeed(std::int32_t handle, std::uint32_t speed) {
    if (!Mouse::IsInitialized()) return MOUSE_ERROR_NOT_INITIALIZED;
    if (speed > MOUSE_MAX_POINTER_SPEED) return MOUSE_ERROR_INVALID_ARG;
    static_cast<void>(handle);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMouseSetProcessPrivilege() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMouseReadAbsolutePosition(std::int32_t handle, MouseAbsoluteData* data, std::int32_t num) {
    return Mouse::ReadAbsolutePosition(handle, data, num);
}

int APS5_VABI sceMouseSetAbsoluteHighPrecisionMode(std::int32_t handle, std::int32_t mode) {
    return Mouse::SetAbsoluteHighPrecisionMode(handle, mode);
}

int APS5_VABI sceMouseSetAbsoluteVirtualScreenSize(std::int32_t handle, std::int32_t size) {
    return Mouse::SetAbsoluteVirtualScreenSize(handle, size);
}

int APS5_VABI sceMouseGetHandle(int userId, std::int32_t type, std::int32_t index) {
    static_cast<void>(index);
    return Mouse::GetHandle(userId, type);
}

int APS5_VABI sceMouseExtensionRead(std::int32_t handle, void* data, std::uint32_t size, std::int32_t num) {
    if (size != MOUSE_EXTENSION_DATA_SIZE) return MOUSE_ERROR_INVALID_ARG;
    if (const int error = Mouse::CheckRead(handle, data, num)) return error;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMouseExtensionReadState(std::int32_t handle, void* state, std::uint32_t size) {
    if (size != MOUSE_EXTENSION_STATE_SIZE || state == nullptr) return MOUSE_ERROR_INVALID_ARG;
    std::memset(state, 0, MOUSE_EXTENSION_STATE_SIZE);
    if (const int error = Mouse::CheckRead(handle, state, MOUSE_MAX_DATA_NUM)) return error;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMouseExtensionSetConfig(std::int32_t handle, const void* config) {
    return Mouse::SetExtensionConfig(handle, static_cast<const std::uint8_t*>(config));
}

int APS5_VABI sceMouseExtensionSetCursor(std::int32_t handle, std::int32_t cursor) {
    if (!Mouse::IsInitialized()) return MOUSE_ERROR_NOT_INITIALIZED;
    if (const int error = Mouse::CheckHandle(handle)) return error;
    static_cast<void>(cursor);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMouseExtensionGetCursor(std::int32_t handle, void* cursor) {
    if (!Mouse::IsInitialized()) return MOUSE_ERROR_NOT_INITIALIZED;
    if (cursor == nullptr) return MOUSE_ERROR_INVALID_ARG;
    if (const int error = Mouse::CheckHandle(handle)) return error;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMouseExtensionScalePosition(std::uint16_t* position, std::uint16_t scale) {
    if (!Mouse::IsInitialized()) return MOUSE_ERROR_NOT_INITIALIZED;
    if (position == nullptr || scale == 0) return MOUSE_ERROR_INVALID_ARG;
    const std::uint32_t scaled = static_cast<std::uint32_t>(*position) * (scale - 1u);
    *position = static_cast<std::uint16_t>((scaled + 0xfffeu) / 0xffffu);
    return MOUSE_OK;
}

}
