#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVoiceQoSInit(void* memBlock, uint32_t memSize, int32_t appType);
int APS5_VABI sceVoiceQoSEnd();
int APS5_VABI sceVoiceQoSCreateLocalEndpoint(int32_t* localId, int32_t userId, int32_t deviceInId, int32_t deviceOutId);
int APS5_VABI sceVoiceQoSDeleteLocalEndpoint(int32_t localId);
int APS5_VABI sceVoiceQoSCreateRemoteEndpoint(int32_t* remoteId);
int APS5_VABI sceVoiceQoSDeleteRemoteEndpoint(int32_t remoteId);
int APS5_VABI sceVoiceQoSConnect(int32_t* connectionId, int32_t localId, int32_t remoteId);
int APS5_VABI sceVoiceQoSDisconnect(int32_t connectionId);
int APS5_VABI sceVoiceQoSSetConnectionAttribute(std::int32_t connectionId, std::int32_t attributeId, const void* value, std::int32_t size);
int APS5_VABI sceVoiceQoSSetLocalEndpointAttribute(int32_t localId, int32_t attributeId, const void* value, int32_t size);
int APS5_VABI sceVoiceQoSSetRemoteEndpointAttribute(int32_t remoteId, int32_t attributeId, const void* value, int32_t size);
int APS5_VABI sceVoiceQoSReadPacket(int32_t connectionId, void* buffer, uint32_t* size);
int APS5_VABI sceVoiceQoSWritePacket(int32_t connectionId, const void* buffer, uint32_t* size);
int APS5_VABI sceVoiceQoSGetLocalEndpointAttribute(int32_t localId, int32_t attributeId, void* value, int32_t size);
int APS5_VABI sceVoiceQoSGetRemoteEndpointAttribute(int32_t remoteId, int32_t attributeId, void* value, int32_t size);
int APS5_VABI sceVoiceQoSGetConnectionAttribute(int32_t connectionId, int32_t attributeId, void* value, int32_t size);
int APS5_VABI sceVoiceQoSGetLocalEndpoint(int32_t connectionId, int32_t* localId);
int APS5_VABI sceVoiceQoSGetRemoteEndpoint(int32_t connectionId, int32_t* remoteId);
int APS5_VABI sceVoiceQoSEnableChat();
int APS5_VABI sceVoiceQoSDisableChat();
int APS5_VABI sceVoiceQoSGetStatus(int32_t connectionId, int32_t statusId, void* value, int32_t size);
int APS5_VABI sceVoiceQoSDebugGetStatus(std::int32_t connectionId, std::int32_t statusId, void* value, std::int32_t size);
int APS5_VABI sceVoiceQoSInitHQ(void* memory, std::uint32_t memorySize, std::int32_t appType);
int APS5_VABI sceVoiceQoSSetMode();
int APS5_VABI sceVoiceQoSTerminate();

}
