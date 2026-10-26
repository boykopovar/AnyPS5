#pragma once

#include "SceTypes.hpp"

struct Audio3dSpeakerArrayParameters {
    const AudioOut2Position* positions;
    std::uint32_t numSpeakers;
    std::uint8_t is3d;
    void* memory;
    std::size_t memorySize;
};

extern "C" {

void APS5_VABI sceAudio3dGetDefaultOpenParameters(Audio3dOpenParameters* p);
int APS5_VABI sceAudio3dInitialize(int64_t reserved);
int APS5_VABI sceAudio3dTerminate();
int APS5_VABI sceAudio3dPortAdvance(uint32_t portId);
int APS5_VABI sceAudio3dPortClose(uint32_t portId);
int APS5_VABI sceAudio3dPortGetQueueLevel(uint32_t portId, uint32_t* queueLevel, uint32_t* queueAvailable);
int APS5_VABI sceAudio3dPortGetAttributesSupported(uint32_t portId, uint32_t* attributes, uint32_t* numAttributes);
int APS5_VABI sceAudio3dPortOpen(int userId, const Audio3dOpenParameters* parameters, uint32_t* id);
int APS5_VABI sceAudio3dPortPush(uint32_t portId, uint32_t blocking);
int APS5_VABI sceAudio3dPortSetAttribute(uint32_t portId, uint32_t attributeId, const void* attribute, size_t attributeSize);
int APS5_VABI sceAudio3dObjectReserve(uint32_t portId, uint32_t* objectId);
int APS5_VABI sceAudio3dObjectUnreserve(uint32_t portId, uint32_t objectId);
int APS5_VABI sceAudio3dObjectSetAttributes(uint32_t portId, uint32_t objectId, uint64_t numAttributes, const Audio3dAttribute* attributeArray);
int APS5_VABI sceAudio3dPortFlush(uint32_t portId);
int APS5_VABI sceAudio3dBedWrite2(uint32_t portId, uint32_t numChannels, uint32_t format, const void* buffer, uint32_t numSamples, uint32_t outputRoute, bool);
int APS5_VABI sceAudio3dBedWrite(uint32_t portId, uint32_t numChannels, uint32_t format, const void* buffer, uint32_t numSamples);
int APS5_VABI sceAudio3dAudioOutOpen(uint32_t portId, int userId, std::int32_t type, std::int32_t index, uint32_t len, uint32_t freq, uint32_t param);
int APS5_VABI sceAudio3dAudioOutClose(std::int32_t handle);
int APS5_VABI sceAudio3dAudioOutOutput(std::int32_t handle, const void* ptr);
int APS5_VABI sceAudio3dAudioOutOutputs(AudioOutOutputParam* param, uint32_t num);
int APS5_VABI sceAudio3dStrError(void);
int APS5_VABI sceAudio3dPortOpenEx(void);
int APS5_VABI sceAudio3dPortCreate(void);
int APS5_VABI sceAudio3dPortDestroy(void);
int APS5_VABI sceAudio3dPortGetStatus(void);
int APS5_VABI sceAudio3dReportRegisterHandler(void);
int APS5_VABI sceAudio3dReportUnregisterHandler(void);
size_t APS5_VABI sceAudio3dGetSpeakerArrayMemorySize(uint32_t numSpeakers, uint8_t is3d);
int APS5_VABI sceAudio3dCreateSpeakerArray(AudioOut2SpeakerArrayHandle* handle, const Audio3dSpeakerArrayParameters* parameters);
int APS5_VABI sceAudio3dDeleteSpeakerArray(AudioOut2SpeakerArrayHandle handle);
int APS5_VABI sceAudio3dGetSpeakerArrayMixCoefficients(AudioOut2SpeakerArrayHandle handle, AudioOut2Position pos, float spread, float* coefficients, uint32_t numCoefficients);
int APS5_VABI sceAudio3dGetSpeakerArrayMixCoefficients2(AudioOut2SpeakerArrayHandle handle, AudioOut2Position pos, float spread, float* coefficients, uint32_t numCoefficients, uint8_t heightAware, float downmixSpreadRadius);

}
