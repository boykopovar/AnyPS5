#pragma once

#include "SceTypes.hpp"
#include <cstdint>

struct NpSignalingNetInfo {
    std::uint64_t size;
    std::uint32_t localAddr;
    std::uint32_t mappedAddr;
    std::int32_t natStatus;
    std::uint32_t padding;
};
static_assert(sizeof(NpSignalingNetInfo) == 0x18);

extern "C" {

int APS5_VABI sceNpSignalingActivateConnection(int ctxId, const void* peerNpId, int* outConnId);
int APS5_VABI sceNpSignalingCreateContext(const void* npId, void* callback, void* callbackArg, int* outContextId);
int APS5_VABI sceNpSignalingDeleteContext(int ctxId);
int APS5_VABI sceNpSignalingGetConnectionInfo(int ctxId, int connId, int infoCode, void* outInfo);
int APS5_VABI sceNpSignalingGetLocalNetInfo(int ctxId, NpSignalingNetInfo* info);
int APS5_VABI sceNpSignalingInitialize(std::int64_t memorySize, int threadPriority, int cpuAffinityMask, std::int64_t threadStackSize);
int APS5_VABI sceNpSignalingSetContextOption(int ctxId, int optionId, int optionValue);
int APS5_VABI sceNpSignalingTerminate();
int APS5_VABI sceNpSignalingTerminateConnection(int ctxId, int connId);
int APS5_VABI sceNpSignalingDeactivateConnection(int ctxId, int connId);

}
