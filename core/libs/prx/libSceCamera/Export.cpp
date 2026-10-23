#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static constexpr int SCE_CAMERA_ERROR_PARAM = static_cast<int>(0x802E0000);

extern "C" {

int APS5_VABI sceCameraIsAttached(int32_t index) {
    if (index != 0) return SCE_CAMERA_ERROR_PARAM;
    return 0;
}

int APS5_VABI sceCameraOpen(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraClose(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraStart(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraStop(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetConfig(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetConfig(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetAttribute(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetDeviceInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetFrameData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraIsValidFrameData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
