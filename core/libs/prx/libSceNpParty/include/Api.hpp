#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpPartyGetState(std::uint16_t* state);
int APS5_VABI sceNpPartyCheckCallback(void);
int APS5_VABI sceNpPartyCreate(void);
int APS5_VABI sceNpPartyCreateA(void);
int APS5_VABI sceNpPartyGetId(void* id);
int APS5_VABI sceNpPartyGetMemberInfo(std::uint16_t, void* memberInfo);
int APS5_VABI sceNpPartyGetMemberInfoA(std::uint16_t, void* memberInfo);
int APS5_VABI sceNpPartyGetMemberSessionInfo(std::uint16_t, void* memberGameSessionInfo);
int APS5_VABI sceNpPartyGetMemberVoiceInfo(std::uint16_t, std::uint16_t, std::uint8_t* memberVoiceState);
int APS5_VABI sceNpPartyGetMembers(void* memberList);
int APS5_VABI sceNpPartyGetMembersA(void* memberList);
int APS5_VABI sceNpPartyGetStateAsUser(void);
int APS5_VABI sceNpPartyGetStateAsUserA(std::int32_t, std::uint16_t* state);
int APS5_VABI sceNpPartyGetVoiceChatPriority(void);
int APS5_VABI sceNpPartyInitialize(const void* param);
int APS5_VABI sceNpPartyJoin(void);
int APS5_VABI sceNpPartyLeave(void);
int APS5_VABI sceNpPartyRegisterHandler(const void* handlers, void* userdata);
int APS5_VABI sceNpPartyRegisterHandlerA(const void* handlers, void*);
int APS5_VABI sceNpPartyRegisterPrivateHandler(void);
int APS5_VABI sceNpPartySendBinaryMessage(const void* sendParams);
int APS5_VABI sceNpPartySetVoiceChatPriority(void);
int APS5_VABI sceNpPartyShowInvitationList(void);
int APS5_VABI sceNpPartyShowInvitationListA(void);
int APS5_VABI sceNpPartyTerminate(void);
int APS5_VABI sceNpPartyUnregisterPrivateHandler(void);

}
