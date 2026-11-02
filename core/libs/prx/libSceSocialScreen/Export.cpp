#include "prx/libSceSocialScreen/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceSocialScreenCloseSeparateMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSocialScreenConfigureSeparateMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSocialScreenCrashDaemon() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSocialScreenGetIgnoreSeparateMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSocialScreenInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSocialScreenInitializeSeparateModeParameter(void* parameter) {
    if (parameter == nullptr) return static_cast<int>(0x81440003);
    alignas(4) std::uint8_t buffer[0x20] = {};
    const std::uint32_t at0 = 1;
    std::memcpy(buffer, &at0, sizeof(at0));
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(parameter, buffer, sizeof(buffer));
    errno = savedErrno;
    if (!copied) return static_cast<int>(0x81440003);
    return 0;
}

int APS5_VABI sceSocialScreenOpenSeparateMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSocialScreenReserveHighFrameRateVideoCaptureRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSocialScreenSetIgnoreSeparateMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSocialScreenSetMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSocialScreenTerminate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
