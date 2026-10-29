#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVoiceInit(const VoiceInitParam* param, std::int32_t version);
int APS5_VABI sceVoiceSetThreadsParams(const VoiceThreadsParams* params);
int APS5_VABI sceVoiceStart(const VoiceStartParam* param);
int APS5_VABI sceVoiceStop(void);
int APS5_VABI sceVoiceEnd_nid_postfix(void);
int APS5_VABI sceVoiceCreatePort(std::uint32_t* portId, const VoicePortParam* param);
int APS5_VABI sceVoiceDeletePort(uint32_t portId);
int APS5_VABI sceVoiceConnectIPortToOPort(uint32_t inputPortId, uint32_t outputPortId);
int APS5_VABI sceVoiceDisconnectIPortFromOPort(uint32_t inputPortId, uint32_t outputPortId);
int APS5_VABI sceVoiceGetBitRate(std::uint32_t portId, std::uint32_t* bitrate);
int APS5_VABI sceVoiceSetVolume(std::uint32_t portId, float volume);
int APS5_VABI sceVoiceGetVolume(std::uint32_t portId, float* volume);
int APS5_VABI sceVoiceGetPortInfo(uint32_t portId, VoicePortInfo* info);
int APS5_VABI sceVoiceGetPortAttr(std::uint32_t portId, std::int32_t attr, void* value, std::int32_t size);
int APS5_VABI sceVoiceReadFromOPort(uint32_t outputPortId, void* data, uint32_t* size);
int APS5_VABI sceVoiceWriteToIPort(uint32_t inputPortId, const void* data, uint32_t* size, int16_t frameGaps);
int APS5_VABI sceVoiceSetMuteFlag(std::uint32_t portId, std::uint8_t muted);
int APS5_VABI sceVoiceGetMuteFlag(std::uint32_t portId, bool* muted);
int APS5_VABI sceVoiceGetResourceInfo(VoiceResourceInfo* info);
int APS5_VABI sceVoiceEnableChat(void);
int APS5_VABI sceVoiceResetPort(std::uint32_t portId);
int APS5_VABI sceVoicePausePort(std::uint32_t portId);
int APS5_VABI sceVoiceResumePort(std::uint32_t portId);
int APS5_VABI sceVoiceDisableChat(void);
int APS5_VABI sceVoiceInitHQ(const VoiceInitParam* param, std::int32_t version);
int APS5_VABI sceVoiceGetMemorySize(void);
int APS5_VABI sceVoiceDeviceInCustomerData(void);
int APS5_VABI sceVoiceQAEnableRecording(void);
int APS5_VABI sceVoiceVADAdjustment(float param1, float param2);
int APS5_VABI sceVoiceVADPVTAdjustment(void);
int APS5_VABI sceVoiceVADSetVersion(std::int32_t version);
int APS5_VABI sceVoiceSetMuteFlagAll(bool muted);
int APS5_VABI sceVoiceSetBitRate(std::uint32_t portId, std::uint32_t bitrate);
int APS5_VABI sceVoiceUpdatePort(std::uint32_t portId, const VoicePortParam* param);
int APS5_VABI sceVoicePausePortAll(void);
int APS5_VABI sceVoiceResumePortAll(void);

}
