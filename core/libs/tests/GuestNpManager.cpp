#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sceNpGetNpId(int user_id, NpId* np_id);
int APS5_VABI sceNpGetUserIdByAccountId(std::uint64_t account_id, int* user_id);
int APS5_VABI sceNpUnknown_8weiQyIdI3A(int userId, void* buffer);
int APS5_VABI sceNpUnknown_WxLcN76q19o(void);
}

extern "C" {
int APS5_VABI sceNpCreateRequest(void);
int APS5_VABI sceNpCreateAsyncRequest(const NpCreateAsyncRequestParameter* param);
int APS5_VABI sceNpDeleteRequest(int reqId);
int APS5_VABI sceNpSetTimeout(int reqId, std::int32_t resolveRetry, std::uint32_t resolveTimeout,
    std::uint32_t connTimeout, std::uint32_t sendTimeout, std::uint32_t recvTimeout);
}

extern "C" {
int APS5_VABI sceNpCheckCallback(void);
int APS5_VABI sceNpRegisterStateCallback(void* callback, void* userdata);
int APS5_VABI sceNpUnregisterStateCallback(void);
int APS5_VABI sceNpRegisterStateCallbackA(void* callback, void* userdata);
int APS5_VABI sceNpUnregisterStateCallbackA(int callbackId);
int APS5_VABI sceNpRegisterNpReachabilityStateCallback(void* callback, void* userdata);
int APS5_VABI sceNpUnregisterNpReachabilityStateCallback(void);
}

namespace {

constexpr int InvalidArgument = static_cast<int>(0x80550003u);
constexpr int SignedOut = static_cast<int>(0x80550006u);
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
    ids[0] = sceNpRegisterStateCallbackA(callbacks[0], &userdata[0]);
    Require(ids[0] >= 1 && ids[0] <= 8, "state callback id outside 1..8");
    Require(sceNpRegisterStateCallbackA(callbacks[0], &userdata[1]) == AlreadyRegistered, "duplicate state callback");
    Require(stateCallsA[0].calls == 0, "registering calls the state callback");
    Require(sceNpRegisterNpReachabilityStateCallback(reinterpret_cast<void*>(&AbortingReachabilityCallback), nullptr) == 0,
        "registering the reachability callback");
    Require(sceNpCheckCallback() == 0, "sceNpCheckCallback failed");
    Require(SignedOutOnce(stateCallsA[0], &userdata[0]), "sceNpCheckCallback must deliver SIGNED_OUT once");
    Require(sceNpUnregisterNpReachabilityStateCallback() == 0, "unregistering the reachability callback");
    Require(sceNpCheckCallback() == 0, "second sceNpCheckCallback failed");
    Require(stateCallsA[0].calls == 1, "the second sceNpCheckCallback delivered the state again");
    for (int index = 1; index < 8; ++index) {
        ids[index] = sceNpRegisterStateCallbackA(callbacks[index], &userdata[index]);
        Require(ids[index] >= 1 && ids[index] <= 8, "state callback id outside 1..8");
        for (int other = 0; other < index; ++other) {
            Require(ids[index] != ids[other], "two state callbacks got the same id");
        }
    }
    Require(sceNpRegisterStateCallbackA(callbacks[8], &userdata[8]) == CallbackMax, "ninth state callback");
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
        Require(sceNpRegisterStateCallbackA(callbacks[index], &userdata[index]) == AlreadyRegistered,
            "unregistering one id removed another callback");
    }
    Require(sceNpRegisterStateCallbackA(callbacks[3], &userdata[3]) == ids[3], "registering again takes the freed slot");
    Require(sceNpCheckCallback() == 0, "sceNpCheckCallback failed after registering again");
    for (int index = 0; index < 8; ++index) {
        Require(stateCallsA[index].calls == (index == 3 ? 2 : 1), "only the registration made again gets SIGNED_OUT");
    }
    for (int index = 0; index < 8; ++index) {
        Require(sceNpUnregisterStateCallbackA(ids[index]) == 0, "unregistering a state callback");
    }
    Require(sceNpUnregisterStateCallbackA(ids[0]) == NotRegistered, "unregistering a state callback twice");
    const int removed = sceNpRegisterStateCallbackA(callbacks[8], &userdata[8]);
    Require(removed >= 1 && removed <= 8, "a freed slot is reused");
    Require(sceNpUnregisterStateCallbackA(removed) == 0, "unregistering before sceNpCheckCallback");
    Require(sceNpCheckCallback() == 0, "sceNpCheckCallback failed with no callbacks");
    Require(stateCallsA[8].calls == 0, "an unregistered callback was called");
    SelfUnregister self{};
    self.id = sceNpRegisterStateCallbackA(reinterpret_cast<void*>(&SelfUnregisteringCallback), &self);
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
    Require(sceNpRegisterStateCallback(callback, &userdata) == 0, "registering the legacy callback");
    Require(sceNpRegisterStateCallback(callback, nullptr) == AlreadyRegistered, "duplicate legacy callback");
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

}

int main() {
    CheckStateCallbacksA();
    CheckLegacyStateCallback();
    CheckRequestTimeouts();
    NpId npId{};
    std::memset(&npId, 0x5a, sizeof(npId));
    NpId untouched{};
    std::memset(&untouched, 0x5a, sizeof(untouched));
    Require(sceNpGetNpId(0x10000, &npId) == SignedOut, "sceNpGetNpId must report the user as signed out");
    Require(std::memcmp(&npId, &untouched, sizeof(npId)) == 0, "sceNpGetNpId must leave the NpId untouched");
    Require(sceNpGetNpId(0x10000, nullptr) == InvalidArgument, "sceNpGetNpId must reject a null NpId");
    int userId = 0x5a5a;
    Require(sceNpGetUserIdByAccountId(0, &userId) == InvalidArgument, "sceNpGetUserIdByAccountId must reject account id 0");
    Require(sceNpGetUserIdByAccountId(0x1234, nullptr) == InvalidArgument, "sceNpGetUserIdByAccountId must reject a null output");
    Require(sceNpGetUserIdByAccountId(0x1234, &userId) == SignedOut, "sceNpGetUserIdByAccountId must report NP as signed out");
    Require(userId == 0x5a5a, "sceNpGetUserIdByAccountId must leave the user id untouched");
    unsigned char buffer[0x11]{};
    try {
        sceNpUnknown_8weiQyIdI3A(0x10000, buffer);
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
