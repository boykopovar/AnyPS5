#pragma once

#include "SceTypes.hpp"

struct AutoMounterUsbDeviceInfo {
    char deviceName[128];
    std::uint64_t totalSpace;
    std::uint64_t availableSpace;
    std::uint16_t vendorId;
    std::uint16_t productId;
    std::uint16_t bcdDevice;
    char manufacturer[256];
    std::uint8_t padding[2];
    std::uint64_t manufacturerLength;
    char product[256];
    std::uint64_t productLength;
    char serialNumber[256];
    std::uint64_t serialNumberLength;
};

extern "C" {

int APS5_VABI sceAutoMounterClientGetUsbDeviceInfo(const char* deviceName, AutoMounterUsbDeviceInfo* info);
int APS5_VABI sceAutoMounterClientGetUsbDeviceList(char (*deviceNames)[64], std::int32_t* count);
int APS5_VABI sceAutoMounterClientInit(PthreadAttr* threadAttr);
int APS5_VABI sceAutoMounterClientRegisterCallback();
int APS5_VABI sceAutoMounterClientTerm();
int APS5_VABI sceAutoMounterClientUnregisterCallback();

}
