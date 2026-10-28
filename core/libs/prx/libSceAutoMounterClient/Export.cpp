#include "prx/libSceAutoMounterClient/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

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

extern "C" {

int APS5_VABI sceAutoMounterClientGetUsbDeviceInfo(const char* deviceName, AutoMounterUsbDeviceInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceAutoMounterClientGetUsbDeviceList(char (*deviceNames)[64], std::int32_t* count) {
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
