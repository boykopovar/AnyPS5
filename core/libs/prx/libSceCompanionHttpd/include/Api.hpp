#pragma once

#include "SceTypes.hpp"

struct CompanionHttpdOptParam {
    std::uint64_t reserved;
    void* workMemory;
    std::uint64_t workMemorySize;
    std::uint8_t settings[0x40];
    const void* temporaryMountPoint;
};

using CompanionHttpdRequestCallback = int (*)(int, int, const void*, void*, void*);

extern "C" {

int APS5_VABI sceCompanionHttpdOptParamInitialize(CompanionHttpdOptParam* param);
int APS5_VABI sceCompanionHttpdInitialize(const CompanionHttpdOptParam* param);
int APS5_VABI sceCompanionHttpdTerminate(void);
int APS5_VABI sceCompanionHttpdRegisterRequestCallback(CompanionHttpdRequestCallback function, void* argument);
int APS5_VABI sceCompanionHttpdUnregisterRequestCallback(void);
int APS5_VABI sceCompanionHttpdStart(void);
int APS5_VABI sceCompanionHttpdStop(void);
int APS5_VABI sceCompanionHttpdGetEvent(void* event);
int APS5_VABI sceCompanionHttpdAddHeader(const char* key, const char* value, void* response);
int APS5_VABI sceCompanionHttpdSetStatus(int status, void* response);
int APS5_VABI sceCompanionHttpdSetBody(const char* body, std::uint64_t size, void* response);
int APS5_VABI sceCompanionHttpdGet2ndScreenStatus(void);
int APS5_VABI sceCompanionHttpdGetUserId(void);
int APS5_VABI sceCompanionHttpdInitialize2(void);
int APS5_VABI sceCompanionHttpdRegisterRequestBodyReceptionCallback(void);
int APS5_VABI sceCompanionHttpdRegisterRequestCallback2(void);
int APS5_VABI sceCompanionHttpdUnregisterRequestBodyReceptionCallback(void);

}
