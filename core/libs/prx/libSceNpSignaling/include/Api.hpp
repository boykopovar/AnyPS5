#pragma once

#include <cstdint>
#include "SceTypes.hpp"

struct NpSignalingAccountPlatformPair {
    std::uint64_t accountId;
    std::uint32_t platformType;
    std::uint32_t reserved;
};
static_assert(sizeof(NpSignalingAccountPlatformPair) == 0x10);

struct NpSignalingNetInfo {
    std::uint64_t size;
    std::uint32_t localAddr;
    std::uint32_t mappedAddr;
    int natStatus;
    std::uint32_t reserved;
};
static_assert(sizeof(NpSignalingNetInfo) == 0x18);

struct NpSignalingConnectionStatistics {
    std::uint32_t peakConnectionCount;
    std::uint32_t activeConnectionCount;
    std::uint32_t transientConnectionCount;
    std::uint32_t establishedConnectionCount;
};
static_assert(sizeof(NpSignalingConnectionStatistics) == 0x10);

extern "C" {

int APS5_VABI sceNpSignalingActivateConnection(int ctxId, const void* peerNpId, int* outConnId);
int APS5_VABI sceNpSignalingCreateContext(const void* npId, void* callback, void* callbackArg, int* outContextId);
int APS5_VABI sceNpSignalingDeleteContext(int ctxId);
int APS5_VABI sceNpSignalingGetConnectionInfo(int ctxId, int connId, int infoCode, void* outInfo);
int APS5_VABI sceNpSignalingInitialize(std::int64_t memorySize, int threadPriority, int cpuAffinityMask, std::int64_t threadStackSize);
int APS5_VABI sceNpSignalingSetContextOption(int ctxId, int optionId, int optionValue);
int APS5_VABI sceNpSignalingTerminate();
int APS5_VABI sceNpSignalingTerminateConnection(int ctxId, int connId);
int APS5_VABI sceNpSignalingDeactivateConnection(int ctxId, int connId);
int APS5_VABI sceNpSignalingCreateContextA(int userId, void* callback, void* callbackArg, int* outContextId);
int APS5_VABI sceNpSignalingActivateConnectionA(int ctxId, const NpSignalingAccountPlatformPair* peerAddr, int* outConnId);
int APS5_VABI sceNpSignalingGetConnectionStatus(int ctxId, int connId, std::uint32_t* outStatus, std::uint32_t* outPeerAddr, std::uint16_t* outPeerPort);
int APS5_VABI sceNpSignalingGetConnectionInfoA(int ctxId, int connId, int infoCode, void* outInfo);
int APS5_VABI sceNpSignalingGetConnectionFromNpId(int ctxId, const void* peerNpId, int* outConnId);
int APS5_VABI sceNpSignalingGetConnectionFromPeerAddress(int ctxId, std::uint32_t peerAddr, std::uint16_t peerPort, int* outConnId);
int APS5_VABI sceNpSignalingGetConnectionFromPeerAddressA(int ctxId, const NpSignalingAccountPlatformPair* peerAddr, int* outConnId);
int APS5_VABI sceNpSignalingGetContextOption(int ctxId, int optionId, int* outOptionValue);
int APS5_VABI sceNpSignalingGetPeerNetInfoA(int ctxId, const void* peerAccount, std::uint32_t* outReqId);
int APS5_VABI sceNpSignalingCancelPeerNetInfo(int ctxId, int reqId);
int APS5_VABI sceNpSignalingGetPeerNetInfoResult(int ctxId, int connId, NpSignalingNetInfo* peerNetInfo);
int APS5_VABI sceNpSignalingGetConnectionStatistics(NpSignalingConnectionStatistics* stats);

}
