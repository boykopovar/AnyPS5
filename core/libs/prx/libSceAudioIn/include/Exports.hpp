#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceAudioInExtClose();
int APS5_VABI sceAudioInExtInput();
int APS5_VABI sceAudioInExtOpen();
int APS5_VABI sceAudioInExtSetAecMode();
int APS5_VABI sceAudioInSetConnectionsForUser();
int APS5_VABI sceAudioInSetFocusForUser();

}
