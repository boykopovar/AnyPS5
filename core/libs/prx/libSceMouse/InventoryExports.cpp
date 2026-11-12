#include "prx/libSceMouse/include/InventoryExports.hpp"
#include "prx/libSceMouse/include/MouseState.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceMouseDisconnectAll() {
    if (!Mouse::IsInitialized()) return MOUSE_ERROR_NOT_INITIALIZED;
    return Mouse::DisconnectAll();
}

int APS5_VABI sceMouseVirtualDeviceAddDevice() {
    if (!Mouse::IsInitialized()) return MOUSE_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMouseVirtualDeviceDeleteDevice() {
    if (!Mouse::IsInitialized()) return MOUSE_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMouseVirtualDeviceGetCursor() {
    if (!Mouse::IsInitialized()) return MOUSE_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
