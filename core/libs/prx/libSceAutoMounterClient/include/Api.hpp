#pragma once

#include "SceTypes.hpp"

struct AutoMounterUsbDeviceInfo {
    char deviceName[128];
    std::byte metadata[22];
    std::byte data0[256];
    std::byte padding[2];
    std::uint64_t data0Size;
    std::byte data1[256];
    std::uint64_t data1Size;
    std::byte data2[256];
    std::uint64_t data2Size;
};

extern "C" {

int APS5_VABI sceAutoMounterClientGetUsbDeviceInfo(const char* deviceName, AutoMounterUsbDeviceInfo* info);
int APS5_VABI sceAutoMounterClientGetUsbDeviceList(char (*deviceNames)[64], std::int32_t* count);
int APS5_VABI sceAutoMounterClientInit(PthreadAttr* threadAttr);
int APS5_VABI sceAutoMounterClientRegisterCallback();
int APS5_VABI sceAutoMounterClientTerm();
int APS5_VABI sceAutoMounterClientUnregisterCallback();

}
