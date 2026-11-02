#include "prx/libSceMouse/include/InventoryExports.hpp"
#include "prx/libSceMouse/include/MouseState.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceMouseDisconnectAll() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMouseExtensionGetCursor() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMouseExtensionRead() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMouseExtensionReadState() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMouseExtensionScalePosition(std::uint16_t* position, std::uint16_t scale) {
    if (!Mouse::IsInitialized()) return MOUSE_ERROR_NOT_INITIALIZED;
    if (position == nullptr || scale == 0) return MOUSE_ERROR_INVALID_ARG;
    const auto scaled = static_cast<std::uint64_t>(*position) * (scale - 1);
    *position = static_cast<std::uint16_t>((scaled + 32767) / 65535);
    return MOUSE_OK;
}

int APS5_VABI sceMouseExtensionSetConfig() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMouseExtensionSetCursor() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMouseGetHandle() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMouseVirtualDeviceAddDevice() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMouseVirtualDeviceDeleteDevice() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMouseVirtualDeviceGetCursor() {
    NotImplemented_nid_no_patch(__func__);
}

}
