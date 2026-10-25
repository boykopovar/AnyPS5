#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVoiceChatInitialize(void* param);
int APS5_VABI sceVoiceChatTerminate(void);
int APS5_VABI sceVoiceChatRegisterHandlers(void* a);
int APS5_VABI sceVoiceChatRegisterMicEventHandler(void* a, void* b);
int APS5_VABI sceVoiceChatProcessEvent(void);
int APS5_VABI sceVoiceChatDeleteRequest(int requestId);
int APS5_VABI sceVoiceChatCreateRequest(void* a, void* b);
int APS5_VABI NAME(int requestId, void* a, void* b);
int APS5_VABI sceVoiceChatAbortRequest(int requestId);
int APS5_VABI sceVoiceChatGetActiveChannel(void* channel);
int APS5_VABI sceVoiceChatGetChannelMemberActiveChannelMatchingState(void* a, void* b);
int APS5_VABI sceVoiceChatGetChannelMemberVoiceConnectionState(void* a, void* b);
int APS5_VABI sceVoiceChatGetChatChannelMemberInfoList(void* a, void* b);
int APS5_VABI sceVoiceChatGetChatGroupMemberInfoList(void* a, void* b);
int APS5_VABI sceVoiceChatGetMicState(void* state);
int APS5_VABI sceVoiceChatGetUserVoiceVolumeGain(void* a, void* gain);
int APS5_VABI sceVoiceChatSetUserVoiceVolumeGain(void* a, int32_t gain);
int APS5_VABI sceVoiceChatRegisterChannelEventHandler(void* a, void* b);
int APS5_VABI sceVoiceChatRegisterDaemonEventHandler(void* a, void* b);
int APS5_VABI sceVoiceChatRegisterMemberEventHandler(void* a, void* b);
int APS5_VABI sceVoiceChatRegisterPartyUserStateChangedHandler(void* a, void* b);
int APS5_VABI sceVoiceChatRegisterVoiceEventHandler(void* a, void* b);
int APS5_VABI sceVoiceChatStartAccessibilityEvent(void* a);
int APS5_VABI sceVoiceChatStartChannelEvent(void* a);
int APS5_VABI sceVoiceChatStartDaemonEvent(void* a);
int APS5_VABI sceVoiceChatStartMemberEvent(void* a);
int APS5_VABI sceVoiceChatStartRangedMemberEvent(void* a);
int APS5_VABI sceVoiceChatStartVoiceEvent(void* a);
int APS5_VABI sceVoiceChatStopAccessibilityEvent(void* a);
int APS5_VABI sceVoiceChatStopChannelEvent(void* a);
int APS5_VABI sceVoiceChatStopDaemonEvent(void* a);
int APS5_VABI sceVoiceChatStopMemberEvent(void* a);
int APS5_VABI sceVoiceChatStopRangedMemberEvent(void* a);
int APS5_VABI sceVoiceChatStopVoiceEvent(void* a);
int APS5_VABI sceVoiceChatUnregisterAccessibilityEventHandler(void* a);
int APS5_VABI sceVoiceChatUnregisterChannelEventHandler(void* a);
int APS5_VABI sceVoiceChatUnregisterDaemonEventHandler(void* a);
int APS5_VABI sceVoiceChatUnregisterMemberEventHandler(void* a);
int APS5_VABI sceVoiceChatUnregisterMicEventHandler(void* a);
int APS5_VABI sceVoiceChatUnregisterPartyUserStateChangedHandler(void* a);
int APS5_VABI sceVoiceChatUnregisterRangedMemberEventHandler(void* a);
int APS5_VABI sceVoiceChatUnregisterVoiceEventHandler(void* a);

}
