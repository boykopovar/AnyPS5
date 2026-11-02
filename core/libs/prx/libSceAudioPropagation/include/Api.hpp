#pragma once

#include "prx/libSceAudioPropagation/AudioPropagation.hpp"
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceAudioPropagationReportApi(std::uint32_t category, AudioPropagationHandle handle, std::int32_t result, std::int32_t operation, const char* function, const char* format, ...);
int APS5_VABI sceAudioPropagationResetAttributes(AudioPropagation::Attribute* attributes, std::uint32_t count);
int APS5_VABI sceAudioPropagationSourceQueryInfo(AudioPropagationHandle source, AudioPropagation::SourceInfo* info);
int APS5_VABI sceAudioPropagationSystemLock(AudioPropagationHandle system);
int APS5_VABI sceAudioPropagationSystemQueryInfo(AudioPropagationHandle system, AudioPropagation::SystemInfo* info, std::size_t size);
int APS5_VABI sceAudioPropagationReportRegisterHandler(std::uint32_t category, AudioPropagation::ReportHandler handler, void* userData, AudioPropagation::ReportRegistration** registration);
int APS5_VABI sceAudioPropagationReportUnregisterHandler(AudioPropagation::ReportRegistration* registration);
int APS5_VABI sceAudioPropagationSystemEnumHandles(AudioPropagationHandle* handles, std::uint32_t capacity);
int APS5_VABI sceAudioPropagationSystemUnlock(AudioPropagationHandle system);

}
