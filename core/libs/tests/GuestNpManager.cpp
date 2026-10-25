#include "prx/libSceNpManager/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

constexpr int InvalidArgument = static_cast<int>(0x80550003u);
constexpr int SignedOut = static_cast<int>(0x80550006u);
constexpr int InvalidSize = static_cast<int>(0x80550011u);
constexpr int UserNotFound = static_cast<int>(0x80550007u);
constexpr int AlreadyRegistered = static_cast<int>(0x80550008u);
constexpr int NotRegistered = static_cast<int>(0x80550009u);
constexpr int RequestNotFound = static_cast<int>(0x80550014u);

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "NpManager: %s\n", message);
        std::abort();
    }
}

struct StateCall {
    int calls;
    int userId;
    std::uint32_t state;
    NpId* npId;
    void* userdata;
};

constexpr int InitialUserId = 0x10000000;
constexpr std::uint32_t StateSignedOut = 1;
StateCall stateCallsA[9];
StateCall legacyStateCall;

template <int Index>
void APS5_VABI StateCallbackA(int userId, std::uint32_t state, void* userdata) {
    stateCallsA[Index] = {stateCallsA[Index].calls + 1, userId, state, nullptr, userdata};
}

void APS5_VABI LegacyStateCallback(int userId, std::uint32_t state, NpId* npId, void* userdata) {
    legacyStateCall = {legacyStateCall.calls + 1, userId, state, npId, userdata};
}

bool SignedOutOnce(const StateCall& call, void* userdata) {
    return call.calls == 1 && call.userId == InitialUserId && call.state == StateSignedOut &&
        call.npId == nullptr && call.userdata == userdata;
}

void APS5_VABI AbortingGamePresenceCallback(const void*, const void*, void*) {
    std::abort();
}

void APS5_VABI AbortingReachabilityCallback(int, std::uint32_t, void*) {
    std::fprintf(stderr, "NpManager: the reachability callback was called\n");
    std::abort();
}

struct SelfUnregister {
    int id;
    int calls;
    int result;
};

void APS5_VABI SelfUnregisteringCallback(int, std::uint32_t, void* userdata) {
    auto* context = static_cast<SelfUnregister*>(userdata);
    ++context->calls;
    context->result = sceNpUnregisterStateCallbackA(context->id);
}

