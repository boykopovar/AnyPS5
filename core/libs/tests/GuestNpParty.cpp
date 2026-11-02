#include "prx/libSceNpParty/libSceNpParty.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int kAlreadyInitialized = static_cast<int>(0x80552502u);
constexpr int kNotInitialized = static_cast<int>(0x80552503u);
constexpr int kInvalidArgument = static_cast<int>(0x80552504u);
constexpr int kNotInParty = static_cast<int>(0x80552506u);
constexpr int kHandlerState = static_cast<int>(0x80552511u);

bool check(bool value, const char* message) {
    if (!value) std::fprintf(stderr, "%s\n", message);
    return value;
}

bool uninitializedTests() {
    std::uint16_t state = 0xa5a5;
    std::uint8_t out[64];
    std::memset(out, 0xa5, sizeof(out));
    bool ok = check(sceNpPartyGetState(&state) == kNotInitialized && sceNpPartyGetState(nullptr) == kNotInitialized, "get state before initialize did not fail")
        && check(sceNpPartyGetStateAsUserA(1, &state) == kNotInitialized && sceNpPartyGetStateAsUserA(-1, nullptr) == kNotInitialized, "get user state before initialize did not fail")
        && check(state == 0xa5a5, "a state was written before initialize")
        && check(sceNpPartyGetId(out) == kNotInitialized && sceNpPartyGetId(nullptr) == kNotInitialized, "get id before initialize did not fail")
        && check(sceNpPartyGetMembers(out) == kNotInitialized && sceNpPartyGetMembersA(out) == kNotInitialized, "get members before initialize did not fail")
        && check(sceNpPartyGetMemberInfo(1, out) == kNotInitialized && sceNpPartyGetMemberInfoA(1, nullptr) == kNotInitialized, "get member info before initialize did not fail")
        && check(sceNpPartyGetMemberSessionInfo(1, out) == kNotInitialized && sceNpPartyGetMemberVoiceInfo(1, 2, out) == kNotInitialized, "get session or voice info before initialize did not fail")
        && check(sceNpPartySendBinaryMessage(out) == kNotInitialized && sceNpPartySendBinaryMessage(nullptr) == kNotInitialized, "send before initialize did not fail")
        && check(sceNpPartyGetVoiceChatPriority(nullptr) == kNotInitialized, "get priority before initialize did not fail")
        && check(sceNpPartySetVoiceChatPriority(1) == kNotInitialized, "set priority before initialize did not fail")
        && check(sceNpPartyRegisterPrivateHandler(out) == kNotInitialized, "register private handler before initialize did not fail")
        && check(sceNpPartyUnregisterPrivateHandler() == kNotInitialized, "unregister private handler before initialize did not fail");
    for (std::uint8_t byte : out) ok = ok && check(byte == 0xa5, "a call before initialize wrote its output");
    return ok;
}

bool stateTests() {
    std::uint16_t states[3];
    std::memset(states, 0xa5, sizeof(states));
    return check(sceNpPartyGetStateAsUserA(-1, &states[1]) == kInvalidArgument && states[1] == 0xa5a5, "user -1 was not refused with INVALID_ARGUMENT")
        && check(sceNpPartyGetState(nullptr) == kInvalidArgument, "a null state was not refused with INVALID_ARGUMENT")
        && check(sceNpPartyGetState(&states[1]) == 0 && states[1] == 2, "the state is not NOT_IN_PARTY")
        && check(states[0] == 0xa5a5 && states[2] == 0xa5a5, "sceNpPartyGetState wrote past its 16-bit state")
        && check(sceNpPartyGetStateAsUserA(1, nullptr) == kInvalidArgument, "a null user state was not refused with INVALID_ARGUMENT")
        && check(sceNpPartyGetStateAsUserA(1, &states[0]) == 0 && states[0] == 2, "the user state is not NOT_IN_PARTY")
        && check(states[2] == 0xa5a5, "sceNpPartyGetStateAsUserA wrote past its 16-bit state");
}

