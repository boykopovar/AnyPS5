#include "prx/libSceNpParty/libSceNpParty.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

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
        && check(sceNpPartyUnregisterPrivateHandler() == kNotInitialized, "unregister private handler before initialize did not fail")
        && check(sceNpPartyLeave(1, nullptr) == kNotInitialized, "leave before initialize did not fail");
    for (std::uint8_t byte : out) ok = ok && check(byte == 0xa5, "a call before initialize wrote its output");
    return ok;
}

bool stateTests() {
    std::uint16_t states[3];
    std::memset(states, 0xa5, sizeof(states));
    errno = 71;
    const bool unmappedOk = check(sceNpPartyGetState(reinterpret_cast<std::uint16_t*>(0x10)) == kInvalidArgument, "unmapped state was not refused")
        && check(errno == 71, "errno was not preserved");
    return unmappedOk
        && check(sceNpPartyGetStateAsUserA(-1, &states[1]) == kInvalidArgument && states[1] == 0xa5a5, "user -1 was not refused with INVALID_ARGUMENT")
        && check(sceNpPartyGetState(nullptr) == kInvalidArgument, "a null state was not refused with INVALID_ARGUMENT")
        && check(sceNpPartyGetState(reinterpret_cast<std::uint16_t*>(0x10)) == kInvalidArgument, "an unmapped state was not refused")
        && check(errno == 71, "errno mutated on unmapped state refusal")
        && check(sceNpPartyGetState(&states[1]) == 0 && states[1] == 2, "the state is not NOT_IN_PARTY")
        && check(states[0] == 0xa5a5 && states[2] == 0xa5a5, "sceNpPartyGetState wrote past its 16-bit state")
        && check(sceNpPartyGetStateAsUserA(1, nullptr) == kInvalidArgument, "a null user state was not refused with INVALID_ARGUMENT")
        && check(sceNpPartyGetStateAsUserA(1, &states[0]) == 0 && states[0] == 2, "the user state is not NOT_IN_PARTY")
        && check(sceNpPartyGetStateAsUserA(0, &states[0]) == 0 && states[0] == 2, "user 0 was refused")
        && check(sceNpPartyGetStateAsUserA(100, &states[0]) == 0 && states[0] == 2, "user 100 was refused")
        && check(states[2] == 0xa5a5, "sceNpPartyGetStateAsUserA wrote past its 16-bit state");
}

bool lifecycleTests() {
    std::uint64_t param[4] = {};
    std::uint64_t zeroParam[4] = {};
    param[0] = 0x6000000;
    std::uint64_t handlers[8] = {};
    handlers[1] = 1;
    std::uint64_t nullCallbackHandlers[8] = {};
    return check(sceNpPartyTerminate() == kNotInitialized, "terminate before initialize did not fail")
        && check(sceNpPartyCheckCallback() == kNotInitialized, "check callback before initialize did not fail")
        && check(sceNpPartyRegisterHandlerA(handlers, nullptr) == kNotInitialized, "register handler before initialize did not fail")
        && check(sceNpPartyRegisterHandler(handlers, nullptr) == kNotInitialized, "the plain register handler before initialize did not fail")
        && check(sceNpPartyInitialize(nullptr) == kInvalidArgument, "a null initialize param was not refused")
        && check((errno = 71, sceNpPartyInitialize(reinterpret_cast<const void*>(0x10)) == kInvalidArgument), "an unmapped initialize param was not refused")
        && check(errno == 71, "errno was not preserved on unmapped initialize param")
        && check(sceNpPartyInitialize(zeroParam) == kInvalidArgument, "a zero-version initialize param was not refused")
        && check(sceNpPartyInitialize(param) == 0, "initialize failed")
        && check(sceNpPartyInitialize(param) == kAlreadyInitialized, "a second initialize did not fail")
        && check(sceNpPartyRegisterHandlerA(nullptr, nullptr) == kInvalidArgument, "null handlers were not refused")
        && check(sceNpPartyRegisterPrivateHandler(handlers) == kHandlerState, "a private handler without a public handler was accepted")
        && check(sceNpPartyUnregisterPrivateHandler() == kHandlerState, "unregister without a public handler did not fail")
        && check(sceNpPartyRegisterHandlerA(handlers, param) == 0, "register handler failed")
        && check(sceNpPartyRegisterHandler(nullptr, nullptr) == kInvalidArgument, "null plain handlers were not refused")
        && check(sceNpPartyRegisterHandler(handlers, param) == 0, "the plain register handler failed")
        && check(sceNpPartyRegisterHandler(nullCallbackHandlers, nullptr) == 0, "plain register with null callback failed")
        && check(sceNpPartyRegisterPrivateHandler(handlers) == kHandlerState, "private handler succeeded when callback was null")
        && check(sceNpPartyRegisterHandler(handlers, param) == 0, "restoring valid handler callback failed")
        && check(sceNpPartyRegisterHandlerA(handlers, param) == kHandlerState, "a second A registration did not fail")
        && check(sceNpPartyRegisterPrivateHandler(handlers) == 0, "register private handler failed")
        && check(sceNpPartyUnregisterPrivateHandler() == 0, "unregister private handler failed")
        && check(sceNpPartyUnregisterPrivateHandler() == kHandlerState, "a second unregister did not fail")
        && check(sceNpPartyRegisterPrivateHandler(nullptr) == 0, "a null private handler was not accepted")
        && check(sceNpPartyUnregisterPrivateHandler() == kHandlerState, "a null private handler was not treated as absent")
        && check(sceNpPartyRegisterPrivateHandler(handlers) == 0, "a non-null private handler was not accepted")
        && check(sceNpPartyUnregisterPrivateHandler() == 0, "unregister of a non-null private handler failed")
        && check(sceNpPartyCheckCallback() == 0, "check callback failed")
        && check(sceNpPartyRegisterPrivateHandler(handlers) == 0, "private handler before terminate failed")
        && check(sceNpPartyTerminate() == 0, "terminate failed")
        && check(sceNpPartyTerminate() == kNotInitialized, "a second terminate did not fail")
        && check(sceNpPartyInitialize(param) == 0, "initialize after terminate failed")
        && check(sceNpPartyRegisterPrivateHandler(handlers) == kHandlerState, "a private handler survived reinitialize")
        && check(sceNpPartyUnregisterPrivateHandler() == kHandlerState, "private handler state survived reinitialize")
        && check(sceNpPartyRegisterHandlerA(handlers, param) == 0, "a fresh A registration after reinitialize failed")
        && check(sceNpPartyRegisterPrivateHandler(handlers) == 0, "private handler after a fresh registration failed")
        && check(sceNpPartyTerminate() == 0, "terminate after reinitialize failed");
}

