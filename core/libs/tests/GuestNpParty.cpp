#include "prx/libSceNpParty/include/Api.hpp"
#include "GuestNpParty.hpp"

#include <cstdint>
#include <iostream>

namespace {

constexpr int kNotInitialized = static_cast<int>(0x80552503);
constexpr int kAlreadyInitialized = static_cast<int>(0x80552502);
constexpr int kNotInParty = static_cast<int>(0x80552506);
constexpr int kInvalidArgument = static_cast<int>(0x80552504);
constexpr int kNotRegistered = static_cast<int>(0x80552511);

bool check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        return false;
    }
    return true;
}

bool uninitializedTests() {
    std::uint8_t out[128];
    std::fill(std::begin(out), std::end(out), 0xa5);
    bool ok = check(sceNpPartyGetState(out) == kNotInitialized && sceNpPartyGetState(nullptr) == kNotInitialized, "get state before initialize did not fail")
        && check(sceNpPartyGetStateAsUserA(-1, out) == kNotInitialized && sceNpPartyGetStateAsUserA(1, out) == kNotInitialized, "get state as user before initialize did not fail")
        && check(sceNpPartyGetId(out) == kNotInitialized && sceNpPartyGetId(nullptr) == kNotInitialized, "get id before initialize did not fail")
        && check(sceNpPartyGetMembers(out) == kNotInitialized && sceNpPartyGetMembersA(out) == kNotInitialized, "get members before initialize did not fail")
        && check(sceNpPartyGetMemberInfo(1, out) == kNotInitialized && sceNpPartyGetMemberInfoA(1, nullptr) == kNotInitialized, "get member info before initialize did not fail")
        && check(sceNpPartyGetMemberSessionInfo(1, out) == kNotInitialized && sceNpPartyGetMemberVoiceInfo(1, 2, out) == kNotInitialized, "get session or voice info before initialize did not fail")
        && check(sceNpPartySendBinaryMessage(out) == kNotInitialized && sceNpPartySendBinaryMessage(nullptr) == kNotInitialized, "send before initialize did not fail")
        && check(sceNpPartyGetVoiceChatPriority(nullptr) == kNotInitialized, "get priority before initialize did not fail")
        && check(sceNpPartySetVoiceChatPriority(1) == kNotInitialized, "set priority before initialize did not fail")
        && check(sceNpPartyRegisterPrivateHandler(out) == kNotInitialized, "register private handler before initialize did not fail")
        && check(sceNpPartyUnregisterPrivateHandler() == kNotInitialized, "unregister private handler before initialize did not fail")
        && check(sceNpPartyLeave() == kNotInitialized, "leave before initialize did not fail");
    for (std::uint8_t byte : out) ok = ok && check(byte == 0xa5, "a call before initialize wrote its output");
    return ok;
}

bool argumentValidationTests() {
    return check(sceNpPartyGetState(nullptr) == kInvalidArgument, "get state with null out did not return invalid argument")
        && check(sceNpPartyGetStateAsUserA(-1, nullptr) == kInvalidArgument, "get state as user with invalid user did not return invalid argument")
        && check(sceNpPartyGetId(nullptr) == kInvalidArgument, "get id with null out did not return invalid argument")
        && check(sceNpPartyGetMembers(nullptr) == kInvalidArgument && sceNpPartyGetMembersA(nullptr) == kInvalidArgument, "get members with null out did not return invalid argument")
        && check(sceNpPartyGetMemberInfo(1, nullptr) == kInvalidArgument && sceNpPartyGetMemberInfoA(1, nullptr) == kInvalidArgument, "get member info with null out did not return invalid argument")
        && check(sceNpPartyGetMemberSessionInfo(1, nullptr) == kInvalidArgument && sceNpPartyGetMemberVoiceInfo(1, 2, nullptr) == kInvalidArgument, "get session or voice info with null out did not return invalid argument")
        && check(sceNpPartySendBinaryMessage(nullptr) == kInvalidArgument, "send binary message with null data did not return invalid argument")
        && check(sceNpPartyGetVoiceChatPriority(nullptr) == kInvalidArgument, "get priority with null out did not return invalid argument")
        && check(sceNpPartySetVoiceChatPriority(0) == kInvalidArgument && sceNpPartySetVoiceChatPriority(3) == kInvalidArgument, "set priority with invalid level did not return invalid argument");
}

bool handlerRegistrationTests() {
    std::uintptr_t dummyHandler[3]{0, 0, 0x1234};
    return check(sceNpPartyRegisterPrivateHandler(nullptr) == kNotRegistered, "register private handler without public handler did not return not registered")
        && check(sceNpPartyUnregisterPrivateHandler() == kNotRegistered, "unregister private handler when none registered did not return not registered")
        && check(sceNpPartyRegisterHandlerA(dummyHandler) == 0, "register handler A failed")
        && check(sceNpPartyRegisterHandlerA(dummyHandler) == kNotRegistered, "second register handler A did not return already registered")
        && check(sceNpPartyRegisterPrivateHandler(nullptr) == 0, "register private null handler failed")
        && check(sceNpPartyUnregisterPrivateHandler() == kNotRegistered, "unregister private null handler did not return not registered")
        && check(sceNpPartyRegisterPrivateHandler(dummyHandler) == 0, "register private handler failed")
        && check(sceNpPartyUnregisterPrivateHandler() == 0, "unregister private handler failed")
        && check(sceNpPartyUnregisterPrivateHandler() == kNotRegistered, "second unregister private handler did not return not registered")
        && check(sceNpPartyRegisterHandler(dummyHandler) == 0, "register handler replacement failed");
}

bool notInPartyTests() {
    std::uint8_t out[128];
    std::fill(std::begin(out), std::end(out), 0x5a);
    bool ok = check(sceNpPartyGetId(out) == kNotInParty, "get id did not return not in party")
        && check(sceNpPartyGetMembers(out) == kNotInParty && sceNpPartyGetMembersA(out) == kNotInParty, "get members did not return not in party")
        && check(sceNpPartyGetMemberInfo(1, out) == kNotInParty && sceNpPartyGetMemberInfoA(1, out) == kNotInParty, "get member info did not return not in party")
        && check(sceNpPartyGetMemberSessionInfo(1, out) == kNotInParty && sceNpPartyGetMemberVoiceInfo(1, 2, out) == kNotInParty, "get session or voice info did not return not in party")
        && check(sceNpPartySendBinaryMessage(out) == kNotInParty, "send binary message did not return not in party")
        && check(sceNpPartyGetVoiceChatPriority(out) == kNotInParty, "get priority did not return not in party")
        && check(sceNpPartySetVoiceChatPriority(1) == kNotInParty && sceNpPartySetVoiceChatPriority(2) == kNotInParty, "set priority did not return not in party")
        && check(sceNpPartyLeave() == kNotInParty, "leave did not return not in party");
    for (std::uint8_t byte : out) ok = ok && check(byte == 0x5a, "a call returning not in party wrote its output");
    return ok;
}

}

int main() {
    if (!uninitializedTests()) return 1;
    if (!check(sceNpPartyInitialize(nullptr) == 0, "initialize failed")) return 2;
    if (!check(sceNpPartyInitialize(nullptr) == kAlreadyInitialized, "second initialize did not report already initialized")) return 3;
    if (!argumentValidationTests()) return 4;
    if (!handlerRegistrationTests()) return 5;
    if (!notInPartyTests()) return 6;
    if (!check(sceNpPartyTerminate() == 0, "terminate failed")) return 7;
    if (!uninitializedTests()) return 8;
    return 0;
}