bool lifecycleTests() {
    std::uint64_t param[4] = {};
    std::uint64_t handlers[8] = {};
    handlers[1] = 1;
    return check(sceNpPartyTerminate() == kNotInitialized, "terminate before initialize did not fail")
        && check(sceNpPartyCheckCallback() == kNotInitialized, "check callback before initialize did not fail")
        && check(sceNpPartyRegisterHandlerA(handlers, nullptr) == kNotInitialized, "register handler before initialize did not fail")
        && check(sceNpPartyRegisterHandler(handlers, nullptr) == kNotInitialized, "the plain register handler before initialize did not fail")
        && check(sceNpPartyInitialize(nullptr) == kInvalidArgument, "a null initialize param was not refused")
        && check(sceNpPartyInitialize(param) == 0, "initialize failed")
        && check(sceNpPartyInitialize(param) == kAlreadyInitialized, "a second initialize did not fail")
        && check(sceNpPartyRegisterHandlerA(nullptr, nullptr) == kInvalidArgument, "null handlers were not refused")
        && check(sceNpPartyRegisterPrivateHandler(handlers) == kHandlerState, "a private handler without a public handler was accepted")
        && check(sceNpPartyUnregisterPrivateHandler() == kHandlerState, "unregister without a public handler did not fail")
        && check(sceNpPartyRegisterHandlerA(handlers, param) == 0, "register handler failed")
        && check(sceNpPartyRegisterHandler(nullptr, nullptr) == kInvalidArgument, "null plain handlers were not refused")
        && check(sceNpPartyRegisterHandler(handlers, param) == 0, "the plain register handler failed")
        && check(sceNpPartyRegisterHandlerA(handlers, param) == kHandlerState, "a second A registration did not fail")
        && check(sceNpPartyRegisterPrivateHandler(handlers) == 0, "register private handler failed")
        && check(sceNpPartyUnregisterPrivateHandler() == 0, "unregister private handler failed")
        && check(sceNpPartyUnregisterPrivateHandler() == kHandlerState, "a second unregister did not fail")
        && check(sceNpPartyRegisterPrivateHandler(nullptr) == 0, "a null private handler was not accepted")
        && check(sceNpPartyUnregisterPrivateHandler() == kHandlerState, "a null private handler was not treated as absent")
        && check(sceNpPartyCheckCallback() == 0, "check callback failed")
        && check(sceNpPartyTerminate() == 0, "terminate failed")
        && check(sceNpPartyTerminate() == kNotInitialized, "a second terminate did not fail")
        && check(sceNpPartyInitialize(param) == 0, "initialize after terminate failed")
        && check(sceNpPartyTerminate() == 0, "terminate after reinitialize failed");
}

bool priorityTests() {
    std::uint32_t priority = 0xa5a5a5a5;
    return check(sceNpPartyGetVoiceChatPriority(nullptr) == kInvalidArgument, "a null priority was not refused")
        && check(sceNpPartyGetVoiceChatPriority(&priority) == kNotInParty && priority == 0xa5a5a5a5, "get priority did not report NOT_IN_PARTY without writing")
        && check(sceNpPartySetVoiceChatPriority(0) == kInvalidArgument, "priority 0 was not refused")
        && check(sceNpPartySetVoiceChatPriority(3) == kInvalidArgument, "priority 3 was not refused")
        && check(sceNpPartySetVoiceChatPriority(-1) == kInvalidArgument, "priority -1 was not refused")
        && check(sceNpPartySetVoiceChatPriority(1) == kNotInParty, "priority 1 did not report NOT_IN_PARTY")
        && check(sceNpPartySetVoiceChatPriority(2) == kNotInParty, "priority 2 did not report NOT_IN_PARTY");
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
        && check(sceNpPartyGetMembers(nullptr) == kInvalidArgument, "a null plain member list was not refused")
        && check(sceNpPartyGetMembers(out) == kNotInParty, "the plain get members did not report NOT_IN_PARTY")
        && check(sceNpPartyGetMemberInfo(1, nullptr) == kInvalidArgument, "a null plain member info was not refused")
        && check(sceNpPartyGetMemberInfo(1, out) == kNotInParty, "the plain get member info did not report NOT_IN_PARTY")
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
    std::uint64_t param[4] = {};
    if (!uninitializedTests() || !lifecycleTests()) return 1;
    if (!check(sceNpPartyInitialize(param) == 0, "initialize for the state checks failed")) return 1;
    if (!stateTests() || !priorityTests() || !notInPartyTests()) return 1;
    if (!check(sceNpPartyTerminate() == 0, "terminate after the state checks failed") || !uninitializedTests()) return 1;
    std::puts("NpParty tests passed");
    return 0;
}
