#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSystemTtsCancel();
int APS5_VABI sceSystemTtsIsAccessibilityAvailable();
int APS5_VABI sceSystemTtsRegisterCallback();
int APS5_VABI sceSystemTtsSpeakRequest();
int APS5_VABI sceSystemTtsUnregisterCallback();

}
