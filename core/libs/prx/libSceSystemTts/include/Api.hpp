#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSystemTtsCancel();
int APS5_VABI sceSystemTtsIsAccessibilityAvailable();
int APS5_VABI sceSystemTtsRegisterCallback();
int APS5_VABI sceSystemTtsSpeakRequest();
int APS5_VABI sceSystemTtsUnregisterCallback();
int APS5_VABI sceSystemTtsConvertText();
int APS5_VABI sceSystemTtsPauseResume();
int APS5_VABI sceSystemTtsRepeat();
int APS5_VABI sceSystemTtsWait();

}
