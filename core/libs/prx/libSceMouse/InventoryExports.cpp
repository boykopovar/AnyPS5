#include "prx/libSceMouse/include/InventoryExports.hpp"
#include "prx/libSceMouse/include/MouseState.hpp"
#include "prx/libSceMouse/include/mouse_structs.h"
#include "prx/libc/include/General.hpp"
#include <cerrno>

namespace {

constexpr int VIRTUAL_DEVICE_ERROR_PARAM = static_cast<int>(0x803B0001u);

[[noreturn]] void HidDriver(const char* function) {
    NotImplemented_nid_no_patch(function);
}

}

extern "C" {

int APS5_VABI sceMouseDisconnectAll(void) {
    if (!Mouse::IsInitialized()) return MOUSE_ERROR_NOT_INITIALIZED;
    HidDriver(__func__);
}

int APS5_VABI sceMouseVirtualDeviceAddDevice(const void* param, void* out) {
    if (!Mouse::IsInitialized()) return MOUSE_ERROR_NOT_INITIALIZED;
    if (param == nullptr || out == nullptr) return MOUSE_ERROR_INVALID_ARG;
    std::uint8_t flag = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&flag, static_cast<const std::uint8_t*>(param) + 4, sizeof(flag));
    errno = savedErrno;
    if (!copied) return MOUSE_ERROR_INVALID_ARG;
    if (flag != 0) return VIRTUAL_DEVICE_ERROR_PARAM;
    HidDriver(__func__);
}

int APS5_VABI sceMouseVirtualDeviceDeleteDevice(std::int32_t id) {
    (void)id;
    if (!Mouse::IsInitialized()) return MOUSE_ERROR_NOT_INITIALIZED;
    HidDriver(__func__);
}

int APS5_VABI sceMouseVirtualDeviceGetCursor(std::int32_t id, void* cursor) {
    (void)id;
    if (!Mouse::IsInitialized()) return MOUSE_ERROR_NOT_INITIALIZED;
    if (cursor == nullptr) return MOUSE_ERROR_INVALID_ARG;
    HidDriver(__func__);
}

}
