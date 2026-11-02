#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceAudioOut2ContextAdvance(AudioOut2ContextHandle ctx);
int APS5_VABI sceAudioOut2ContextCreate(const AudioOut2ContextParam* params, void* buffer, size_t bufferSize, AudioOut2ContextHandle* ctx);
int APS5_VABI sceAudioOut2ContextDestroy(AudioOut2ContextHandle ctx);
int APS5_VABI sceAudioOut2ContextGetQueueLevel(AudioOut2ContextHandle ctx, uint32_t* queueLevel, uint32_t* availableQueue);
int APS5_VABI sceAudioOut2ContextPush(AudioOut2ContextHandle ctx, uint32_t blocking);
int APS5_VABI sceAudioOut2ContextQueryMemory(const AudioOut2ContextParam* params, size_t* memorySize);
int APS5_VABI sceAudioOut2ContextResetParam(AudioOut2ContextParam* params);
int APS5_VABI sceAudioOut2ContextSetAttributes(AudioOut2ContextHandle ctx, const AudioOut2Attribute* attributes, uint32_t num);
int APS5_VABI sceAudioOut2LoInit(void);
int APS5_VABI sceAudioOut2LoTerminate(void);
int APS5_VABI sceAudioOut2LoContextQueryMemory(const AudioOut2ContextParam* params, size_t* memorySize);
int APS5_VABI sceAudioOut2LoContextCreate(const AudioOut2ContextParam* params, void* buffer, size_t bufferSize, AudioOut2ContextHandle* ctx);
int APS5_VABI sceAudioOut2LoContextDestroy(AudioOut2ContextHandle ctx);
int APS5_VABI sceAudioOut2LoContextSetAttributes(AudioOut2ContextHandle ctx, const AudioOut2Attribute* attributes, uint32_t num);
int APS5_VABI sceAudioOut2LoContextAdvance(AudioOut2ContextHandle ctx);
int APS5_VABI sceAudioOut2LoContextPush(AudioOut2ContextHandle ctx, uint32_t blocking);
int APS5_VABI sceAudioOut2LoContextGetQueueLevel(AudioOut2ContextHandle ctx, uint32_t* queueLevel, uint32_t* availableQueue);
int APS5_VABI sceAudioOut2LoPortCreate(AudioOut2ContextHandle ctx, const AudioOut2PortParam* params, AudioOut2PortHandle* port);
int APS5_VABI sceAudioOut2LoPortDestroy(AudioOut2PortHandle port);
int APS5_VABI sceAudioOut2LoPortSetAttributes(AudioOut2PortHandle port, const AudioOut2Attribute* attributes, uint32_t num);
int APS5_VABI sceAudioOut2LoPortGetState(AudioOut2PortHandle port, AudioOut2PortState* state);
int APS5_VABI sceAudioOut2MasteringInit(uint32_t flags);
int APS5_VABI sceAudioOut2MasteringInitEx(uint32_t flags);
int APS5_VABI sceAudioOut2MasteringTerm(void);
int APS5_VABI sceAudioOut2MasteringGetState(AudioOut2MasteringStatesHeader* state, uint32_t output, AudioOut2UserHandle user);
int APS5_VABI sceAudioOut2MasteringSetParam(const AudioOut2MasteringParamsHeader* param, uint32_t output, uint32_t flags);
int APS5_VABI sceAudioOut2PortCreate(AudioOut2ContextHandle ctx, const AudioOut2PortParam* params, AudioOut2PortHandle* port);
int APS5_VABI sceAudioOut2PortDestroy(AudioOut2PortHandle port);
int APS5_VABI sceAudioOut2PortGetState(AudioOut2PortHandle port, AudioOut2PortState* state);
int APS5_VABI sceAudioOut2PortSetAttributes(AudioOut2PortHandle port, const AudioOut2Attribute* attributes, uint32_t num);
int APS5_VABI sceAudioOut2SpeakerArrayCreate(AudioOut2SpeakerArrayHandle* handle, const void* vbapParams, const void* ambiParams);
int APS5_VABI sceAudioOut2SpeakerArrayDestroy(AudioOut2SpeakerArrayHandle handle);
int APS5_VABI sceAudioOut2GetSpeakerArrayAmbisonicsCoefficients(AudioOut2SpeakerArrayHandle handle, uint32_t ambisonicsChannel, float* coefficients, uint32_t numCoefficients);
int APS5_VABI sceAudioOut2GetSpeakerArrayCoefficients(AudioOut2SpeakerArrayHandle handle, AudioOut2Position pos, float spread, float* coefficients, uint32_t numCoefficients, uint8_t heightAware, float downmixSpreadRadius);
size_t APS5_VABI sceAudioOut2GetSpeakerArrayMemorySize(uint32_t numSpeakers, uint8_t is3d, uint8_t isAmbisonics);
int APS5_VABI sceAudioOut2GetSpeakerInfo(AudioOut2SpeakerInfo* info, uint32_t flags);
int APS5_VABI sceAudioOut2Initialize(void);
int APS5_VABI sceAudioOut2InitializeEx(std::uint32_t param);
int APS5_VABI sceAudioOut2Set3DLatency(int userId, std::uint32_t output);
int APS5_VABI sceAudioOut2GetSystemState(AudioOut2SystemState* state);
int APS5_VABI sceAudioOut2SetSystemDebugState(const AudioOut2SystemDebugStateParam* param);
int APS5_VABI sceAudioOut2EnableChat(void);
int APS5_VABI sceAudioOut2DisableChat(void);
int APS5_VABI sceAudioOut2UserCreate(uint32_t userId, AudioOut2UserHandle* handle);
int APS5_VABI sceAudioOut2UserDestroy(AudioOut2UserHandle handle);
int APS5_VABI sceAudioOut2UserGetSupportedAttributes(AudioOut2UserHandle handle, uint32_t* contextAttributes, uint32_t* portAttributes);
int APS5_VABI sceAudioOut2UserCreateEx(void);
int APS5_VABI sceAudioOut2ArbitrationInitialize(void);
int APS5_VABI sceAudioOut2MbusInit(void);
int APS5_VABI sceAudioOut2MbusSetPortConnections(void);
int APS5_VABI sceAudioOut2MbusSetConnections(void);
int APS5_VABI sceAudioOut2MbusSetPortStatuses(void);
int APS5_VABI sceAudioOut2MbusSetHeadphoneOutMode(void);
int APS5_VABI sceAudioOut2MbusUserLoginFor3DAudio(void);
int APS5_VABI sceAudioOut2MbusSetDeviceConnections(void);
int APS5_VABI sceAudioOut2MbusSetMorpheusParam(void);
int APS5_VABI sceAudioOut2MbusSetCaesarParam(void);
int APS5_VABI sceAudioOut2MbusSetSparkParam(void);
int APS5_VABI sceAudioOut2AllAudioMainToHeadphoneForDiag(void);
int APS5_VABI sceAudioOut2LoPortGetStateEx(void);
int APS5_VABI sceAudioOut2ReportRegisterHandler(void);
int APS5_VABI sceAudioOut2ReportUnregisterHandler(void);
int APS5_VABI sceAudioOut2GetTvCorrectionInfo(void);
int APS5_VABI sceAudioOut2GetIpcHandle(void);
int APS5_VABI sceAudioOut2GetSystemStateEx(void);
int APS5_VABI sceAudioOut2SetDebugHeadphoneMode(void);
int APS5_VABI sceAudioOut2DebugStateCtrl(void);
int APS5_VABI sceAudioOut2ContextBedWrite(void);
int APS5_VABI sceAudioOut2PortCreateEx(void);
int APS5_VABI sceAudioOut2Terminate(void);
int APS5_VABI sceAudioOut2PortRegister(void);
int APS5_VABI sceAudioOut2PortUnregister(void);
int APS5_VABI sceAudioOut2GetAmbisonicsEncodeCoefficients(void);
int APS5_VABI sceAudioOut2GetHrtfIdForCronos(void);

}