bool handlerContentionTests() {
    std::uint64_t param[4] = {};
    param[0] = 0x6000000;
    std::uint64_t handlers[8] = {};
    handlers[1] = 1;
    bool ok = true;
    for (int round = 0; round < 16; ++round) {
        ok = check(sceNpPartyInitialize(param) == 0, "contention initialize failed") && ok;
        std::atomic<bool> go{false};
        std::atomic<int> succeeded{0};
        std::atomic<int> rejected{0};
        std::vector<std::thread> threads;
        for (int i = 0; i < 8; ++i) {
            threads.emplace_back([&] {
                while (!go.load()) std::this_thread::yield();
                const int result = sceNpPartyRegisterHandlerA(handlers, nullptr);
                if (result == 0) ++succeeded;
                if (result == kHandlerState) ++rejected;
            });
        }
        go.store(true);
        for (auto& thread : threads) thread.join();
        ok = check(succeeded.load() == 1 && rejected.load() == 7, "only one concurrent A registration must succeed") && ok;
        ok = check(sceNpPartyRegisterPrivateHandler(handlers) == 0, "private handler before unregister contention failed") && ok;
        succeeded.store(0);
        rejected.store(0);
        go.store(false);
        threads.clear();
        for (int i = 0; i < 8; ++i) {
            threads.emplace_back([&] {
                while (!go.load()) std::this_thread::yield();
                const int result = sceNpPartyUnregisterPrivateHandler();
                if (result == 0) ++succeeded;
                if (result == kHandlerState) ++rejected;
            });
        }
        go.store(true);
        for (auto& thread : threads) thread.join();
        ok = check(succeeded.load() == 1 && rejected.load() == 7, "only one concurrent private unregister must succeed") && ok;
        ok = check(sceNpPartyTerminate() == 0, "contention terminate failed") && ok;
    }
    return ok;
}

bool priorityTests() {
    std::uint32_t priority = 0xa5a5a5a5;
    return check(sceNpPartyGetVoiceChatPriority(nullptr) == kInvalidArgument, "a null priority was not refused")
        && check(sceNpPartyGetVoiceChatPriority(&priority) == kNotInParty && priority == 0xa5a5a5a5, "get priority did not report NOT_IN_PARTY without writing")
        && check(sceNpPartySetVoiceChatPriority(0) == kInvalidArgument, "priority 0 was not refused")
        && check(sceNpPartySetVoiceChatPriority(3) == kInvalidArgument, "priority 3 was not refused")
        && check(sceNpPartySetVoiceChatPriority(4) == kInvalidArgument, "priority 4 was not refused")
        && check(sceNpPartySetVoiceChatPriority(-1) == kInvalidArgument, "priority -1 was not refused")
        && check(sceNpPartySetVoiceChatPriority(-2) == kInvalidArgument, "priority -2 was not refused")
        && check(sceNpPartySetVoiceChatPriority(-2147483647 - 1) == kInvalidArgument, "INT32_MIN priority was not refused")
        && check(sceNpPartySetVoiceChatPriority(2147483647) == kInvalidArgument, "INT32_MAX priority was not refused")
        && check(sceNpPartySetVoiceChatPriority(1) == kNotInParty, "priority 1 did not report NOT_IN_PARTY")
        && check(sceNpPartySetVoiceChatPriority(2) == kNotInParty, "priority 2 did not report NOT_IN_PARTY");
}

