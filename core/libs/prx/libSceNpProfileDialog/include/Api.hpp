#pragma once

#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>

struct SceCommonDialogBaseParam {
    std::uint64_t size;
    std::uint8_t reserved[36];
    std::uint32_t magic;
};

struct SceNpProfileDialogParam {
    SceCommonDialogBaseParam baseParam;
    std::uint64_t size;
    std::uint32_t mode;
    std::int32_t userId;
    NpOnlineId targetOnlineId;
    std::uint8_t padding[4];
    void* userData;
    std::uint8_t reserved[32];
};

struct SceNpProfileDialogParamA {
    SceCommonDialogBaseParam baseParam;
    std::uint64_t size;
    std::uint32_t mode;
    std::int32_t userId;
    std::uint64_t targetAccountId;
    std::uint8_t padding[8];
    void* userData;
    std::uint8_t reserved[32];
};

struct SceNpProfileDialogResult {
    std::int32_t result;
    std::int32_t userAction;
    void* userData;
    std::uint8_t reserved[32];
};

extern "C" {

int APS5_VABI sceNpProfileDialogInitialize(void);
int APS5_VABI sceNpProfileDialogTerminate(void);
int APS5_VABI sceNpProfileDialogUpdateStatus(void);
int APS5_VABI sceNpProfileDialogGetStatus(void);
int APS5_VABI sceNpProfileDialogOpen(const SceNpProfileDialogParam* param);
int APS5_VABI sceNpProfileDialogOpenA(const SceNpProfileDialogParamA* param);
int APS5_VABI sceNpProfileDialogClose(void);
int APS5_VABI sceNpProfileDialogGetResult(SceNpProfileDialogResult* result);

}