void CheckStateCallbacksA() {
    constexpr int CallbackMax = static_cast<int>(0x8055001Du);
    void* const callbacks[] = {
        reinterpret_cast<void*>(&StateCallbackA<0>), reinterpret_cast<void*>(&StateCallbackA<1>),
        reinterpret_cast<void*>(&StateCallbackA<2>), reinterpret_cast<void*>(&StateCallbackA<3>),
        reinterpret_cast<void*>(&StateCallbackA<4>), reinterpret_cast<void*>(&StateCallbackA<5>),
        reinterpret_cast<void*>(&StateCallbackA<6>), reinterpret_cast<void*>(&StateCallbackA<7>),
        reinterpret_cast<void*>(&StateCallbackA<8>),
    };
    int userdata[9]{};
    Require(sceNpRegisterStateCallbackA(nullptr, &userdata[0]) == InvalidArgument, "null state callback");
    int ids[8]{};
    ids[0] = sceNpRegisterStateCallbackA(reinterpret_cast<NpStateCallbackA>(callbacks[0]), &userdata[0]);
    Require(ids[0] >= 1 && ids[0] <= 8, "state callback id outside 1..8");
    Require(sceNpRegisterStateCallbackA(reinterpret_cast<NpStateCallbackA>(callbacks[0]), &userdata[1]) == AlreadyRegistered, "duplicate state callback");
    Require(stateCallsA[0].calls == 0, "registering calls the state callback");
    Require(sceNpRegisterNpReachabilityStateCallback(reinterpret_cast<void*>(&AbortingReachabilityCallback), nullptr) == 0,
        "registering the reachability callback");
    sceNpRegisterGamePresenceCallback(nullptr, nullptr);
    sceNpRegisterGamePresenceCallback(reinterpret_cast<void*>(&AbortingGamePresenceCallback), &userdata[0]);

    Require(sceNpCheckCallback() == 0, "sceNpCheckCallback failed");
    Require(SignedOutOnce(stateCallsA[0], &userdata[0]), "sceNpCheckCallback must deliver SIGNED_OUT once");
    Require(sceNpUnregisterNpReachabilityStateCallback() == 0, "unregistering the reachability callback");
    Require(sceNpCheckCallback() == 0, "second sceNpCheckCallback failed");
    Require(stateCallsA[0].calls == 1, "the second sceNpCheckCallback delivered the state again");
    for (int index = 1; index < 8; ++index) {
        ids[index] = sceNpRegisterStateCallbackA(reinterpret_cast<NpStateCallbackA>(callbacks[index]), &userdata[index]);
        Require(ids[index] >= 1 && ids[index] <= 8, "state callback id outside 1..8");
        for (int other = 0; other < index; ++other) {
            Require(ids[index] != ids[other], "two state callbacks got the same id");
        }
    }
    Require(sceNpRegisterStateCallbackA(reinterpret_cast<NpStateCallbackA>(callbacks[8]), &userdata[8]) == CallbackMax, "ninth state callback");
    Require(sceNpCheckCallback() == 0, "sceNpCheckCallback failed with eight callbacks");
    for (int index = 0; index < 8; ++index) {
        Require(SignedOutOnce(stateCallsA[index], &userdata[index]), "each callback gets SIGNED_OUT once");
    }
    Require(stateCallsA[8].calls == 0, "a rejected callback was called");
    Require(sceNpUnregisterStateCallbackA(0) == InvalidArgument, "callback id 0");
    Require(sceNpUnregisterStateCallbackA(9) == InvalidArgument, "callback id 9");
    Require(sceNpUnregisterStateCallbackA(ids[3]) == 0, "unregistering one state callback");
    for (int index = 0; index < 8; ++index) {
        if (index == 3) continue;
        Require(sceNpRegisterStateCallbackA(reinterpret_cast<NpStateCallbackA>(callbacks[index]), &userdata[index]) == AlreadyRegistered,
            "unregistering one id removed another callback");
    }
    Require(sceNpRegisterStateCallbackA(reinterpret_cast<NpStateCallbackA>(callbacks[3]), &userdata[3]) == ids[3], "registering again takes the freed slot");
    Require(sceNpCheckCallbackForLib() == 0, "sceNpCheckCallbackForLib failed after registering again");
    for (int index = 0; index < 8; ++index) {
        Require(stateCallsA[index].calls == (index == 3 ? 2 : 1), "only the registration made again gets SIGNED_OUT");
    }
    Require(sceNpCheckCallbackForLib() == 0 && sceNpCheckCallback() == 0, "checking again failed");
    for (int index = 0; index < 8; ++index) {
        Require(stateCallsA[index].calls == (index == 3 ? 2 : 1), "sceNpCheckCallbackForLib left the state pending");
    }
    for (int index = 0; index < 8; ++index) {
        Require(sceNpUnregisterStateCallbackA(ids[index]) == 0, "unregistering a state callback");
    }
    Require(sceNpUnregisterStateCallbackA(ids[0]) == NotRegistered, "unregistering a state callback twice");
    const int removed = sceNpRegisterStateCallbackA(reinterpret_cast<NpStateCallbackA>(callbacks[8]), &userdata[8]);
    Require(removed >= 1 && removed <= 8, "a freed slot is reused");
    Require(sceNpUnregisterStateCallbackA(removed) == 0, "unregistering before sceNpCheckCallback");
    Require(sceNpCheckCallback() == 0, "sceNpCheckCallback failed with no callbacks");
    Require(stateCallsA[8].calls == 0, "an unregistered callback was called");
    SelfUnregister self{};
    self.id = sceNpRegisterStateCallbackA(reinterpret_cast<NpStateCallbackA>(reinterpret_cast<void*>(&SelfUnregisteringCallback)), &self);
    Require(self.id >= 1 && self.id <= 8, "state callback id outside 1..8");
    Require(sceNpCheckCallback() == 0, "sceNpCheckCallback failed with a callback that unregisters itself");
    Require(self.calls == 1 && self.result == 0, "a state callback must be able to unregister itself");
    Require(sceNpUnregisterStateCallbackA(self.id) == NotRegistered, "the callback did not unregister itself");
}

