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
constexpr int NP_PARTY_ERROR_HANDLER_STATE = static_cast<int>(0x80552511u);
constexpr std::uint16_t NP_PARTY_STATE_NOT_IN_PARTY = 2;

std::atomic<bool> g_initialized{false};
std::atomic<void*> g_handlerCallback{nullptr};
std::atomic<void*> g_privateHandler{nullptr};
std::atomic<bool> g_handlerRegistered{false};

int notInParty(const void* out) {
    if (!g_initialized) return NP_PARTY_ERROR_NOT_INITIALIZED;
    if (!out) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    return NP_PARTY_ERROR_NOT_IN_PARTY;
}

int RecordHandler(const void* handlers, bool rejectRegistered) {
    if (!g_initialized) return NP_PARTY_ERROR_NOT_INITIALIZED;
    if (!handlers) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    if (rejectRegistered && g_handlerRegistered.load()) return NP_PARTY_ERROR_HANDLER_STATE;
    const void* callback = *reinterpret_cast<const void* const*>(static_cast<const unsigned char*>(handlers) + 8);
    g_handlerCallback.store(const_cast<void*>(callback));
    g_handlerRegistered.store(true);
    return 0;
}

}

extern "C" {

int APS5_VABI sceNpPartyGetState(std::uint16_t* state) {
    if (!g_initialized) return NP_PARTY_ERROR_NOT_INITIALIZED;
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

int APS5_VABI sceNpPartyGetStateAsUserA(std::int32_t userId, std::uint16_t* state) {
    if (!g_initialized) return NP_PARTY_ERROR_NOT_INITIALIZED;
    if (userId == -1) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    return sceNpPartyGetState(state);
}

int APS5_VABI sceNpPartyGetVoiceChatPriority(std::uint32_t* priority) {
    if (!g_initialized) return NP_PARTY_ERROR_NOT_INITIALIZED;
    if (!priority) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    return NP_PARTY_ERROR_NOT_IN_PARTY;
}

int APS5_VABI sceNpPartyInitialize(const void* param) {
    if (!param) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) return NP_PARTY_ERROR_ALREADY_INITIALIZED;
    g_handlerCallback.store(nullptr);
    g_privateHandler.store(nullptr);
    g_handlerRegistered.store(false);
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
    return RecordHandler(handlers, true);
}

int APS5_VABI sceNpPartyRegisterHandler(const void* handlers, void*) {
    return RecordHandler(handlers, false);
}

int APS5_VABI sceNpPartyRegisterPrivateHandler(void* handler) {
    if (!g_initialized) return NP_PARTY_ERROR_NOT_INITIALIZED;
    if (!g_handlerCallback.load()) return NP_PARTY_ERROR_HANDLER_STATE;
    g_privateHandler.store(handler);
    return 0;
}

int APS5_VABI sceNpPartySendBinaryMessage(const void* sendParams) {
    return notInParty(sendParams);
}

int APS5_VABI sceNpPartySetVoiceChatPriority(std::int32_t priority) {
    if (!g_initialized) return NP_PARTY_ERROR_NOT_INITIALIZED;
    if (static_cast<std::uint32_t>(priority) - 1u > 1u) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    return NP_PARTY_ERROR_NOT_IN_PARTY;
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
    g_handlerCallback.store(nullptr);
    g_privateHandler.store(nullptr);
    g_handlerRegistered.store(false);
    return 0;
}

int APS5_VABI sceNpPartyUnregisterPrivateHandler(void) {
    if (!g_initialized) return NP_PARTY_ERROR_NOT_INITIALIZED;
    if (!g_privateHandler.load()) return NP_PARTY_ERROR_HANDLER_STATE;
    g_privateHandler.store(nullptr);
    return 0;
}

}
