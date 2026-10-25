#include "prx/libSceAutoMounterClient/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static_assert(sizeof(AutoMounterUsbDeviceInfo) == 944);
static_assert(offsetof(AutoMounterUsbDeviceInfo, data0) == 150);
static_assert(offsetof(AutoMounterUsbDeviceInfo, data0Size) == 408);
static_assert(offsetof(AutoMounterUsbDeviceInfo, data1) == 416);
static_assert(offsetof(AutoMounterUsbDeviceInfo, data1Size) == 672);
static_assert(offsetof(AutoMounterUsbDeviceInfo, data2) == 680);
static_assert(offsetof(AutoMounterUsbDeviceInfo, data2Size) == 936);

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