void CheckLegacyStateCallback() {
    int userdata = 0;
    NpId sentinel{};
    auto* callback = reinterpret_cast<void*>(&LegacyStateCallback);
    Require(sceNpUnregisterStateCallback() == NotRegistered, "unregistering with no legacy callback");
    Require(sceNpRegisterStateCallback(nullptr, &userdata) == InvalidArgument, "null legacy callback");
    Require(sceNpRegisterStateCallback(reinterpret_cast<NpStateCallback>(callback), &userdata) == 0, "registering the legacy callback");
    Require(sceNpRegisterStateCallback(reinterpret_cast<NpStateCallback>(callback), nullptr) == AlreadyRegistered, "duplicate legacy callback");
    legacyStateCall.npId = &sentinel;
    Require(sceNpCheckCallback() == 0, "sceNpCheckCallback failed with the legacy callback");
    Require(SignedOutOnce(legacyStateCall, &userdata), "the legacy callback must get SIGNED_OUT with no NpId");
    Require(sceNpCheckCallback() == 0, "second sceNpCheckCallback failed with the legacy callback");
    Require(legacyStateCall.calls == 1, "the legacy callback got the state again");
    Require(sceNpUnregisterStateCallback() == 0, "unregistering the legacy callback");
    Require(sceNpUnregisterStateCallback() == NotRegistered, "unregistering the legacy callback twice");
}

void CheckRequestTimeouts() {
    Require(sceNpSetTimeout(0x7fffffff, 2, 0, 0, 0, 0) == RequestNotFound, "never created request");
    Require(sceNpSetTimeout(0x7fffffff, 0, 0, 0, 0, 0) == InvalidArgument, "arguments checked before the request");
    const int requestId = sceNpCreateRequest();
    Require(requestId > 0, "sceNpCreateRequest must return a positive request id");
    Require(sceNpSetTimeout(requestId, 2, 1000000, 10000000, 10000000, 10000000) == 0, "minimum timeouts");
    Require(sceNpSetTimeout(requestId, 0, 0, 0, 0, 30000000) == 0, "only the receive timeout");
    Require(sceNpSetTimeout(requestId, 3, 0, 0, 0, 0) == 0, "only the resolve retries");
    Require(sceNpSetTimeout(requestId, 0, 0, 0, 0, 0) == InvalidArgument, "every value left at its default");
    Require(sceNpSetTimeout(0, 2, 0, 0, 0, 0) == InvalidArgument, "request id 0");
    Require(sceNpSetTimeout(-1, 2, 0, 0, 0, 0) == InvalidArgument, "negative request id");
    Require(sceNpSetTimeout(requestId, -1, 0, 0, 0, 30000000) == InvalidArgument, "negative resolve retries");
    Require(sceNpSetTimeout(requestId, 0, 999999, 0, 0, 0) == InvalidArgument, "resolve timeout below 1 s");
    Require(sceNpSetTimeout(requestId, 0, 0, 9999999, 0, 0) == InvalidArgument, "connect timeout below 10 s");
    Require(sceNpSetTimeout(requestId, 0, 0, 0, 9999999, 0) == InvalidArgument, "send timeout below 10 s");
    Require(sceNpSetTimeout(requestId, 0, 0, 0, 0, 1) == InvalidArgument, "receive timeout below 10 s");
    Require(sceNpDeleteRequest(requestId) == 0, "sceNpDeleteRequest failed");
    Require(sceNpSetTimeout(requestId, 2, 0, 0, 0, 0) == RequestNotFound, "deleted request");
    NpCreateAsyncRequestParameter param{};
    const int asyncId = sceNpCreateAsyncRequest(&param);
    Require(asyncId > 0, "sceNpCreateAsyncRequest must return a positive request id");
    Require(sceNpSetTimeout(asyncId, 2, 0, 0, 0, 0) == 0, "async request");
    Require(sceNpDeleteRequest(asyncId) == 0, "sceNpDeleteRequest failed on the async request");
    Require(sceNpSetTimeout(asyncId, 2, 0, 0, 0, 0) == RequestNotFound, "deleted async request");
}

