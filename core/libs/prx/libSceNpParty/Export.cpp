#include "prx/libSceNpParty/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int NP_PARTY_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80552502u);
constexpr int NP_PARTY_ERROR_NOT_INITIALIZED = static_cast<int>(0x80552503u);
constexpr int NP_PARTY_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80552504u);
constexpr int NP_PARTY_ERROR_NOT_IN_PARTY = static_cast<int>(0x80552506u);
constexpr std::uint16_t NP_PARTY_STATE_NOT_IN_PARTY = 2;

std::atomic<bool> g_initialized{false};

int notInParty(const void* out) {
    if (!out) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    return NP_PARTY_ERROR_NOT_IN_PARTY;
}

}

extern "C" {

int APS5_VABI sceNpPartyGetState(std::uint16_t* state) {
    if (!state) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    *state = NP_PARTY_STATE_NOT_IN_PARTY;
    return 0;
}

int APS5_VABI sceNpPartyCheckCallback(void) {
    if (!g_initialized) return NP_PARTY_ERROR_NOT_INITIALIZED;
    return 0;
}

int APS5_VABI sceNpPartyCreate(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpPartyCreateA(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpPartyGetId(void* id) {
    return notInParty(id);
}

int APS5_VABI sceNpPartyGetMemberInfo(std::uint16_t, void* memberInfo) {
    return notInParty(memberInfo);
}

int APS5_VABI sceNpPartyGetMemberInfoA(std::uint16_t, void* memberInfo) {
    return notInParty(memberInfo);
}

int APS5_VABI sceNpPartyGetMemberSessionInfo(std::uint16_t, void* memberGameSessionInfo) {
    return notInParty(memberGameSessionInfo);
}

int APS5_VABI sceNpPartyGetMemberVoiceInfo(std::uint16_t, std::uint16_t, std::uint8_t* memberVoiceState) {
    return notInParty(memberVoiceState);
}

int APS5_VABI sceNpPartyGetMembers(void* memberList) {
    return notInParty(memberList);
}

int APS5_VABI sceNpPartyGetMembersA(void* memberList) {
    return notInParty(memberList);
}

int APS5_VABI sceNpPartyGetStateAsUser(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpPartyGetStateAsUserA(std::int32_t, std::uint16_t* state) {
    return sceNpPartyGetState(state);
}

int APS5_VABI sceNpPartyGetVoiceChatPriority(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpPartyInitialize(const void* param) {
    if (!param) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) return NP_PARTY_ERROR_ALREADY_INITIALIZED;
    return 0;
}

int APS5_VABI sceNpPartyJoin(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpPartyLeave(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpPartyRegisterHandlerA(const void* handlers, void*) {
    if (!g_initialized) return NP_PARTY_ERROR_NOT_INITIALIZED;
    if (!handlers) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI sceNpPartyRegisterHandler(const void* handlers, void* userdata) {
    return sceNpPartyRegisterHandlerA(handlers, userdata);
}

int APS5_VABI sceNpPartyRegisterPrivateHandler(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpPartySendBinaryMessage(const void* sendParams) {
    return notInParty(sendParams);
}

int APS5_VABI sceNpPartySetVoiceChatPriority(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpPartyShowInvitationList(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpPartyShowInvitationListA(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpPartyTerminate(void) {
    if (!g_initialized.exchange(false)) return NP_PARTY_ERROR_NOT_INITIALIZED;
    return 0;
}

int APS5_VABI sceNpPartyUnregisterPrivateHandler(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
