#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI sceTextToSpeechCancelImpl(std::uint32_t id);
int APS5_VABI sceTextToSpeechCloseImpl(std::uint32_t id);
int APS5_VABI sceTextToSpeechGetStatusImpl(std::uint32_t id, void* status);
int APS5_VABI sceTextToSpeechInitImpl(std::uint32_t id, const void* parameter);
int APS5_VABI sceTextToSpeechOpenImpl(std::uint32_t id);
int APS5_VABI sceTextToSpeechPauseImpl(std::uint32_t id);
int APS5_VABI sceTextToSpeechResumeImpl(std::uint32_t id);
int APS5_VABI sceTextToSpeechSpeakContinuousImpl(int id);
int APS5_VABI sceTextToSpeechSpeakImpl(std::uint32_t id);
int APS5_VABI sceTextToSpeechTermImpl(std::uint32_t id);

}