void CheckAsyncRequestLifecycle() {
    NpCreateAsyncRequestParameter param{};
    const int unusedAsyncResult = sceNpCreateAsyncRequest(&param);
    Require(unusedAsyncResult > 0, "sceNpCreateAsyncRequest must return a positive request id");
    const int deletedId = sceNpCreateAsyncRequest(&param);
    Require(sceNpDeleteRequest(deletedId) == 0, "initial delete");

    constexpr int resultSentinel = 0x5a5a5a5a;
    const int unknownIds[] = {0, -1, 0x7fffffff, deletedId};
    for (const int id : unknownIds) {
        int result = resultSentinel;
        Require(sceNpPollAsync(id, &result) == RequestNotFound, "poll must reject an absent or deleted id");
        Require(result == resultSentinel, "poll must not touch the result for an absent or deleted id");
        Require(sceNpPollAsync(id, nullptr) == InvalidArgument,
            "poll must reject a null result before the id");
        Require(sceNpAbortRequest(id) == RequestNotFound, "abort must reject an absent or deleted id");
        Require(sceNpDeleteRequest(id) == RequestNotFound, "delete must reject an absent or deleted id");
    }

    int result = resultSentinel;
    Require(sceNpPollAsync(unusedAsyncResult, &result) == 0, "poll on a live async request");
    Require(result == SignedOut, "poll must report the signed-out result on a live async request");
    Require(sceNpAbortRequest(unusedAsyncResult) == 0, "abort on a live request");
    Require(sceNpDeleteRequest(unusedAsyncResult) == 0, "delete after abort");
    Require(sceNpDeleteRequest(unusedAsyncResult) == RequestNotFound, "second delete");
}

