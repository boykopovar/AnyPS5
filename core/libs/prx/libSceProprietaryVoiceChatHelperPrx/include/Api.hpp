#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceProprietaryVoiceChatHelperInitialize(void);
int APS5_VABI sceProprietaryVoiceChatHelperTerminate(void);
int APS5_VABI sceProprietaryVoiceChatHelperSetVoiceChatState(void);
int APS5_VABI sceProprietaryVoiceChatHelperGetVoiceChatUsageState(void);
int APS5_VABI sceProprietaryVoiceChatHelperGetVoiceChat(void);

}
