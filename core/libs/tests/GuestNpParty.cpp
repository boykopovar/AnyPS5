#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
int APS5_VABI sceNpPartyGetState(std::uint16_t* state);
int APS5_VABI sceNpPartyInitialize(const void* param);
int APS5_VABI sceNpPartyTerminate(void);
int APS5_VABI sceNpPartyCheckCallback(void);
int APS5_VABI sceNpPartyRegisterHandlerA(const void* handlers, void* userdata);
int APS5_VABI sceNpPartyGetStateAsUserA(std::int32_t asUserId, std::uint16_t* state);
int APS5_VABI sceNpPartyGetId(void* id);
int APS5_VABI sceNpPartyGetMembersA(void* memberList);
int APS5_VABI sceNpPartyGetMemberInfoA(std::uint16_t memberId, void* memberInfo);
int APS5_VABI sceNpPartyGetMemberSessionInfo(std::uint16_t memberId, void* memberGameSessionInfo);
int APS5_VABI sceNpPartyGetMemberVoiceInfo(std::uint16_t asMemberId, std::uint16_t memberId, std::uint8_t* memberVoiceState);
int APS5_VABI sceNpPartySendBinaryMessage(const void* sendParams);
}

namespace {

constexpr int kAlreadyInitialized = static_cast<int>(0x80552502u);
constexpr int kNotInitialized = static_cast<int>(0x80552503u);
constexpr int kInvalidArgument = static_cast<int>(0x80552504u);
constexpr int kNotInParty = static_cast<int>(0x80552506u);

bool check(bool value, const char* message) {
    if (!value) std::fprintf(stderr, "%s\n", message);
    return value;
}

bool stateTests() {
    std::uint16_t states[3];
    std::memset(states, 0xa5, sizeof(states));
    return check(sceNpPartyGetState(nullptr) == kInvalidArgument, "a null state was not refused with INVALID_ARGUMENT")
        && check(sceNpPartyGetState(&states[1]) == 0 && states[1] == 2, "the state is not NOT_IN_PARTY")
        && check(states[0] == 0xa5a5 && states[2] == 0xa5a5, "sceNpPartyGetState wrote past its 16-bit state")
        && check(sceNpPartyGetStateAsUserA(1, nullptr) == kInvalidArgument, "a null user state was not refused with INVALID_ARGUMENT")
        && check(sceNpPartyGetStateAsUserA(1, &states[0]) == 0 && states[0] == 2, "the user state is not NOT_IN_PARTY")
        && check(states[2] == 0xa5a5, "sceNpPartyGetStateAsUserA wrote past its 16-bit state");
}

bool lifecycleTests() {
    std::uint64_t param[4] = {};
    std::uint64_t handlers[8] = {};
    return check(sceNpPartyTerminate() == kNotInitialized, "terminate before initialize did not fail")
        && check(sceNpPartyCheckCallback() == kNotInitialized, "check callback before initialize did not fail")
        && check(sceNpPartyRegisterHandlerA(handlers, nullptr) == kNotInitialized, "register handler before initialize did not fail")
        && check(sceNpPartyInitialize(nullptr) == kInvalidArgument, "a null initialize param was not refused")
        && check(sceNpPartyInitialize(param) == 0, "initialize failed")
        && check(sceNpPartyInitialize(param) == kAlreadyInitialized, "a second initialize did not fail")
        && check(sceNpPartyRegisterHandlerA(nullptr, nullptr) == kInvalidArgument, "null handlers were not refused")
        && check(sceNpPartyRegisterHandlerA(handlers, param) == 0, "register handler failed")
        && check(sceNpPartyCheckCallback() == 0, "check callback failed")
        && check(sceNpPartyTerminate() == 0, "terminate failed")
        && check(sceNpPartyTerminate() == kNotInitialized, "a second terminate did not fail")
        && check(sceNpPartyInitialize(param) == 0, "initialize after terminate failed")
        && check(sceNpPartyTerminate() == 0, "terminate after reinitialize failed");
}

bool notInPartyTests() {
    std::uint8_t out[64];
    std::memset(out, 0xa5, sizeof(out));
    bool ok = check(sceNpPartyGetId(nullptr) == kInvalidArgument, "a null party id was not refused")
        && check(sceNpPartyGetId(out) == kNotInParty, "get id did not report NOT_IN_PARTY")
        && check(sceNpPartyGetMembersA(nullptr) == kInvalidArgument, "a null member list was not refused")
        && check(sceNpPartyGetMembersA(out) == kNotInParty, "get members did not report NOT_IN_PARTY")
        && check(sceNpPartyGetMemberInfoA(1, nullptr) == kInvalidArgument, "a null member info was not refused")
        && check(sceNpPartyGetMemberInfoA(1, out) == kNotInParty, "get member info did not report NOT_IN_PARTY")
        && check(sceNpPartyGetMemberSessionInfo(1, nullptr) == kInvalidArgument, "a null session info was not refused")
        && check(sceNpPartyGetMemberSessionInfo(1, out) == kNotInParty, "get session info did not report NOT_IN_PARTY")
        && check(sceNpPartyGetMemberVoiceInfo(1, 2, nullptr) == kInvalidArgument, "a null voice state was not refused")
        && check(sceNpPartyGetMemberVoiceInfo(1, 2, out) == kNotInParty, "get voice info did not report NOT_IN_PARTY")
        && check(sceNpPartySendBinaryMessage(nullptr) == kInvalidArgument, "null send params were not refused")
        && check(sceNpPartySendBinaryMessage(out) == kNotInParty, "send did not report NOT_IN_PARTY");
    for (std::uint8_t byte : out) ok = ok && check(byte == 0xa5, "a NOT_IN_PARTY call wrote its output");
    return ok;
}

}

int main() {
    if (!stateTests() || !lifecycleTests() || !notInPartyTests()) return 1;
    std::puts("NpParty tests passed");
    return 0;
}