template <typename T, typename U>
void SetField(T& param, std::size_t offset, const U& value) {
    std::memcpy(param.data + offset, &value, sizeof(value));
}

bool offlinePartyTests() {
    std::uint8_t users20[2][0x14] = {{1}, {2}};
    std::uint8_t users8[2][8] = {{1}, {2}};
    std::uint8_t payload[8] = {1};
    const std::uint64_t array20 = reinterpret_cast<std::uint64_t>(users20);
    const std::uint64_t array8 = reinterpret_cast<std::uint64_t>(users8);
    const std::uint64_t data = reinterpret_cast<std::uint64_t>(payload);
    bool ok = true;

    NpPartyCreateParam create{};
    ok = check(sceNpPartyCreate(-1, &create) == kInvalidArgument, "create refused user -1") && ok;
    ok = check(sceNpPartyCreate(1, &create) == kNotInParty, "create did not report NOT_IN_PARTY") && ok;
    create.data[0x2DA] = 1;
    ok = check(sceNpPartyCreate(1, &create) == kInvalidArgument, "create accepted a nonzero reserved tail") && ok;
    create = {};
    create.data[0x2D8] = 0x01;
    create.data[0x2D9] = 0x08;
    ok = check(sceNpPartyCreate(1, &create) == kInvalidArgument, "create accepted an oversized blob") && ok;
    create = {};
    create.data[0x2C8] = 17;
    ok = check(sceNpPartyCreate(1, &create) == kInvalidArgument, "create accepted too many users") && ok;
    create = {};
    create.data[0x2C8] = 1;
    ok = check(sceNpPartyCreate(1, &create) == kInvalidArgument, "create accepted a null user array") && ok;
    create = {};
    create.data[0x2C8] = 2;
    SetField(create, 0x2C0, array20);
    SetField(create, 0x2D0, data);
    ok = check(sceNpPartyCreate(1, &create) == kNotInParty, "create with users did not report NOT_IN_PARTY") && ok;
    users20[1][0] = 1;
    ok = check(sceNpPartyCreate(1, &create) == kInvalidArgument, "create accepted duplicate users") && ok;
    users20[1][0] = 2;

    NpPartyCreateAParam createA{};
    ok = check(sceNpPartyCreateA(-1, &createA) == kInvalidArgument, "create A refused user -1") && ok;
    ok = check(sceNpPartyCreateA(1, &createA) == kNotInParty, "create A did not report NOT_IN_PARTY") && ok;
    createA.data[0xC9] = 1;
    ok = check(sceNpPartyCreateA(1, &createA) == kInvalidArgument, "create A accepted the legacy flag") && ok;
    createA = {};
    createA.data[0xC8] = 17;
    ok = check(sceNpPartyCreateA(1, &createA) == kInvalidArgument, "create A accepted too many users") && ok;
    createA = {};
    createA.data[0xC8] = 2;
    SetField(createA, 0xC0, array8);
    ok = check(sceNpPartyCreateA(1, &createA) == kNotInParty, "create A with users did not report NOT_IN_PARTY") && ok;
    users8[1][0] = 1;
    ok = check(sceNpPartyCreateA(1, &createA) == kInvalidArgument, "create A accepted duplicate users") && ok;
    users8[1][0] = 2;

    NpPartyJoinParam join{};
    ok = check(sceNpPartyJoin(-1, join) == kInvalidArgument, "join refused user -1") && ok;
    ok = check(sceNpPartyJoin(1, join) == kInvalidArgument, "join accepted an empty parameter") && ok;
    join.data[0] = 1;
    ok = check(sceNpPartyJoin(1, join) == kNotInParty, "join did not report NOT_IN_PARTY") && ok;

    NpPartyLeaveParam leave{};
    ok = check(sceNpPartyLeave(0, nullptr) == kInvalidArgument, "leave refused user 0") && ok;
    ok = check(sceNpPartyLeave(1, nullptr) == kNotInParty, "leave without a parameter did not report NOT_IN_PARTY") && ok;
    leave.size = 0x801;
    ok = check(sceNpPartyLeave(1, &leave) == kInvalidArgument, "leave accepted an oversized blob") && ok;
    leave.size = 8;
    ok = check(sceNpPartyLeave(1, &leave) == kNotInParty, "leave with a null blob did not report NOT_IN_PARTY") && ok;
    leave.data = payload;
    ok = check(sceNpPartyLeave(1, &leave) == kNotInParty, "leave with a blob did not report NOT_IN_PARTY") && ok;

    std::uint16_t state = 0xa5a5;
    ok = check(sceNpPartyGetStateAsUser(0, &state) == kInvalidArgument, "get user state accepted user 0") && ok;
    ok = check(sceNpPartyGetStateAsUser(1, nullptr) == kInvalidArgument, "get user state accepted a null output") && ok;
    ok = check(sceNpPartyGetStateAsUser(1, &state) == 0 && state == 2, "the plain user state is not NOT_IN_PARTY") && ok;

    NpPartyShowInvitationListParam list{};
    ok = check(sceNpPartyShowInvitationList(0, &list) == kInvalidArgument, "show invitations accepted user 0") && ok;
    ok = check(sceNpPartyShowInvitationList(1, &list) == kNotInParty, "show invitations did not report NOT_IN_PARTY") && ok;
    list.data[0x20A] = 1;
    ok = check(sceNpPartyShowInvitationList(1, &list) == kInvalidArgument, "show invitations accepted a nonzero reserved byte") && ok;
    list = {};
    list.data[0x208] = 17;
    ok = check(sceNpPartyShowInvitationList(1, &list) == kInvalidArgument, "show invitations accepted too many users") && ok;
    list = {};
    list.data[0x208] = 1;
    ok = check(sceNpPartyShowInvitationList(1, &list) == kInvalidArgument, "show invitations accepted a null user array") && ok;
    list = {};
    list.data[0x208] = 2;
    SetField(list, 0x200, array20);
    ok = check(sceNpPartyShowInvitationList(1, &list) == kNotInParty, "show invitations with users did not report NOT_IN_PARTY") && ok;
    users20[1][0] = 1;
    ok = check(sceNpPartyShowInvitationList(1, &list) == kInvalidArgument, "show invitations accepted duplicate users") && ok;
    users20[1][0] = 2;
    list = {};
    list.data[0x218] = 0x01;
    list.data[0x219] = 0x08;
    ok = check(sceNpPartyShowInvitationList(1, &list) == kInvalidArgument, "show invitations accepted an oversized blob") && ok;

    NpPartyShowInvitationListAParam listA{};
    ok = check(sceNpPartyShowInvitationListA(0, &listA) == kInvalidArgument, "show invitations A accepted user 0") && ok;
    ok = check(sceNpPartyShowInvitationListA(1, &listA) == kNotInParty, "show invitations A did not report NOT_IN_PARTY") && ok;
    listA.data[0x9] = 1;
    ok = check(sceNpPartyShowInvitationListA(1, &listA) == kInvalidArgument, "show invitations A accepted the legacy flag") && ok;
    listA = {};
    listA.data[0x8] = 17;
    ok = check(sceNpPartyShowInvitationListA(1, &listA) == kInvalidArgument, "show invitations A accepted too many users") && ok;
    listA = {};
    listA.data[0x8] = 2;
    SetField(listA, 0x0, array8);
    ok = check(sceNpPartyShowInvitationListA(1, &listA) == kNotInParty, "show invitations A with users did not report NOT_IN_PARTY") && ok;
    users8[1][0] = 1;
    ok = check(sceNpPartyShowInvitationListA(1, &listA) == kInvalidArgument, "show invitations A accepted duplicate users") && ok;
    users8[1][0] = 2;
    return ok;
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
        && check(sceNpPartySendBinaryMessage(out) == kNotInParty, "send did not report NOT_IN_PARTY")
        && check(sceNpPartyLeave(1, nullptr) == kNotInParty, "leave did not report NOT_IN_PARTY");
    for (std::uint8_t byte : out) ok = ok && check(byte == 0xa5, "a NOT_IN_PARTY call wrote its output");
    return ok;
}

}

int main() {
    std::uint64_t param[4] = {};
    param[0] = 0x6000000;
    if (!uninitializedTests() || !lifecycleTests() || !handlerContentionTests()) return 1;
    if (!check(sceNpPartyInitialize(param) == 0, "initialize for the state checks failed")) return 1;
    if (!stateTests() || !priorityTests() || !offlinePartyTests() || !notInPartyTests()) return 1;
    if (!check(sceNpPartyTerminate() == 0, "terminate after the state checks failed") || !uninitializedTests()) return 1;
    std::puts("NpParty tests passed");
    return 0;
}
