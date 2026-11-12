#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNpSessionSignaling/include/Api.hpp"
#include <cstdint>

struct NpRemotePlaySessionSignalingLocalNetInfo {
    std::uint32_t localAddress;
    std::uint32_t mappedAddress;
    std::uint32_t mappedPort;
};

extern "C" {

int APS5_VABI sceNpRemotePlaySessionSignalingCreateJoinSession();
int32_t APS5_VABI sceNpRemotePlaySessionSignalingGetConnectionStatus(std::uint32_t contextId, std::uint32_t connectionId, std::int32_t* status, std::uint32_t* peerAddress, std::uint16_t* peerPort);
int APS5_VABI sceNpRemotePlaySessionSignalingGetCustomProperty(std::uint32_t contextId, std::uint32_t* property);
int32_t APS5_VABI sceNpRemotePlaySessionSignalingGetLocalNetInfo(std::uint32_t contextId, NpRemotePlaySessionSignalingLocalNetInfo* info);
int APS5_VABI sceNpRemotePlaySessionSignalingGetMemoryInfo(SessionSignalingMemoryInfo* memoryInfo);
int APS5_VABI sceNpRemotePlaySessionSignalingInitialize(const SessionSignalingInitParam* param);
int APS5_VABI sceNpRemotePlaySessionSignalingJoinSession();
int APS5_VABI sceNpRemotePlaySessionSignalingLeaveSession(std::uint32_t contextId);
int APS5_VABI sceNpRemotePlaySessionSignalingSetCustomProperty(std::uint32_t contextId, const std::uint32_t* property, std::uint32_t* previous);
int APS5_VABI sceNpRemotePlaySessionSignalingTerminate(void);
int APS5_VABI sceNpRemotePlaySessionSignalingAbortConnection();
int APS5_VABI sceNpRemotePlaySessionSignalingEstablishConnection();
int APS5_VABI sceNpRemotePlaySessionSignalingManualUdpCreateJoinSession();
int APS5_VABI sceNpRemotePlaySessionSignalingManualUdpJoinSession();

}