void CheckOnlineIdQueries() {
    NpOnlineId onlineId{};
    std::memcpy(onlineId.data, "player", 6);
    unsigned char output[64];
    std::memset(output, 0x5a, sizeof(output));
    unsigned char untouched[64];
    std::memset(untouched, 0x5a, sizeof(untouched));
    std::int8_t age = 0x5a;
    int userId = 0x5a5a;
    Require(sceNpGetAccountCountry(&onlineId, output) == UserNotFound, "sceNpGetAccountCountry must find no user");
    Require(sceNpGetAccountCountry(nullptr, output) == InvalidArgument, "sceNpGetAccountCountry must reject a null online id");
    Require(sceNpGetAccountCountry(&onlineId, nullptr) == InvalidArgument, "sceNpGetAccountCountry must reject a null output");
    Require(sceNpGetAccountDateOfBirth(&onlineId, output) == UserNotFound, "sceNpGetAccountDateOfBirth must find no user");
    Require(sceNpGetAccountDateOfBirth(nullptr, output) == InvalidArgument, "sceNpGetAccountDateOfBirth must reject a null online id");
    Require(sceNpGetAccountDateOfBirth(&onlineId, nullptr) == InvalidArgument, "sceNpGetAccountDateOfBirth must reject a null output");
    Require(sceNpGetAccountLanguage(1, &onlineId, output) == UserNotFound, "sceNpGetAccountLanguage must find no user");
    Require(sceNpGetAccountLanguage(1, nullptr, output) == InvalidArgument, "sceNpGetAccountLanguage must reject a null online id");
    Require(sceNpGetAccountLanguage(1, &onlineId, nullptr) == InvalidArgument, "sceNpGetAccountLanguage must reject a null output");
    Require(sceNpGetParentalControlInfo(1, &onlineId, &age, output) == UserNotFound, "sceNpGetParentalControlInfo must find no user");
    Require(sceNpGetParentalControlInfo(1, nullptr, &age, output) == InvalidArgument, "sceNpGetParentalControlInfo must reject a null online id");
    Require(sceNpGetParentalControlInfo(1, &onlineId, nullptr, output) == InvalidArgument, "sceNpGetParentalControlInfo must reject a null age");
    Require(sceNpGetParentalControlInfo(1, &onlineId, &age, nullptr) == InvalidArgument, "sceNpGetParentalControlInfo must reject a null info");
    Require(sceNpGetUserIdByOnlineId(&onlineId, &userId) == SignedOut, "sceNpGetUserIdByOnlineId must report NP as signed out");
    Require(sceNpGetUserIdByOnlineId(nullptr, &userId) == InvalidArgument, "sceNpGetUserIdByOnlineId must reject a null online id");
    Require(sceNpGetUserIdByOnlineId(&onlineId, nullptr) == InvalidArgument, "sceNpGetUserIdByOnlineId must reject a null output");
    Require(sceNpCheckPlus(1, output, output + 32) == SignedOut, "sceNpCheckPlus must report NP as signed out");
    Require(sceNpCheckPlus(0, output, output + 32) == InvalidArgument, "sceNpCheckPlus must reject request id 0");
    Require(sceNpCheckPlus(1, nullptr, output + 32) == InvalidArgument, "sceNpCheckPlus must reject a null parameter");
    Require(sceNpCheckPlus(1, output, nullptr) == InvalidArgument, "sceNpCheckPlus must reject a null result");
    std::uint64_t accountId = 0x5a5a;
    Require(sceNpGetAccountId(&onlineId, &accountId) == SignedOut, "sceNpGetAccountId must report NP as signed out");
    Require(sceNpGetAccountId(nullptr, &accountId) == InvalidArgument, "sceNpGetAccountId must reject a null online id");
    Require(sceNpGetAccountId(&onlineId, nullptr) == InvalidArgument, "sceNpGetAccountId must reject a null output");
    const int presenceRequest = sceNpCreateRequest();
    Require(presenceRequest > 0, "sceNpCreateRequest must return a positive request id");
    Require(sceNpSetGamePresenceOnline(presenceRequest, &onlineId, 0) == SignedOut, "sceNpSetGamePresenceOnline must report NP as signed out");
    Require(sceNpSetGamePresenceOnline(0, &onlineId, 0) == InvalidArgument, "sceNpSetGamePresenceOnline must reject request id 0");
    Require(sceNpSetGamePresenceOnline(presenceRequest, nullptr, 0) == InvalidArgument, "sceNpSetGamePresenceOnline must reject a null online id");
    Require(sceNpSetGamePresenceOnline(0x7fffffff, &onlineId, 0) == RequestNotFound, "sceNpSetGamePresenceOnline must reject a request that was never created");
    Require(sceNpDeleteRequest(presenceRequest) == 0, "sceNpDeleteRequest failed");
    Require(sceNpSetGamePresenceOnline(presenceRequest, &onlineId, 0) == RequestNotFound, "sceNpSetGamePresenceOnline must reject a deleted request");
    Require(sceNpRegisterPlusEventCallback(output, nullptr) == 0 && sceNpUnregisterPlusEventCallback() == 0, "the plus event callback must register and unregister");
    Require(sceNpUnregisterPlusEventCallback() == 0, "unregistering the plus event callback again must succeed like the premium one");
    Require(std::memcmp(output, untouched, sizeof(output)) == 0 && age == 0x5a && userId == 0x5a5a && accountId == 0x5a5a, "a failed query wrote its output");
    try {
        sceNpNotifyPlusFeature(output);

        Require(false, "sceNpNotifyPlusFeature must report that it is not implemented");
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()) == "sceNpNotifyPlusFeature not implemented", "sceNpNotifyPlusFeature threw");
    }
}

