#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceUserServiceSetAccessibilityKeyremapEnableAston();
int APS5_VABI sceUserServiceSetHmd2CinematicAudioRotation();
int APS5_VABI sceUserServiceSetIsPersonalizedHrtfEnabled();
int APS5_VABI sceUserServiceSetUserWithHearingAssist();
int APS5_VABI sceUserServiceSetVoiceAgentEnableByButton();

}
