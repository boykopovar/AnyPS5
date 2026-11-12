#pragma once

#include "SceTypes.hpp"

struct NpPartyCreateParam {
    std::uint8_t data[0x310];
};

struct NpPartyCreateAParam {
    std::uint8_t data[0x110];
};

struct NpPartyJoinParam {
    std::uint8_t data[0x34];
};

struct NpPartyLeaveParam {
    const void* data;
    std::uint16_t size;
    std::uint8_t reserved[6];
};

struct NpPartyShowInvitationListParam {
    std::uint8_t data[0x250];
};

struct NpPartyShowInvitationListAParam {
    std::uint8_t data[0x50];
};

extern "C" {

int APS5_VABI sceNpPartyGetState(std::uint16_t* state);
int APS5_VABI sceNpPartyCheckCallback(void);
int APS5_VABI sceNpPartyCreate(std::int32_t userId, const NpPartyCreateParam* param);
int APS5_VABI sceNpPartyCreateA(std::int32_t userId, const NpPartyCreateAParam* param);
int APS5_VABI sceNpPartyGetId(void* id);
int APS5_VABI sceNpPartyGetMemberInfo(std::uint16_t, void* memberInfo);
int APS5_VABI sceNpPartyGetMemberInfoA(std::uint16_t, void* memberInfo);
int APS5_VABI sceNpPartyGetMemberSessionInfo(std::uint16_t, void* memberGameSessionInfo);
int APS5_VABI sceNpPartyGetMemberVoiceInfo(std::uint16_t, std::uint16_t, std::uint8_t* memberVoiceState);
int APS5_VABI sceNpPartyGetMembers(void* memberList);
int APS5_VABI sceNpPartyGetMembersA(void* memberList);
int APS5_VABI sceNpPartyGetStateAsUser(std::int32_t userId, std::uint16_t* state);
int APS5_VABI sceNpPartyGetStateAsUserA(std::int32_t userId, std::uint16_t* state);
int APS5_VABI sceNpPartyGetVoiceChatPriority(std::uint32_t* priority);
int APS5_VABI sceNpPartyInitialize(const void* param);
int APS5_VABI sceNpPartyJoin(std::int32_t userId, NpPartyJoinParam param);
int APS5_VABI sceNpPartyLeave(std::uint16_t userId, const NpPartyLeaveParam* param);
int APS5_VABI sceNpPartyRegisterHandler(const void* handlers, void* userdata);
int APS5_VABI sceNpPartyRegisterHandlerA(const void* handlers, void*);
int APS5_VABI sceNpPartyRegisterPrivateHandler(void* handler);
int APS5_VABI sceNpPartySendBinaryMessage(const void* sendParams);
int APS5_VABI sceNpPartySetVoiceChatPriority(std::int32_t priority);
int APS5_VABI sceNpPartyShowInvitationList(std::uint16_t userId, const NpPartyShowInvitationListParam* param);
int APS5_VABI sceNpPartyShowInvitationListA(std::uint16_t userId, const NpPartyShowInvitationListAParam* param);
int APS5_VABI sceNpPartyTerminate(void);
int APS5_VABI sceNpPartyUnregisterPrivateHandler(void);

}