void CheckUserIdQueries() {
    constexpr int InvalidUserId = -1;
    constexpr int OtherUserId = 0x10000;
    constexpr int NeverCreated = 0x7fffffff;
    unsigned char output[64];
    std::memset(output, 0x5a, sizeof(output));
    unsigned char untouched[64];
    std::memset(untouched, 0x5a, sizeof(untouched));
    std::int8_t age = 0x5a;
    const int request = sceNpCreateRequest();
    Require(request > 0, "sceNpCreateRequest must return a positive request id");
    for (const int userId : {InitialUserId, OtherUserId}) {
        Require(sceNpCheckNpAvailabilityA(request, userId) == SignedOut, "sceNpCheckNpAvailabilityA must report the user as signed out");
        Require(sceNpGetAccountLanguageA(request, userId, output) == SignedOut, "sceNpGetAccountLanguageA must report the user as signed out");
        Require(sceNpGetParentalControlInfoA(request, userId, &age, output) == SignedOut, "sceNpGetParentalControlInfoA must report the user as signed out");
        Require(sceNpGetAccountDateOfBirthA(userId, output) == SignedOut, "sceNpGetAccountDateOfBirthA must report the user as signed out");
    }
    Require(sceNpCheckNpAvailabilityA(request, InvalidUserId) == InvalidArgument, "sceNpCheckNpAvailabilityA must reject the invalid user id");
    Require(sceNpCheckNpAvailabilityA(0, InitialUserId) == InvalidArgument, "sceNpCheckNpAvailabilityA must reject request id 0");
    Require(sceNpCheckNpAvailabilityA(NeverCreated, InitialUserId) == RequestNotFound, "sceNpCheckNpAvailabilityA must reject a request that was never created");
    Require(sceNpCheckNpAvailabilityA(NeverCreated, InvalidUserId) == InvalidArgument, "sceNpCheckNpAvailabilityA must check the user before the request");
    Require(sceNpGetAccountLanguageA(request, InitialUserId, nullptr) == InvalidArgument, "sceNpGetAccountLanguageA must reject a null output");
    Require(sceNpGetAccountLanguageA(request, InvalidUserId, output) == InvalidArgument, "sceNpGetAccountLanguageA must reject the invalid user id");
    Require(sceNpGetAccountLanguageA(0, InitialUserId, output) == InvalidArgument, "sceNpGetAccountLanguageA must reject request id 0");
    Require(sceNpGetAccountLanguageA(NeverCreated, InitialUserId, output) == RequestNotFound, "sceNpGetAccountLanguageA must reject a request that was never created");
    Require(sceNpGetAccountLanguageA(NeverCreated, InitialUserId, nullptr) == InvalidArgument, "sceNpGetAccountLanguageA must check the output before the request");
    Require(sceNpGetParentalControlInfoA(request, InitialUserId, nullptr, output) == InvalidArgument, "sceNpGetParentalControlInfoA must reject a null age");
    Require(sceNpGetParentalControlInfoA(request, InitialUserId, &age, nullptr) == InvalidArgument, "sceNpGetParentalControlInfoA must reject a null info");
    Require(sceNpGetParentalControlInfoA(request, InvalidUserId, &age, output) == InvalidArgument, "sceNpGetParentalControlInfoA must reject the invalid user id");
    Require(sceNpGetParentalControlInfoA(0, InitialUserId, &age, output) == InvalidArgument, "sceNpGetParentalControlInfoA must reject request id 0");
    Require(sceNpGetParentalControlInfoA(NeverCreated, InitialUserId, &age, output) == RequestNotFound, "sceNpGetParentalControlInfoA must reject a request that was never created");
    Require(sceNpGetParentalControlInfoA(NeverCreated, InvalidUserId, &age, output) == InvalidArgument, "sceNpGetParentalControlInfoA must check the user before the request");
    Require(sceNpGetAccountDateOfBirthA(InitialUserId, nullptr) == InvalidArgument, "sceNpGetAccountDateOfBirthA must reject a null output");
    Require(sceNpGetAccountDateOfBirthA(InvalidUserId, output) == InvalidArgument, "sceNpGetAccountDateOfBirthA must reject the invalid user id");
    Require(sceNpDeleteRequest(request) == 0, "sceNpDeleteRequest failed");
    Require(sceNpCheckNpAvailabilityA(request, InitialUserId) == RequestNotFound, "sceNpCheckNpAvailabilityA must reject a deleted request");
    Require(sceNpGetAccountLanguageA(request, InitialUserId, output) == RequestNotFound, "sceNpGetAccountLanguageA must reject a deleted request");
    Require(sceNpGetParentalControlInfoA(request, InitialUserId, &age, output) == RequestNotFound, "sceNpGetParentalControlInfoA must reject a deleted request");
    Require(std::memcmp(output, untouched, sizeof(output)) == 0 && age == 0x5a, "a signed-out account query wrote its output");
    constexpr std::uint32_t Sentinel = 0x5a5a5a5au;
    constexpr std::uint32_t Offline = 0;
    NpOnlineId onlineId{};
    std::memcpy(onlineId.data, "player", 6);
    for (const int userId : {InitialUserId, OtherUserId}) {
        std::uint32_t status = Sentinel;
        Require(sceNpGetGamePresenceStatusA(userId, &status) == 0 && status == Offline, "sceNpGetGamePresenceStatusA must report the user as offline");
    }
    std::uint32_t status = Sentinel;
    Require(sceNpGetGamePresenceStatusA(InvalidUserId, &status) == InvalidArgument, "sceNpGetGamePresenceStatusA must reject the invalid user id");
    Require(sceNpGetGamePresenceStatusA(InitialUserId, nullptr) == InvalidArgument, "sceNpGetGamePresenceStatusA must reject a null status");
    Require(sceNpGetGamePresenceStatus(nullptr, &status) == InvalidArgument, "sceNpGetGamePresenceStatus must reject a null online id");
    Require(sceNpGetGamePresenceStatus(&onlineId, nullptr) == InvalidArgument, "sceNpGetGamePresenceStatus must reject a null status");
    Require(status == Sentinel, "a rejected presence query wrote its status");
    Require(sceNpGetGamePresenceStatus(&onlineId, &status) == 0 && status == Offline, "sceNpGetGamePresenceStatus must report the online id as offline");
}

}

