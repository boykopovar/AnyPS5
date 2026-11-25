#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI sceUserServiceSetAccessibilityKeyremapEnableAston(std::int32_t userId, std::int32_t enabled);
int APS5_VABI sceUserServiceSetHmd2CinematicAudioRotation(std::int32_t userId, std::int32_t rotation);
int APS5_VABI sceUserServiceSetIsPersonalizedHrtfEnabled(std::int32_t userId, std::int32_t enabled);
int APS5_VABI sceUserServiceSetUserWithHearingAssist(std::int32_t userId);
int APS5_VABI sceUserServiceSetVoiceAgentEnableByButton();

}
