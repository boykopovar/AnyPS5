#include <cstdio>
#include <string>
#include "prx/libSceAutoMounterClient/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>

static_assert(sizeof(AutoMounterUsbDeviceInfo) == 944);
static_assert(offsetof(AutoMounterUsbDeviceInfo, totalSpace) == 128);
static_assert(offsetof(AutoMounterUsbDeviceInfo, availableSpace) == 136);
static_assert(offsetof(AutoMounterUsbDeviceInfo, vendorId) == 144);
static_assert(offsetof(AutoMounterUsbDeviceInfo, productId) == 146);
static_assert(offsetof(AutoMounterUsbDeviceInfo, bcdDevice) == 148);
static_assert(offsetof(AutoMounterUsbDeviceInfo, manufacturer) == 150);
static_assert(offsetof(AutoMounterUsbDeviceInfo, manufacturerLength) == 408);
static_assert(offsetof(AutoMounterUsbDeviceInfo, product) == 416);
static_assert(offsetof(AutoMounterUsbDeviceInfo, productLength) == 672);
static_assert(offsetof(AutoMounterUsbDeviceInfo, serialNumber) == 680);
static_assert(offsetof(AutoMounterUsbDeviceInfo, serialNumberLength) == 936);

constexpr int AUTOMOUNTER_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80F40002u);

extern "C" {

int APS5_VABI sceAutoMounterClientGetUsbDeviceInfo(const char* deviceName, AutoMounterUsbDeviceInfo* info) {
    const int savedErrno = errno;
    std::string name;
    if (deviceName == nullptr || CopyGuestString_nid_no_patch(deviceName, 128, name) != 0) {
        errno = savedErrno;
        return AUTOMOUNTER_ERROR_INVALID_ARGUMENT;
    }
    char buf[128];
    if (sscanf(name.c_str(), "/dev%127s", buf) == 0) {
        errno = savedErrno;
        return AUTOMOUNTER_ERROR_INVALID_ARGUMENT;
    }
    if (info == nullptr || !GuestWritable_nid_no_patch(info, sizeof(*info))) {
        errno = savedErrno;
        return AUTOMOUNTER_ERROR_INVALID_ARGUMENT;
    }
    errno = savedErrno;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceAutoMounterClientGetUsbDeviceList(char (*deviceNames)[64], std::int32_t* count) {
    const int savedErrno = errno;
    if (deviceNames == nullptr || count == nullptr ||
        !GuestWritable_nid_no_patch(count, sizeof(*count)) ||
        !GuestWritable_nid_no_patch(deviceNames, sizeof(*deviceNames))) {
        errno = savedErrno;
        return AUTOMOUNTER_ERROR_INVALID_ARGUMENT;
    }
    errno = savedErrno;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceAutoMounterClientInit(PthreadAttr* threadAttr) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAutoMounterClientRegisterCallback() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAutoMounterClientTerm() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAutoMounterClientUnregisterCallback() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