struct ContentRestrictionFields {
    std::uint64_t size;
    std::int8_t defaultAgeRestriction;
    std::uint8_t padding[3];
    std::int32_t ageRestrictionCount;
    const void* ageRestrictions;
};

int SetContentRestriction(const ContentRestrictionFields& fields) {
    return sceNpSetContentRestriction(reinterpret_cast<const NpContentRestriction*>(&fields));
}

void CheckPlusAndContentRestriction() {
    bool isMember = true;
    Require(sceNpIsPlusMember(0x10000, &isMember) == 0 && !isMember, "sceNpIsPlusMember must report no Plus membership");
    Require(sceNpIsPlusMember(0x10000, nullptr) == InvalidArgument, "sceNpIsPlusMember must reject a null output");

    const std::uint8_t ages[8]{};
    ContentRestrictionFields fields{sizeof(ContentRestrictionFields), 12, {}, 2, ages};
    Require(SetContentRestriction(fields) == 0, "sceNpSetContentRestriction must accept a valid restriction");
    fields.ageRestrictionCount = 0;
    fields.ageRestrictions = nullptr;
    Require(SetContentRestriction(fields) == 0, "sceNpSetContentRestriction must accept no age restrictions");
    Require(sceNpSetContentRestriction(nullptr) == InvalidArgument, "sceNpSetContentRestriction must reject null");
    auto bad = fields;
    bad.size = sizeof(ContentRestrictionFields) + 8;
    Require(SetContentRestriction(bad) == InvalidSize, "sceNpSetContentRestriction must check the size");
    bad = fields;
    bad.defaultAgeRestriction = -1;
    Require(SetContentRestriction(bad) == InvalidArgument, "sceNpSetContentRestriction must reject a negative default age");
    bad = fields;
    bad.ageRestrictionCount = 0x101;
    bad.ageRestrictions = ages;
    Require(SetContentRestriction(bad) == InvalidArgument, "sceNpSetContentRestriction must reject more than 256 restrictions");
    bad.ageRestrictionCount = 0x100;
    Require(SetContentRestriction(bad) == 0, "sceNpSetContentRestriction must accept 256 restrictions");
    bad.ageRestrictions = nullptr;
    Require(SetContentRestriction(bad) == InvalidArgument, "sceNpSetContentRestriction must reject restrictions without an array");
}

