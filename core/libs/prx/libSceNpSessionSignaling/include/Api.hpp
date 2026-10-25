#pragma once

#include "SceTypes.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>

struct SessionSignalingInitParam {
    std::int32_t userId;
    std::uint64_t poolSize;
    std::uint64_t affinityMask;
    std::int32_t threadPriority;
    std::uint64_t stackSize;
};

struct SessionSignalingMemoryInfo {
    std::uint64_t poolSize;
    std::uint64_t usedSize;
    std::uint64_t peakUsedSize;
};

struct SessionSignalingPeerAddress {
    std::uint64_t accountId;
    std::uint32_t platform;
};

struct SessionSignalingSessionParam {
    std::uint32_t mode;
    std::uint32_t option;
};

struct SessionSignalingNetAddress {
    std::uint32_t address;
    std::uint16_t port;
};

union SessionSignalingConnectionInfo {
    std::uint32_t value;
    SessionSignalingNetAddress netAddress;
    SessionSignalingPeerAddress peerAddress;
};

struct SessionSignalingLocalNetInfo {
    std::uint32_t localAddress;
    std::uint32_t mappedAddress;
    std::uint32_t mappedPort;
    std::uint32_t mappingStatus;
};

using SessionSignalingEventCallback = void (APS5_VABI *)(std::uint32_t, std::uint32_t, std::int32_t, const SessionSignalingPeerAddress*, std::int32_t, void*);

using SessionSignalingConnectionCallback = void (APS5_VABI *)(std::uint32_t, std::uint32_t, std::int32_t, std::int32_t, void*);

using SessionSignalingConnectionCallback2 = void (APS5_VABI *)(std::uint32_t, std::uint32_t, std::uint32_t, std::int32_t, std::int32_t, void*);

template<typename TConnectionCallback>
struct SessionSignalingContextParam {
    std::int32_t userId;
    std::uint32_t serviceLabel;
    SessionSignalingEventCallback prepareCallback;
    void* prepareUserData;
    SessionSignalingEventCallback sessionCallback;
    void* sessionUserData;
    TConnectionCallback connectionCallback;
    void* connectionUserData;
};

using SessionSignalingContextParam1 = SessionSignalingContextParam<SessionSignalingConnectionCallback>;

using SessionSignalingContextParam2 = SessionSignalingContextParam<SessionSignalingConnectionCallback2>;

struct SessionSignalingConnectionStatistics {
    std::uint32_t peakConnectionCount;
    std::uint32_t connectionCount;
    std::uint32_t connectingCount;
    std::uint32_t connectedCount;
};

struct SessionSignalingGroupInfo {
    std::uint32_t connectionIds[64];
    std::uint64_t connectionCount;
};

extern "C" {

int APS5_VABI sceNpSessionSignalingInitialize(const SessionSignalingInitParam* param);
int APS5_VABI sceNpSessionSignalingActivateSession(std::uint32_t contextId, const char* sessionId, std::uint32_t sessionType, const SessionSignalingSessionParam* param, std::uint32_t* groupId);
int APS5_VABI sceNpSessionSignalingCreateContext2(const SessionSignalingContextParam2* param, std::uint32_t* contextId);
int APS5_VABI sceNpSessionSignalingCreateContext(const SessionSignalingContextParam1* param, std::uint32_t* contextId);
int APS5_VABI sceNpSessionSignalingDeactivate(std::uint32_t contextId, std::uint32_t connectionId);
int APS5_VABI sceNpSessionSignalingDestroyContext(std::uint32_t contextId);
int APS5_VABI sceNpSessionSignalingGetConnectionInfo(std::uint32_t contextId, std::uint32_t connectionId, std::uint32_t code, SessionSignalingConnectionInfo* info);
int APS5_VABI sceNpSessionSignalingActivateUser(std::uint32_t contextId, const SessionSignalingPeerAddress* peerAddress, std::uint32_t* connectionId);
int APS5_VABI sceNpSessionSignalingGetConnectionFromPeerAddress2(std::uint32_t contextId, std::uint32_t groupId, const SessionSignalingPeerAddress* peerAddress, std::uint32_t* connectionId);
int APS5_VABI sceNpSessionSignalingTerminate(void);
int32_t APS5_VABI sceNpSessionSignalingGetConnectionStatus(std::uint32_t contextId, std::uint32_t connectionId, std::int32_t* status, std::uint32_t* peerAddress, std::uint16_t* peerPort);
int32_t APS5_VABI sceNpSessionSignalingGetLocalNetInfo(std::uint32_t contextId, SessionSignalingLocalNetInfo* info);
int APS5_VABI sceNpSessionSignalingRequestPrepare(uint32_t contextId, uint32_t* requestId);
int APS5_VABI sceNpSessionSignalingGetMemoryInfo(SessionSignalingMemoryInfo* memoryInfo);
int APS5_VABI sceNpSessionSignalingGetConnectionStatistics(SessionSignalingConnectionStatistics* statistics);
int APS5_VABI sceNpSessionSignalingGetConnectionFromPeerAddress(std::uint32_t contextId, const SessionSignalingPeerAddress* peerAddress, std::uint32_t* connectionId);
int APS5_VABI sceNpSessionSignalingGetGroupFromSessionId(std::uint32_t contextId, const char* sessionId, std::uint32_t* groupId);
int APS5_VABI sceNpSessionSignalingGetGroupInfo(std::uint32_t contextId, std::uint32_t groupId, SessionSignalingGroupInfo* groupInfo);
int APS5_VABI sceNpSessionSignalingGetConnectionFromNetAddress2(std::uint32_t contextId, std::uint32_t groupId, std::uint32_t address, std::uint16_t port, std::uint32_t* connectionId);
int APS5_VABI sceNpSessionSignalingCreateJoinActivate();
int APS5_VABI sceNpSessionSignalingGetConnectionFromNetAddress(std::uint32_t contextId, std::uint32_t address, std::uint16_t port, std::uint32_t* connectionId);
int APS5_VABI sceNpSessionSignalingGetCustomProperty();
int APS5_VABI sceNpSessionSignalingGetGroupFromPeerAddress();
int APS5_VABI sceNpSessionSignalingInitializeWithPort();
int APS5_VABI sceNpSessionSignalingJoinActivate();
int APS5_VABI sceNpSessionSignalingSetCustomProperty();

}
