#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceTextToSpeechCancelImpl();
int APS5_VABI sceTextToSpeechCloseImpl();
int APS5_VABI sceTextToSpeechGetStatusImpl();
int APS5_VABI sceTextToSpeechInitImpl();
int APS5_VABI sceTextToSpeechOpenImpl();
int APS5_VABI sceTextToSpeechPauseImpl();
int APS5_VABI sceTextToSpeechResumeImpl();
int APS5_VABI sceTextToSpeechSpeakContinuousImpl();
int APS5_VABI sceTextToSpeechSpeakImpl();
int APS5_VABI sceTextToSpeechTermImpl();

}