int main() {
    CheckOnlineIdQueries();
    CheckUserIdQueries();
    CheckStateCallbacksA();
    CheckLegacyStateCallback();
    CheckRequestTimeouts();
    CheckAsyncRequestLifecycle();
    CheckPlusAndContentRestriction();
    NpId npId{};
    std::memset(&npId, 0x5a, sizeof(npId));
    NpId untouched{};
    std::memset(&untouched, 0x5a, sizeof(untouched));
    Require(sceNpGetNpId(0x10000, &npId) == SignedOut, "sceNpGetNpId must report the user as signed out");
    Require(std::memcmp(&npId, &untouched, sizeof(npId)) == 0, "sceNpGetNpId must leave the NpId untouched");
    Require(sceNpGetNpId(0x10000, nullptr) == InvalidArgument, "sceNpGetNpId must reject a null NpId");
    char countryCode[4]{};
    std::memset(countryCode, 0x5a, sizeof(countryCode));
    char untouchedCountryCode[4]{};
    std::memset(untouchedCountryCode, 0x5a, sizeof(untouchedCountryCode));
    Require(sceNpGetAccountCountryA(0x10000, countryCode) == SignedOut, "sceNpGetAccountCountryA must report the user as signed out");
    Require(std::memcmp(countryCode, untouchedCountryCode, sizeof(countryCode)) == 0, "sceNpGetAccountCountryA must leave the country code untouched");
    Require(sceNpGetAccountCountryA(0x10000, nullptr) == InvalidArgument, "sceNpGetAccountCountryA must reject a null country code");
    NpOnlineId onlineId{};
    std::memset(&onlineId, 0x5a, sizeof(onlineId));
    NpOnlineId untouchedOnlineId{};
    std::memset(&untouchedOnlineId, 0x5a, sizeof(untouchedOnlineId));
    Require(sceNpGetOnlineId(0x10000, &onlineId) == SignedOut, "sceNpGetOnlineId must report the user as signed out");
    Require(std::memcmp(&onlineId, &untouchedOnlineId, sizeof(onlineId)) == 0, "sceNpGetOnlineId must leave the online id untouched");
    Require(sceNpGetOnlineId(0x10000, nullptr) == InvalidArgument, "sceNpGetOnlineId must reject a null online id");
    int userId = 0x5a5a;
    Require(sceNpGetUserIdByAccountId(0, &userId) == InvalidArgument, "sceNpGetUserIdByAccountId must reject account id 0");
    Require(sceNpGetUserIdByAccountId(0x1234, nullptr) == InvalidArgument, "sceNpGetUserIdByAccountId must reject a null output");
    Require(sceNpGetUserIdByAccountId(0x1234, &userId) == SignedOut, "sceNpGetUserIdByAccountId must report NP as signed out");
    Require(userId == 0x5a5a, "sceNpGetUserIdByAccountId must leave the user id untouched");
    NpOnlineId buffer{};
    try {
        sceNpUnknown_8weiQyIdI3A(0x10000, &buffer);

        Require(false, "sceNpUnknown_8weiQyIdI3A must report that it is not implemented");
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()) == "sceNpUnknown_8weiQyIdI3A not implemented",
            "sceNpUnknown_8weiQyIdI3A threw");
    }
    try {
        sceNpUnknown_WxLcN76q19o();

        Require(false, "sceNpUnknown_WxLcN76q19o must report that it is not implemented");
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()) == "sceNpUnknown_WxLcN76q19o not implemented",
            "sceNpUnknown_WxLcN76q19o threw");
    }
    return 0;
}
