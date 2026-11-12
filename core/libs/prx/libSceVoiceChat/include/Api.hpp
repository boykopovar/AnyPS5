#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVoiceChatInitialize(void* param);
int APS5_VABI sceVoiceChatTerminate(void);
int APS5_VABI sceVoiceChatRegisterHandlers(void* handlers, void* userArg);
int APS5_VABI sceVoiceChatRegisterMicEventHandler(std::int32_t userId, void* handler, void* userArg);
int APS5_VABI sceVoiceChatProcessEvent(void);
int APS5_VABI sceVoiceChatDeleteRequest(int requestId);
int APS5_VABI sceVoiceChatCreateRequest(void* a, void* b);
int APS5_VABI sceVoiceChatRequestRegisterSession(int requestId, void* a, void* b);
int APS5_VABI sceVoiceChatRequestUnregisterSession(int requestId, void* a, void* b);
int APS5_VABI sceVoiceChatRequestCreateGameSessionVoiceChatChannel(int requestId, void* a, void* b);
int APS5_VABI sceVoiceChatRequestDeleteGameSessionVoiceChatChannel(int requestId, void* a, void* b);
int APS5_VABI sceVoiceChatRequestJoinGameSessionVoiceChatChannel(int requestId, void* a, void* b);
int APS5_VABI sceVoiceChatRequestLeaveGameSessionVoiceChatChannel(int requestId, void* a, void* b);
int APS5_VABI sceVoiceChatRequestCreatePlayerSessionVoiceChatChannel(int requestId, void* a, void* b);
int APS5_VABI sceVoiceChatRequestDeletePlayerSessionVoiceChatChannel(int requestId, void* a, void* b);
int APS5_VABI sceVoiceChatRequestJoinPlayerSessionVoiceChatChannel(int requestId, void* a, void* b);
int APS5_VABI sceVoiceChatRequestLeavePlayerSessionVoiceChatChannel(int requestId, void* a, void* b);
int APS5_VABI sceVoiceChatRequestCreateVoiceChatGroup(int requestId, void* a, void* b);
int APS5_VABI sceVoiceChatRequestJoinVoiceChatGroup(int requestId, void* a, void* b);
int APS5_VABI sceVoiceChatRequestLeaveVoiceChatGroup(int requestId, void* a, void* b);
int APS5_VABI sceVoiceChatRequestDeleteVoiceChatGroup(int requestId, void* a, void* b);
int APS5_VABI sceVoiceChatRequest0(int requestId, void* a, void* b);
int APS5_VABI sceVoiceChatRequest1(int requestId, void* a, void* b);
int APS5_VABI sceVoiceChatRequestAcsTts(int requestId, void* a, void* b);
int APS5_VABI sceVoiceChatRequestAction(int requestId, void* a, void* b);
int APS5_VABI sceVoiceChatRequestSetMemberVolume(int requestId, void* a, void* b);
int APS5_VABI sceVoiceChatRequestUpdateGameSessionVoiceChatChannelName(int requestId, void* a, void* b);
int APS5_VABI sceVoiceChatRequestUpdatePlayerSessionVoiceChatChannelName(int requestId, void* a, void* b);
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
