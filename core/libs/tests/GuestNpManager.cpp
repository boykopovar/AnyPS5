#include "prx/libSceNpCommon/include/Api.hpp"
#include "prx/libSceNpManager/include/Api.hpp"
#include "prx/libSceNpManager/include/AdditionalExports.hpp"
#include "prx/libSceNpManager/include/InGameMessageIds.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
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
constexpr int InvalidId = static_cast<int>(0x80550015u);
constexpr int IdMapperRefusedArgument = static_cast<int>(0x80553083u);
constexpr int IdMapperRefusedRequestId = static_cast<int>(0x80553085u);
constexpr int ManagerIntTableFull = static_cast<int>(0x80559D05u);
constexpr int ManagerIntNotInTable = static_cast<int>(0x80559D0Au);
constexpr int InGameMessageInvalidArgument = static_cast<int>(0x80552B02u);
constexpr int InGameMessageContextNotFound = static_cast<int>(0x80552B03u);
constexpr int InGameMessageHandleNotFound = static_cast<int>(0x80552B05u);
constexpr int InGameMessageNotConnected = static_cast<int>(0x80552B08u);

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
    Require(sceNpUnregisterStateCallbackA(0) == InvalidId, "callback id 0");
    Require(sceNpUnregisterStateCallbackA(9) == InvalidId, "callback id 9");
    Require(sceNpUnregisterStateCallbackA(-1) == InvalidId, "callback id -1");
    Require(sceNpUnregisterStateCallbackA(std::numeric_limits<int>::min()) == InvalidId, "callback id INT_MIN");
    Require(sceNpUnregisterStateCallbackA(std::numeric_limits<int>::max()) == InvalidId, "callback id INT_MAX");
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

void CheckGamePresenceCallbacksA() {
    constexpr int CallbackMax = static_cast<int>(0x8055001Du);
    int dummy = 0;
    void* const cb1 = reinterpret_cast<void*>(&AbortingGamePresenceCallback);
    void* const cb2 = reinterpret_cast<void*>(&dummy);
    Require(sceNpRegisterGamePresenceCallbackA(nullptr, &dummy) == InvalidArgument, "null game presence callback");
    Require(sceNpUnregisterGamePresenceCallbackA(0) == NotRegistered, "unregistering id 0");
    Require(sceNpUnregisterGamePresenceCallbackA(-1) == NotRegistered, "unregistering negative id");
    Require(sceNpUnregisterGamePresenceCallbackA(1) == NotRegistered, "unregistering unknown id");
    int ids[8]{};
    for (int i = 0; i < 8; ++i) {
        void* const cb = (i % 2 == 0) ? cb1 : cb2;
        ids[i] = sceNpRegisterGamePresenceCallbackA(cb, &dummy);
        Require(ids[i] == i + 1, "game presence callback id must be monotonic starting at 1");
    }
    Require(sceNpRegisterGamePresenceCallbackA(cb1, &dummy) == CallbackMax, "ninth game presence callback");
    Require(sceNpCheckCallback() == 0, "checking callbacks does not invoke game presence callback A");
    Require(sceNpUnregisterGamePresenceCallbackA(ids[3]) == 0, "unregistering valid callback id");
    Require(sceNpUnregisterGamePresenceCallbackA(ids[3]) == NotRegistered, "double unregistering callback id");
    const int reusedId = sceNpRegisterGamePresenceCallbackA(cb2, &dummy);
    Require(reusedId == 9, "re-registering into freed slot must receive next monotonic id 9");
    Require(sceNpRegisterGamePresenceCallbackA(cb1, &dummy) == CallbackMax, "table must be full again");
    for (int i = 0; i < 8; ++i) {
        if (i == 3) continue;
        Require(sceNpUnregisterGamePresenceCallbackA(ids[i]) == 0, "unregistering remaining callback");
    }
    Require(sceNpUnregisterGamePresenceCallbackA(reusedId) == 0, "unregistering reused callback id");
    Require(sceNpUnregisterGamePresenceCallbackA(reusedId) == NotRegistered, "double unregistering reused id");
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

void CheckWaitAsync() {
    NpCreateAsyncRequestParameter param{};
    const int liveId = sceNpCreateAsyncRequest(&param);
    Require(liveId > 0, "sceNpCreateAsyncRequest must return a positive request id");
    const int deletedId = sceNpCreateAsyncRequest(&param);
    Require(deletedId > 0 && deletedId != liveId, "sceNpCreateAsyncRequest must return distinct positive request ids");
    Require(sceNpDeleteRequest(deletedId) == 0, "initial delete");

    constexpr int resultSentinel = 0x5a5a5a5a;
    for (const int id : {0, -1, 0x7fffffff, deletedId}) {
        int result = resultSentinel;
        Require(sceNpWaitAsync(id, &result) == RequestNotFound, "wait must reject an absent or deleted id");
        Require(result == resultSentinel, "wait must not touch the result for an absent or deleted id");
        Require(sceNpWaitAsync(id, nullptr) == InvalidArgument, "wait must reject a null result before the id");
    }
    Require(sceNpWaitAsync(liveId, nullptr) == InvalidArgument, "wait must reject a null result for a live request");

    for (int round = 0; round < 2; ++round) {
        int result = resultSentinel;
        Require(sceNpWaitAsync(liveId, &result) == 0, "wait on a live async request");
        Require(result == SignedOut, "wait must report the signed-out result on a live async request");
    }
    int pollResult = resultSentinel;
    Require(sceNpPollAsync(liveId, &pollResult) == 0, "poll after wait");
    Require(pollResult == SignedOut, "poll after wait must report the same result");

    Require(sceNpDeleteRequest(liveId) == 0, "delete after wait");
    int result = resultSentinel;
    Require(sceNpWaitAsync(liveId, &result) == RequestNotFound, "wait after delete");
    Require(result == resultSentinel, "wait must not touch the result after delete");
}

void CheckIdMapperRequests() {
    for (const int reqId : {0, -1, -0x7fffffff}) {
        Require(sceNpIdMapperAbortRequest(reqId) == IdMapperRefusedRequestId, "sceNpIdMapperAbortRequest must refuse a request id of 0 or less");
        Require(sceNpIdMapperDeleteRequest(reqId) == IdMapperRefusedRequestId, "sceNpIdMapperDeleteRequest must refuse a request id of 0 or less");
    }
    const int first = sceNpIdMapperCreateRequest();
    const int second = sceNpIdMapperCreateRequest();
    Require(first > 0 && second > 0 && first != second, "sceNpIdMapperCreateRequest must return distinct positive request ids");
    Require(sceNpIdMapperAbortRequest(first) == 0, "abort on a live IdMapper request");
    Require(sceNpIdMapperAbortRequest(first) == 0, "abort must leave the IdMapper request live");
    Require(sceNpIdMapperDeleteRequest(first) == 0, "delete after abort on an IdMapper request");
    Require(sceNpIdMapperAbortRequest(first) == RequestNotFound, "abort after delete");
    Require(sceNpIdMapperDeleteRequest(first) == RequestNotFound, "second delete of an IdMapper request");
    Require(sceNpIdMapperAbortRequest(0x7fffffff) == RequestNotFound, "abort of a never-created IdMapper request");
    Require(sceNpIdMapperDeleteRequest(0x7fffffff) == RequestNotFound, "delete of a never-created IdMapper request");
    Require(sceNpIdMapperDeleteRequest(second) == 0, "delete the second IdMapper request");
}

void CheckManagerIntRequests() {
    int live[64];
    for (int& reqId : live) {
        reqId = sceNpManagerIntCreateRequest();
        Require(reqId > 0, "sceNpManagerIntCreateRequest must return a positive request id while a slot is free");
    }
    for (std::size_t i = 0; i < 64; ++i) {
        for (std::size_t j = i + 1; j < 64; ++j) Require(live[i] != live[j], "live NpManagerInt request ids must differ");
    }
    Require(sceNpManagerIntCreateRequest() == ManagerIntTableFull, "the 65th live NpManagerInt request must give the full-table error");
    Require(sceNpManagerIntDeleteRequest(live[0]) == 0, "deleting a live NpManagerInt request");
    live[0] = sceNpManagerIntCreateRequest();
    Require(live[0] > 0, "a deleted NpManagerInt request frees its slot");
    Require(sceNpManagerIntCreateRequest() == ManagerIntTableFull, "the table is full again");
    Require(sceNpManagerIntAbortRequest(live[1]) == 0, "abort on a live NpManagerInt request");
    Require(sceNpManagerIntAbortRequest(live[1]) == 0, "abort must leave the NpManagerInt request live");
    Require(sceNpManagerIntDeleteRequest(live[1]) == 0, "delete after abort on an NpManagerInt request");
    Require(sceNpManagerIntDeleteRequest(live[1]) == ManagerIntNotInTable, "second delete of an NpManagerInt request");
    for (const int reqId : {0, -1, 0x7fffffff}) {
        Require(sceNpManagerIntDeleteRequest(reqId) == ManagerIntNotInTable, "delete of an NpManagerInt request id not in the table");
        Require(sceNpManagerIntAbortRequest(reqId) == RequestNotFound, "abort of an unknown NpManagerInt request id");
    }
    Require(sceNpManagerIntAbortRequest(live[1]) == RequestNotFound, "abort of a deleted NpManagerInt request id");
    for (std::size_t i = 0; i < 64; ++i) {
        if (i != 1) Require(sceNpManagerIntDeleteRequest(live[i]) == 0, "deleting each NpManagerInt request");
    }
}

void CheckIdMapperLookup(const char* name, decltype(&sceNpIdMapperNpIdToAccountId) lookup) {
    constexpr int UserId = 0x10000000;
    unsigned char input[36] = {};
    unsigned char output[64];
    std::memset(output, 0x5a, sizeof(output));
    unsigned char untouched[sizeof(output)];
    std::memcpy(untouched, output, sizeof(output));
    Require(lookup(0, UserId, input, output, 1, 0, 0) == IdMapperRefusedRequestId, name);
    Require(lookup(-1, UserId, input, output, 1, 0, 0) == IdMapperRefusedRequestId, name);
    Require(lookup(1, -1, input, output, 1, 0, 0) == IdMapperRefusedArgument, name);
    Require(lookup(1, UserId, nullptr, output, 1, 0, 0) == IdMapperRefusedArgument, name);
    Require(lookup(1, UserId, input, nullptr, 1, 0, 0) == IdMapperRefusedArgument, name);
    Require(lookup(1, UserId, input, output, 0, 0, 0) == IdMapperRefusedArgument, name);
    Require(lookup(1, UserId, input, output, 1001, 0, 0) == IdMapperRefusedArgument, name);
    Require(lookup(1, UserId, input, output, 0xffffffffu, 0, 0) == IdMapperRefusedArgument, name);
    Require(lookup(0, UserId, nullptr, output, 1, 0, 0) == IdMapperRefusedRequestId, name);
    Require(lookup(1, UserId, input, output, 1, 0, 0) == SignedOut, name);
    Require(lookup(0x7fffffff, UserId, input, output, 1000, 0xff, ~std::uint64_t{0}) == SignedOut, name);
    Require(std::memcmp(output, untouched, sizeof(output)) == 0, name);
}

struct TicketReleaseRecord {
    void* pointer;
    int calls;
};

void APS5_VABI ReleaseTicket(void* pointer, void* context) {
    auto* record = static_cast<TicketReleaseRecord*>(context);
    record->pointer = pointer;
    record->calls += 1;
}

struct TicketStorage {
    const void* const* vtable;
    NpAllocatorEx* allocator;
    void* data;
    std::size_t size;
};

void CheckTicketTeardown() {
    TicketReleaseRecord record{};
    NpAllocatorEx allocator{};
    allocator.allocator.release = ReleaseTicket;
    allocator.allocator.context = &record;
    std::uint8_t data[4] {};
    TicketStorage ticket{};
    ticket.allocator = &allocator;
    ticket.data = data;
    ticket.size = sizeof(data);
    _ZN3sce2np8NpTicket5ClearEv(&ticket);
    Require(record.calls == 1 && record.pointer == data, "NpTicket::Clear must release the stored data");
    Require(ticket.data == nullptr && ticket.size == 0, "NpTicket::Clear must clear the data and size");
    ticket.data = data;
    ticket.size = sizeof(data);
    _ZN3sce2np8NpTicketD1Ev(&ticket);
    Require(record.calls == 2 && record.pointer == data, "the NpTicket destructor must release the stored data");
    Require(ticket.vtable != nullptr, "the NpTicket destructor must restore the vtable");
    ticket.data = nullptr;
    ticket.size = 0;
    _ZN3sce2np8NpTicket5ClearEv(&ticket);
    Require(record.calls == 2, "NpTicket::Clear must not release an empty ticket");
}

struct SentinelBuffer {
    unsigned char bytes[64];
    SentinelBuffer() { std::memset(bytes, 0x5a, sizeof(bytes)); }
    bool Untouched() const {
        for (const unsigned char byte : bytes) {
            if (byte != 0x5a) return false;
        }
        return true;
    }
};

void CheckPushLifecycle() {
    constexpr int PushAlreadyInitialized = static_cast<int>(0x8055A001u);
    constexpr int PushNotInitialized = static_cast<int>(0x8055A002u);
    constexpr int PushInvalidArgument = static_cast<int>(0x8055A003u);
    constexpr int Push2NotInitialized = static_cast<int>(0x8055C002u);
    constexpr int Push2InvalidArgument = static_cast<int>(0x8055C003u);
    constexpr int Push2NoSlot = static_cast<int>(0x8055C005u);

    Require(sceNpPushCheckCallback() == PushNotInitialized, "sceNpPushCheckCallback must refuse an uninitialized library");
    Require(sceNpPushTerm() == PushNotInitialized, "sceNpPushTerm must refuse an uninitialized library");
    Require(sceNpPushInit() == 0, "sceNpPushInit must initialize the library");
    Require(sceNpPushInit() == PushAlreadyInitialized, "sceNpPushInit must refuse a second initialization");
    std::int32_t connectionState = 0x5a5a5a5a;
    Require(sceNpPushIntGetConnectionState(&connectionState) == 0 && connectionState == 0, "sceNpPushIntGetConnectionState must report the offline state");
    Require(sceNpPushIntGetConnectionState(nullptr) == PushInvalidArgument, "sceNpPushIntGetConnectionState must refuse a null output");
    Require(sceNpPushCheckCallback() == 0, "sceNpPushCheckCallback must answer an initialized library");
    Require(sceNpPushTerm() == 0, "sceNpPushTerm must terminate the library");
    Require(sceNpPushTerm() == PushNotInitialized, "sceNpPushTerm must refuse a second termination");

    Require(!sceNpPush2IsInit(), "sceNpPush2IsInit must be false before initialization");
    Require(sceNpPush2OptionalCheckCallback() == Push2NotInitialized, "sceNpPush2OptionalCheckCallback must refuse an uninitialized library");
    Require(sceNpPush2TriggerEmptyUserEvent() == PushNotInitialized, "sceNpPush2TriggerEmptyUserEvent must report the uninitialized error");
    Require(sceNpPush2Init(nullptr) == Push2InvalidArgument, "sceNpPush2Init must refuse a null parameter");
    std::uint8_t parameter[0x18] {};
    Require(sceNpPush2Init(parameter) == Push2InvalidArgument, "sceNpPush2Init must refuse a zero mode");
    *reinterpret_cast<std::uint32_t*>(parameter + 0x10) = 2;
    *reinterpret_cast<std::uint32_t*>(parameter + 0x14) = 1;
    Require(sceNpPush2Init(parameter) == Push2InvalidArgument, "sceNpPush2Init must refuse an allocator kind above 1");
    *reinterpret_cast<std::uint32_t*>(parameter + 0x10) = 0;
    const int firstContext = sceNpPush2Init(parameter);
    const int secondContext = sceNpPush2Init(parameter);
    Require(firstContext == 1 && secondContext == 2, "sceNpPush2Init must return consecutive context ids");
    Require(sceNpPush2IsInit(), "sceNpPush2IsInit must be true after initialization");
    Require(sceNpPush2OptionalCheckCallback() == 0, "sceNpPush2OptionalCheckCallback must answer an initialized library");
    Require(sceNpPush2TriggerEmptyUserEvent() == 0, "sceNpPush2TriggerEmptyUserEvent must answer an initialized library");
    Require(sceNpPush2SetGlobalMutex(nullptr) == Push2InvalidArgument, "sceNpPush2SetGlobalMutex must refuse a null mutex");
    int mutexToken = 0;
    Require(sceNpPush2SetGlobalMutex(&mutexToken) == 0, "sceNpPush2SetGlobalMutex must accept a mutex");
    Require(sceNpPush2Term(firstContext) == 0, "sceNpPush2Term must terminate a context");
    Require(sceNpPush2IsInit(), "sceNpPush2IsInit must stay true while a context is live");
    Require(sceNpPush2Term(secondContext) == 0, "sceNpPush2Term must terminate the second context");
    Require(!sceNpPush2IsInit(), "sceNpPush2IsInit must be false after every context is terminated");
    Require(sceNpPush2OptionalCheckCallback() == Push2NotInitialized, "sceNpPush2OptionalCheckCallback must refuse a terminated library");
    for (int expected = 1; expected <= 32; ++expected) {
        Require(sceNpPush2Init(parameter) == expected, "sceNpPush2Init must hand out every context slot");
    }
    Require(sceNpPush2Init(parameter) == Push2NoSlot, "sceNpPush2Init must refuse a full context table");
    for (int contextId = 1; contextId <= 32; ++contextId) sceNpPush2Term(contextId);
    Require(!sceNpPush2IsInit(), "sceNpPush2Term must empty the context table");
}

void CheckManagerIntProfileGetters() {
    const int live = sceNpManagerIntCreateRequest();
    const int deleted = sceNpManagerIntCreateRequest();
    Require(live > 0 && deleted > 0 && sceNpManagerIntDeleteRequest(deleted) == 0, "NpManagerInt requests for the profile getters");
    SentinelBuffer out;
    Require(sceNpManagerIntGetAccountAge(live, -1, out.bytes) == InvalidArgument && out.Untouched(), "sceNpManagerIntGetAccountAge must refuse user id -1");
    Require(sceNpManagerIntGetAccountAge(live, InitialUserId, nullptr) == InvalidArgument, "sceNpManagerIntGetAccountAge must refuse a null output");
    Require(sceNpManagerIntGetAccountAge(deleted, InitialUserId, nullptr) == InvalidArgument, "sceNpManagerIntGetAccountAge checks its arguments before the request");
    Require(sceNpManagerIntGetAccountLanguageA(live, -1, out.bytes) == InvalidArgument && out.Untouched(), "sceNpManagerIntGetAccountLanguageA must refuse user id -1");
    Require(sceNpManagerIntGetAccountLanguageA(live, InitialUserId, nullptr) == InvalidArgument, "sceNpManagerIntGetAccountLanguageA must refuse a null output");
    Require(sceNpManagerIntGetAccountLanguageA(deleted, InitialUserId, nullptr) == InvalidArgument, "sceNpManagerIntGetAccountLanguageA checks its arguments before the request");
    for (const int request : {live, deleted, 0x7fffffff, 0, -1}) {
        Require(sceNpManagerIntGetAccountAge(request, InitialUserId, out.bytes) == SignedOut && out.Untouched(), "sceNpManagerIntGetAccountAge must report NP as signed out and write nothing");
        Require(sceNpManagerIntGetAccountLanguageA(request, InitialUserId, out.bytes) == SignedOut && out.Untouched(), "sceNpManagerIntGetAccountLanguageA must report NP as signed out and write nothing");
    }
    Require(sceNpManagerIntGetAccountCountryA(-1, out.bytes) == InvalidArgument && out.Untouched(), "sceNpManagerIntGetAccountCountryA must refuse user id -1");
    Require(sceNpManagerIntGetAccountCountryA(InitialUserId, nullptr) == InvalidArgument, "sceNpManagerIntGetAccountCountryA must refuse a null output");
    Require(sceNpManagerIntGetAccountCountryA(InitialUserId, out.bytes) == SignedOut && out.Untouched(), "sceNpManagerIntGetAccountCountryA must report NP as signed out and write nothing");
    std::uint64_t accountId = 0x5a5a5a5a5a5a5a5a;
    Require(sceNpManagerIntGetAccountId(-1, &accountId) == InvalidArgument && accountId == 0x5a5a5a5a5a5a5a5a, "sceNpManagerIntGetAccountId must refuse user id -1");
    Require(sceNpManagerIntGetAccountId(InitialUserId, nullptr) == InvalidArgument, "sceNpManagerIntGetAccountId must refuse a null output");
    Require(sceNpManagerIntGetAccountId(InitialUserId, &accountId) == SignedOut && accountId == 0x5a5a5a5a5a5a5a5a, "sceNpManagerIntGetAccountId must report NP as signed out and write nothing");
    Require(sceNpManagerIntGetAccountIdSdk(-1, &accountId) == InvalidArgument && accountId == 0x5a5a5a5a5a5a5a5a, "sceNpManagerIntGetAccountIdSdk must refuse user id -1");
    Require(sceNpManagerIntGetAccountIdSdk(InitialUserId, nullptr) == InvalidArgument, "sceNpManagerIntGetAccountIdSdk must refuse a null output");
    Require(sceNpManagerIntGetAccountIdSdk(InitialUserId, &accountId) == SignedOut && accountId == 0x5a5a5a5a5a5a5a5a, "sceNpManagerIntGetAccountIdSdk must report NP as signed out and write nothing");
    for (const auto getter : {sceNpManagerIntGetAccountCountrySdk, sceNpManagerIntGetAccountDateOfBirthSdk, sceNpManagerIntGetAccountNpEnv, sceNpManagerIntGetActiveSigninState, sceNpManagerIntGetAuthServerErrorFlag, sceNpManagerIntGetOnlineIdChangeFlag, sceNpManagerIntGetPlusMemberTypeNB, sceNpManagerIntGetServerError, sceNpManagerIntGetSigninState, sceNpManagerIntGetUserNum}) {
        Require(getter(-1, out.bytes) == InvalidArgument && out.Untouched(), "an NpManagerInt profile getter must refuse user id -1");
        Require(getter(InitialUserId, nullptr) == InvalidArgument, "an NpManagerInt profile getter must refuse a null output");
        Require(getter(InitialUserId, out.bytes) == SignedOut && out.Untouched(), "an NpManagerInt profile getter must report NP as signed out and write nothing");
    }
    std::int32_t accountType = 0x5a5a5a5a;
    Require(sceNpManagerIntGetAccountType(-1, &accountType) == InvalidArgument && accountType == 0x5a5a5a5a, "sceNpManagerIntGetAccountType must refuse user id -1");
    Require(sceNpManagerIntGetAccountType(InitialUserId, nullptr) == InvalidArgument, "sceNpManagerIntGetAccountType must refuse a null output");
    Require(sceNpManagerIntGetAccountType(InitialUserId, &accountType) == SignedOut && accountType == 0, "sceNpManagerIntGetAccountType must zero the account type");
    SentinelBuffer npEnv;
    Require(sceNpManagerIntGetNpEnv(nullptr) == InvalidArgument, "sceNpManagerIntGetNpEnv must refuse a null output");
    Require(sceNpManagerIntGetNpEnv(npEnv.bytes) == SignedOut, "sceNpManagerIntGetNpEnv must report NP as signed out");
    for (std::size_t i = 0; i < 0x18; ++i) Require(npEnv.bytes[i] == 0, "sceNpManagerIntGetNpEnv must zero the environment block");
    for (std::size_t i = 0x18; i < sizeof(npEnv.bytes); ++i) Require(npEnv.bytes[i] == 0x5a, "sceNpManagerIntGetNpEnv must leave the tail untouched");
    Require(sceNpManagerIntGetVshClientId(nullptr) == InvalidArgument, "sceNpManagerIntGetVshClientId must refuse a null output");
    Require(sceNpManagerIntGetVshClientId(out.bytes) == SignedOut && out.Untouched(), "sceNpManagerIntGetVshClientId must report NP as signed out and write nothing");
    std::uint64_t offlineAccountId = 0x5a5a5a5a5a5a5a5a;
    bool offlineFlag = true;
    Require(sceNpManagerIntGetOfflineAccountId(-1, &offlineAccountId, &offlineFlag) == InvalidArgument && offlineAccountId == 0x5a5a5a5a5a5a5a5a && offlineFlag, "sceNpManagerIntGetOfflineAccountId must refuse user id -1");
    Require(sceNpManagerIntGetOfflineAccountId(InitialUserId, &offlineAccountId, &offlineFlag) == SignedOut && offlineAccountId == 0x5a5a5a5a5a5a5a5a && offlineFlag, "sceNpManagerIntGetOfflineAccountId must report NP as signed out and write nothing");
    Require(sceNpManagerIntGetOfflineAccountId(InitialUserId, nullptr, nullptr) == SignedOut, "sceNpManagerIntGetOfflineAccountId must accept null outputs");
    NpId* npId = reinterpret_cast<NpId*>(out.bytes);
    Require(sceNpManagerIntGetNpId(-1, npId) == InvalidArgument && out.Untouched(), "sceNpManagerIntGetNpId must refuse user id -1");
    Require(sceNpManagerIntGetNpId(InitialUserId, nullptr) == InvalidArgument, "sceNpManagerIntGetNpId must refuse a null output");
    Require(sceNpManagerIntGetNpId(InitialUserId, npId) == SignedOut && out.Untouched(), "sceNpManagerIntGetNpId must report NP as signed out and write nothing");
    NpOnlineId* onlineId = reinterpret_cast<NpOnlineId*>(out.bytes);
    Require(sceNpManagerIntGetOnlineId(-1, onlineId) == InvalidArgument && out.Untouched(), "sceNpManagerIntGetOnlineId must refuse user id -1");
    Require(sceNpManagerIntGetOnlineId(InitialUserId, nullptr) == InvalidArgument, "sceNpManagerIntGetOnlineId must refuse a null output");
    Require(sceNpManagerIntGetOnlineId(InitialUserId, onlineId) == SignedOut && out.Untouched(), "sceNpManagerIntGetOnlineId must report NP as signed out and write nothing");
    Require(sceNpManagerIntDeleteRequest(live) == 0, "the profile getters leave a live request live");
    Require(sceNpManagerIntDeleteRequest(deleted) == ManagerIntNotInTable, "the profile getters leave a deleted request deleted");
}

void CheckManagerIntUserLookups() {
    NpOnlineId onlineId{};
    std::memcpy(onlineId.data, "player", 6);
    NpId npId{};
    npId.online_id = onlineId;
    int userId = 0x5a5a5a5a;
    Require(sceNpManagerIntGetUserIdByOnlineId(nullptr, &userId) == InvalidArgument && userId == 0x5a5a5a5a, "sceNpManagerIntGetUserIdByOnlineId must refuse a null online id");
    Require(sceNpManagerIntGetUserIdByOnlineId(&onlineId, nullptr) == InvalidArgument, "sceNpManagerIntGetUserIdByOnlineId must refuse a null output");
    Require(sceNpManagerIntGetUserIdByOnlineId(&onlineId, &userId) == SignedOut && userId == 0x5a5a5a5a, "sceNpManagerIntGetUserIdByOnlineId must report NP as signed out and write nothing");
    Require(sceNpManagerIntGetUserIdByNpId(nullptr, &userId) == InvalidArgument && userId == 0x5a5a5a5a, "sceNpManagerIntGetUserIdByNpId must refuse a null NpId");
    Require(sceNpManagerIntGetUserIdByNpId(&npId, nullptr) == InvalidArgument, "sceNpManagerIntGetUserIdByNpId must refuse a null output");
    Require(sceNpManagerIntGetUserIdByNpId(&npId, &userId) == SignedOut && userId == 0x5a5a5a5a, "sceNpManagerIntGetUserIdByNpId must report NP as signed out and write nothing");
    Require(sceNpManagerIntGetUserIdByOnlineIdSdk(nullptr, &userId) == InvalidArgument && userId == 0x5a5a5a5a, "sceNpManagerIntGetUserIdByOnlineIdSdk must refuse a null online id");
    Require(sceNpManagerIntGetUserIdByOnlineIdSdk(&onlineId, nullptr) == InvalidArgument, "sceNpManagerIntGetUserIdByOnlineIdSdk must refuse a null output");
    Require(sceNpManagerIntGetUserIdByOnlineIdSdk(&onlineId, &userId) == SignedOut && userId == 0x5a5a5a5a, "sceNpManagerIntGetUserIdByOnlineIdSdk must report NP as signed out and write nothing");
    Require(sceNpManagerIntGetNpIdByOnlineId(nullptr, &npId) == InvalidArgument, "sceNpManagerIntGetNpIdByOnlineId must refuse a null online id");
    Require(sceNpManagerIntGetNpIdByOnlineId(&onlineId, nullptr) == InvalidArgument, "sceNpManagerIntGetNpIdByOnlineId must refuse a null output");
    Require(sceNpManagerIntGetNpIdByOnlineId(&onlineId, &npId) == SignedOut, "sceNpManagerIntGetNpIdByOnlineId must report NP as signed out");
    Require(sceNpManagerIntGetUserIdByAccountId(0, &userId) == InvalidArgument && userId == 0x5a5a5a5a, "sceNpManagerIntGetUserIdByAccountId must refuse a zero account id");
    Require(sceNpManagerIntGetUserIdByAccountId(1, nullptr) == InvalidArgument, "sceNpManagerIntGetUserIdByAccountId must refuse a null output");
    Require(sceNpManagerIntGetUserIdByAccountId(1, &userId) == SignedOut && userId == 0x5a5a5a5a, "sceNpManagerIntGetUserIdByAccountId must report NP as signed out and write nothing");
    Require(sceNpManagerIntGetOnlineIdByAccountId(0, &onlineId) == InvalidArgument, "sceNpManagerIntGetOnlineIdByAccountId must refuse a zero account id");
    Require(sceNpManagerIntGetOnlineIdByAccountId(1, nullptr) == InvalidArgument, "sceNpManagerIntGetOnlineIdByAccountId must refuse a null output");
    Require(sceNpManagerIntGetOnlineIdByAccountId(1, &onlineId) == SignedOut, "sceNpManagerIntGetOnlineIdByAccountId must report NP as signed out");
    Require(sceNpManagerIntGetNpIdSdk(-1, &npId) == InvalidArgument, "sceNpManagerIntGetNpIdSdk must refuse user id -1");
    Require(sceNpManagerIntGetNpIdSdk(InitialUserId, nullptr) == InvalidArgument, "sceNpManagerIntGetNpIdSdk must refuse a null output");
    Require(sceNpManagerIntGetNpIdSdk(InitialUserId, &npId) == SignedOut, "sceNpManagerIntGetNpIdSdk must report NP as signed out");
    Require(sceNpManagerIntGetOnlineIdSdk(-1, &onlineId) == InvalidArgument, "sceNpManagerIntGetOnlineIdSdk must refuse user id -1");
    Require(sceNpManagerIntGetOnlineIdSdk(InitialUserId, nullptr) == InvalidArgument, "sceNpManagerIntGetOnlineIdSdk must refuse a null output");
    Require(sceNpManagerIntGetOnlineIdSdk(InitialUserId, &onlineId) == SignedOut, "sceNpManagerIntGetOnlineIdSdk must report NP as signed out");
    SentinelBuffer parentalFlag;
    Require(sceNpManagerIntGetParentalControlFlag(-1, parentalFlag.bytes) == InvalidArgument && parentalFlag.Untouched(), "sceNpManagerIntGetParentalControlFlag must refuse user id -1");
    Require(sceNpManagerIntGetParentalControlFlag(InitialUserId, nullptr) == InvalidArgument, "sceNpManagerIntGetParentalControlFlag must refuse a null output");
    Require(sceNpManagerIntGetParentalControlFlag(InitialUserId, parentalFlag.bytes) == SignedOut && parentalFlag.Untouched(), "sceNpManagerIntGetParentalControlFlag must report NP as signed out and write nothing");
}

void CheckManagerIntAuthorization() {
    constexpr int InvalidPlatformType = static_cast<int>(0x80550004u);
    constexpr int OutOfMemory = static_cast<int>(0x80550005u);
    const int live = sceNpManagerIntCreateRequest();
    Require(live > 0, "an NpManagerInt request for the authorization calls");
    SentinelBuffer parameter;
    SentinelBuffer code;
    SentinelBuffer extra;
    auto untouched = [&] { return parameter.Untouched() && code.Untouched() && extra.Untouched(); };
    Require(sceNpManagerIntAuthGetAuthorizationCode(live, -1, parameter.bytes, nullptr, 0, 1, 0, code.bytes) == InvalidArgument && untouched(), "sceNpManagerIntAuthGetAuthorizationCode must refuse user id -1");
    Require(sceNpManagerIntAuthGetAuthorizationCode(live, InitialUserId, nullptr, nullptr, 0, 1, 0, code.bytes) == InvalidArgument && untouched(), "sceNpManagerIntAuthGetAuthorizationCode must refuse a null parameter");
    Require(sceNpManagerIntAuthGetAuthorizationCode(live, InitialUserId, parameter.bytes, nullptr, 0, 1, 0, nullptr) == InvalidArgument && untouched(), "sceNpManagerIntAuthGetAuthorizationCode must refuse a null output");
    Require(sceNpManagerIntAuthGetAuthorizationCode(live, InitialUserId, parameter.bytes, nullptr, 4, 1, 0, code.bytes) == InvalidArgument && untouched(), "sceNpManagerIntAuthGetAuthorizationCode must refuse a first value above 3");
    Require(sceNpManagerIntAuthGetAuthorizationCode(live, InitialUserId, parameter.bytes, nullptr, 0, 0, 0, code.bytes) == InvalidArgument && untouched(), "sceNpManagerIntAuthGetAuthorizationCode must refuse a second value of 0");
    Require(sceNpManagerIntAuthGetAuthorizationCode(live, InitialUserId, parameter.bytes, nullptr, 0, 4, 0, code.bytes) == InvalidArgument && untouched(), "sceNpManagerIntAuthGetAuthorizationCode must refuse a second value above 3");
    Require(sceNpManagerIntAuthGetAuthorizationCode(live, InitialUserId, parameter.bytes, nullptr, 0, 1, 0, code.bytes) == SignedOut && untouched(), "sceNpManagerIntAuthGetAuthorizationCode must report NP as signed out and write nothing");
    Require(sceNpManagerIntAuthGetAuthorizationCode(0x7fffffff, InitialUserId, parameter.bytes, extra.bytes, 3, 3, 0xff, code.bytes) == SignedOut && untouched(), "sceNpManagerIntAuthGetAuthorizationCode with the optional pointer and the upper values");
    Require(sceNpManagerIntAuthGetIdToken(live, -1, parameter.bytes, parameter.bytes, parameter.bytes, 0, code.bytes) == InvalidArgument && untouched(), "sceNpManagerIntAuthGetIdToken must refuse user id -1");
    Require(sceNpManagerIntAuthGetIdToken(live, InitialUserId, nullptr, parameter.bytes, parameter.bytes, 0, code.bytes) == InvalidArgument && untouched(), "sceNpManagerIntAuthGetIdToken must refuse a null first pointer");
    Require(sceNpManagerIntAuthGetIdToken(live, InitialUserId, parameter.bytes, nullptr, parameter.bytes, 0, code.bytes) == InvalidArgument && untouched(), "sceNpManagerIntAuthGetIdToken must refuse a null second pointer");
    Require(sceNpManagerIntAuthGetIdToken(live, InitialUserId, parameter.bytes, parameter.bytes, nullptr, 0, code.bytes) == InvalidArgument && untouched(), "sceNpManagerIntAuthGetIdToken must refuse a null third pointer");
    Require(sceNpManagerIntAuthGetIdToken(live, InitialUserId, parameter.bytes, parameter.bytes, parameter.bytes, 0, nullptr) == InvalidArgument && untouched(), "sceNpManagerIntAuthGetIdToken must refuse a null output");
    Require(sceNpManagerIntAuthGetIdToken(live, InitialUserId, parameter.bytes, parameter.bytes, parameter.bytes, ~std::uint64_t{0}, code.bytes) == SignedOut && untouched(), "sceNpManagerIntAuthGetIdToken must report NP as signed out and write nothing");
    Require(sceNpManagerIntGetAuthorizationCode(live, -1, parameter.bytes, parameter.bytes, nullptr, 0, 0, nullptr, code.bytes) == InvalidArgument && untouched(), "sceNpManagerIntGetAuthorizationCode must refuse user id -1");
    Require(sceNpManagerIntGetAuthorizationCode(live, InitialUserId, nullptr, parameter.bytes, nullptr, 0, 0, nullptr, code.bytes) == InvalidArgument && untouched(), "sceNpManagerIntGetAuthorizationCode must refuse a null first pointer");
    Require(sceNpManagerIntGetAuthorizationCode(live, InitialUserId, parameter.bytes, nullptr, nullptr, 0, 0, nullptr, code.bytes) == InvalidArgument && untouched(), "sceNpManagerIntGetAuthorizationCode must refuse a null second pointer");
    Require(sceNpManagerIntGetAuthorizationCode(live, InitialUserId, parameter.bytes, parameter.bytes, nullptr, 0, 0, nullptr, nullptr) == InvalidArgument && untouched(), "sceNpManagerIntGetAuthorizationCode must refuse a null output");
    for (const std::uint32_t mode : {1u, 2u, 4u, 6u, 0xffffffffu}) {
        Require(sceNpManagerIntGetAuthorizationCode(live, InitialUserId, parameter.bytes, parameter.bytes, nullptr, mode, 0, nullptr, code.bytes) == InvalidPlatformType && untouched(), "sceNpManagerIntGetAuthorizationCode must refuse a mode other than 0, 3 or 5");
    }
    Require(sceNpManagerIntGetAuthorizationCode(live, -1, parameter.bytes, parameter.bytes, nullptr, 1, 0, nullptr, code.bytes) == InvalidArgument && untouched(), "sceNpManagerIntGetAuthorizationCode checks the user id before the mode");
    for (const std::uint32_t mode : {0u, 3u, 5u}) {
        Require(sceNpManagerIntGetAuthorizationCode(live, InitialUserId, parameter.bytes, parameter.bytes, extra.bytes, mode, 0xff, extra.bytes, code.bytes) == SignedOut && untouched(), "sceNpManagerIntGetAuthorizationCode must report NP as signed out and write nothing");
    }
    static char longUri[0x2001];
    std::memset(longUri, 'a', 0x2000);
    const char* uri = "https://example.com/";
    Require(sceNpManagerIntGetAuthorizationCodeWithPsnoUri(live, -1, uri, 0, code.bytes, 0x801) == InvalidArgument && untouched(), "sceNpManagerIntGetAuthorizationCodeWithPsnoUri must refuse user id -1");
    Require(sceNpManagerIntGetAuthorizationCodeWithPsnoUri(live, InitialUserId, nullptr, 0, code.bytes, 0x801) == InvalidArgument && untouched(), "sceNpManagerIntGetAuthorizationCodeWithPsnoUri must refuse a null URI");
    Require(sceNpManagerIntGetAuthorizationCodeWithPsnoUri(live, InitialUserId, uri, 0, nullptr, 0x801) == InvalidArgument && untouched(), "sceNpManagerIntGetAuthorizationCodeWithPsnoUri must refuse a null output");
    Require(sceNpManagerIntGetAuthorizationCodeWithPsnoUri(live, InitialUserId, uri, 0, code.bytes, 0) == InvalidSize && untouched(), "sceNpManagerIntGetAuthorizationCodeWithPsnoUri must refuse a size of 0");
    Require(sceNpManagerIntGetAuthorizationCodeWithPsnoUri(live, InitialUserId, uri, 0, code.bytes, 0x802) == InvalidSize && untouched(), "sceNpManagerIntGetAuthorizationCodeWithPsnoUri must refuse a size above 0x801");
    Require(sceNpManagerIntGetAuthorizationCodeWithPsnoUri(live, InitialUserId, uri, 4, code.bytes, 0x801) == InvalidPlatformType && untouched(), "sceNpManagerIntGetAuthorizationCodeWithPsnoUri must refuse a mode other than 0, 3 or 5");
    Require(sceNpManagerIntGetAuthorizationCodeWithPsnoUri(live, InitialUserId, longUri, 0, code.bytes, 0x801) == OutOfMemory && untouched(), "sceNpManagerIntGetAuthorizationCodeWithPsnoUri must refuse a URI of 0x2000 bytes");
    Require(sceNpManagerIntGetAuthorizationCodeWithPsnoUri(live, InitialUserId, nullptr, 0, code.bytes, 0) == InvalidArgument && untouched(), "sceNpManagerIntGetAuthorizationCodeWithPsnoUri checks a null URI before the size");
    Require(sceNpManagerIntGetAuthorizationCodeWithPsnoUri(live, InitialUserId, uri, 4, code.bytes, 0) == InvalidSize && untouched(), "sceNpManagerIntGetAuthorizationCodeWithPsnoUri checks the size before the mode");
    Require(sceNpManagerIntGetAuthorizationCodeWithPsnoUri(live, InitialUserId, longUri, 4, code.bytes, 0x801) == InvalidPlatformType && untouched(), "sceNpManagerIntGetAuthorizationCodeWithPsnoUri checks the mode before the URI length");
    for (const std::size_t size : {std::size_t{1}, std::size_t{0x801}}) {
        Require(sceNpManagerIntGetAuthorizationCodeWithPsnoUri(live, InitialUserId, uri, 5, code.bytes, size) == SignedOut && untouched(), "sceNpManagerIntGetAuthorizationCodeWithPsnoUri must report NP as signed out and write nothing");
    }
    Require(sceNpManagerIntDeleteRequest(live) == 0, "the authorization calls leave a live request live");
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
        Require(sceNpSetGamePresenceOnlineA(request, userId, 0) == SignedOut, "sceNpSetGamePresenceOnlineA must report NP as signed out");
    }
    NpCreateAsyncRequestParameter asyncParam{};
    const int asyncRequest = sceNpCreateAsyncRequest(&asyncParam);
    Require(asyncRequest > 0, "sceNpCreateAsyncRequest must return a positive request id");
    Require(sceNpSetGamePresenceOnlineA(asyncRequest, InitialUserId, 0) == SignedOut, "sceNpSetGamePresenceOnlineA must report NP as signed out");
    Require(sceNpDeleteRequest(asyncRequest) == 0, "sceNpDeleteRequest failed");
    Require(sceNpSetGamePresenceOnlineA(asyncRequest, InitialUserId, 0) == RequestNotFound, "sceNpSetGamePresenceOnlineA must reject a deleted request");
    Require(sceNpSetGamePresenceOnlineA(request, InvalidUserId, 0) == InvalidArgument, "sceNpSetGamePresenceOnlineA must reject the invalid user id");
    Require(sceNpSetGamePresenceOnlineA(0, InitialUserId, 0) == InvalidArgument, "sceNpSetGamePresenceOnlineA must reject request id 0");
    Require(sceNpSetGamePresenceOnlineA(NeverCreated, InitialUserId, 0) == RequestNotFound, "sceNpSetGamePresenceOnlineA must reject a request that was never created");
    Require(sceNpSetGamePresenceOnlineA(NeverCreated, InvalidUserId, 0) == InvalidArgument, "sceNpSetGamePresenceOnlineA must check the user before the request");
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
    Require(sceNpSetGamePresenceOnlineA(request, InitialUserId, 0) == RequestNotFound, "sceNpSetGamePresenceOnlineA must reject a deleted request");
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
    bad.ageRestrictionCount = -1;
    Require(SetContentRestriction(bad) == InvalidArgument, "sceNpSetContentRestriction must reject a negative restriction count");
    bad.ageRestrictionCount = 0x100;
    Require(SetContentRestriction(bad) == 0, "sceNpSetContentRestriction must accept 256 restrictions");
    bad.ageRestrictions = nullptr;
    Require(SetContentRestriction(bad) == InvalidArgument, "sceNpSetContentRestriction must reject restrictions without an array");
}

static void APS5_VABI PremiumEvent(int, int, void*) {}

static void CheckAccountQueriesAndPremiumCallback() {
    std::uint8_t age = 0x5a;
    Require(sceNpGetAccountAge(1, 0x10000, nullptr) == InvalidArgument, "sceNpGetAccountAge must reject a null age");
    Require(sceNpGetAccountAge(1, 0x10000, &age) == SignedOut, "sceNpGetAccountAge must report the user as signed out");
    Require(age == 0x5a, "sceNpGetAccountAge must leave the age untouched");
    unsigned char language[8];
    std::memset(language, 0x5a, sizeof(language));
    Require(sceNpGetAccountLanguage2(1, 0x10000, language) == SignedOut, "sceNpGetAccountLanguage2 must report the user as signed out");
    for (const unsigned char byte : language) Require(byte == 0x5a, "sceNpGetAccountLanguage2 must leave the language untouched");
    Require(sceNpRegisterPremiumEventCallback(nullptr, nullptr) == InvalidArgument, "sceNpRegisterPremiumEventCallback must reject a null callback");
    Require(sceNpRegisterPremiumEventCallback(reinterpret_cast<void*>(&PremiumEvent), nullptr) == 0, "registering a premium event callback");
    Require(sceNpUnregisterPremiumEventCallback() == 0, "unregistering the premium event callback");
}

void CheckUnregisteredClientError() {
    const std::uint32_t others[] = {0, 0x82e0104c, 0x82e0104e, 0x80550000, 0x82e1104d, 0xffffffff};
    Require(sceNpManagerIntIsUnregisteredClientError(static_cast<int>(0x82e0104d)), "0x82E0104D must be the unregistered client error");
    for (const std::uint32_t error : others) Require(!sceNpManagerIntIsUnregisteredClientError(static_cast<int>(error)), "only 0x82E0104D is the unregistered client error");
}

static void CheckSandboxInfo() {
    unsigned char info[0x60];
    std::memset(info, 0xa5, sizeof(info));
    Require(sceNpGetSandboxInfo(info) == 0, "sceNpGetSandboxInfo failed");
    Require(std::memcmp(info, "RETAIL", 7) == 0, "sceNpGetSandboxInfo must report the RETAIL platform at offset 0");
    Require(std::memcmp(info + 0x28, "LIVE", 5) == 0, "sceNpGetSandboxInfo must report the LIVE environment at offset 0x28");
    for (std::size_t index = 7; index < 0x28; ++index) Require(info[index] == 0, "sceNpGetSandboxInfo must clear the rest of the platform field");
    for (std::size_t index = 0x2d; index < 0x50; ++index) Require(info[index] == 0, "sceNpGetSandboxInfo must clear the rest of the 0x50-byte block");
    for (std::size_t index = 0x50; index < sizeof(info); ++index) Require(info[index] == 0xa5, "sceNpGetSandboxInfo must not write past 0x50 bytes");
    Require(sceNpGetSandboxInfo(nullptr) == InvalidArgument, "sceNpGetSandboxInfo must reject a null buffer");
}

void CheckValueClasses() {
    constexpr int OnlineIdTooLong = static_cast<int>(0x80559E11u);
    constexpr int UserInvalid = static_cast<int>(0x80559E2Au);
    constexpr int AccountIdMissing = static_cast<int>(0x80559E2Bu);
    constexpr int GuestUser = 0x20000000;

    NpOnlineIdObject onlineId;
    _ZN3sce2np10NpOnlineIdC1Ev(&onlineId);
    Require(_ZNK3sce2np10NpOnlineId7IsEmptyEv(&onlineId), "a default online id must be empty");
    Require(_ZN3sce2np10NpOnlineId16SetNpOnlineIdStrEPKc(&onlineId, "player") == 0, "a short online id must be accepted");
    Require(!_ZNK3sce2np10NpOnlineId7IsEmptyEv(&onlineId), "a set online id must not be empty");
    Require(std::strcmp(onlineId.data, "player") == 0, "the online id text must be stored");
    Require(onlineId.data[6] == '\0' && onlineId.data[15] == '\0', "the online id must be zero padded");
    Require(_ZN3sce2np10NpOnlineId16SetNpOnlineIdStrEPKc(&onlineId, "0123456789abcdef") == 0, "a 16-character online id must be accepted");
    Require(_ZN3sce2np10NpOnlineId16SetNpOnlineIdStrEPKc(&onlineId, "0123456789abcdefg") == OnlineIdTooLong, "a longer online id must be refused");
    Require(std::memcmp(onlineId.data, "0123456789abcdef", 16) == 0, "a refused online id must leave the text untouched");
    Require(_ZN3sce2np10NpOnlineId16SetNpOnlineIdStrEPKc(&onlineId, nullptr) == InvalidArgument, "a null online id text must be refused");

    NpOnlineIdObject onlineIdCopy;
    _ZN3sce2np10NpOnlineIdC1ERKS1_(&onlineIdCopy, &onlineId);
    Require(_ZN3sce2npeqERKNS0_10NpOnlineIdES3_(&onlineId, &onlineIdCopy), "an online id must equal its copy");
    Require(!_ZN3sce2npneERKNS0_10NpOnlineIdES3_(&onlineId, &onlineIdCopy), "an online id must not differ from its copy");
    SceNpOnlineId sdkOnlineId {};
    std::memcpy(sdkOnlineId.data, onlineId.data, sizeof(sdkOnlineId.data));
    sdkOnlineId.term = onlineId.term;
    Require(_ZN3sce2npeqERK13SceNpOnlineIdRKNS0_10NpOnlineIdE(&sdkOnlineId, &onlineId), "an SDK online id must equal the matching object");
    Require(_ZN3sce2npeqERKNS0_10NpOnlineIdERK13SceNpOnlineId(&onlineId, &sdkOnlineId), "the object must equal the matching SDK online id");
    sdkOnlineId.data[0] = 'x';
    Require(_ZN3sce2npneERK13SceNpOnlineIdRKNS0_10NpOnlineIdE(&sdkOnlineId, &onlineId), "a different SDK online id must differ");
    Require(_ZN3sce2npneERKNS0_10NpOnlineIdERK13SceNpOnlineId(&onlineId, &sdkOnlineId), "the object must differ from a different SDK online id");
    _ZN3sce2np10NpOnlineId5ClearEv(&onlineId);
    Require(_ZNK3sce2np10NpOnlineId7IsEmptyEv(&onlineId), "a cleared online id must be empty");

    NpIdObject npId;
    _ZN3sce2np4NpIdC1Ev(&npId);
    SceNpId sdkNpId {};
    std::memcpy(sdkNpId.handle.data, "player", 6);
    sdkNpId.opt[0] = 1;
    sdkNpId.reserved[0] = 2;
    NpIdObject npIdFromSdk;
    _ZN3sce2np4NpIdC1ERK7SceNpId(&npIdFromSdk, &sdkNpId);
    Require(std::memcmp(&npIdFromSdk.id, &sdkNpId, sizeof(sdkNpId)) == 0, "an NpId must copy the SDK block");
    NpIdObject npIdCopy;
    _ZN3sce2np4NpIdC1ERKS1_(&npIdCopy, &npIdFromSdk);
    Require(std::memcmp(&npIdCopy.id, &sdkNpId, sizeof(sdkNpId)) == 0, "an NpId must copy another NpId");
    _ZN3sce2np4NpId5ClearEv(&npIdCopy);
    Require(npIdCopy.id.handle.data[0] == '\0', "a cleared NpId must drop the handle");

    UserObject user;
    _ZN3sce2np4UserC1Ev(&user);
    Require(_ZNK3sce2np4User7IsEmptyEv(&user), "a default user must be empty");
    Require(_ZNK3sce2np4User9GetUserIdEv(&user) == USER_SERVICE_USER_ID_INVALID, "a default user must have the invalid id");
    UserObject guestUser;
    _ZN3sce2np4UserC1Ei(&guestUser, GuestUser);
    Require(_ZNK3sce2np4User7IsGuestEv(&guestUser), "a guest id must be reported as a guest");
    Require(!_ZNK3sce2np4User7IsGuestEv(&user), "an invalid id must not be reported as a guest");
    Require(!_ZNK3sce2np4User10IsLoggedInEv(&user), "an invalid id must not be logged in");
    std::uint64_t accountId = 0x5a5a;
    Require(_ZNK3sce2np4User12GetAccountIdEPm(&user, &accountId) < 0 && accountId == 0, "a missing user must not report an account id");
    bool hasAccountId = true;
    Require(_ZNK3sce2np4User12HasAccountIdEPb(&user, &hasAccountId) < 0, "a missing user must not answer an account id query");
    Require(_ZNK3sce2np4User25GetAccountIdFromRegistoryEPm(&user, &accountId) < 0, "a missing user must not read the registry");
    Require(_ZNK3sce2np4User12GetAccountIdEPm(&user, nullptr) == InvalidArgument, "a null account id output must be refused");
    Require(_ZNK3sce2np4User12HasAccountIdEPb(&user, nullptr) == InvalidArgument, "a null account id flag must be refused");

    UserObject userCopy;
    _ZN3sce2np4UserC1ERKS1_(&userCopy, &guestUser);
    Require(_ZN3sce2npeqERKNS0_4UserES3_(&userCopy, &guestUser), "a user must equal its copy");
    const int guestId = GuestUser;
    Require(_ZN3sce2npeqERKNS0_4UserERKi(&userCopy, &guestId), "a user must equal its id");
    Require(!_ZN3sce2npneERKNS0_4UserES3_(&userCopy, &guestUser), "a user must not differ from its copy");
    Require(!_ZN3sce2npneERKNS0_4UserERKi(&userCopy, &guestId), "a user must not differ from its id");
    _ZN3sce2np4User5ClearEv(&userCopy);
    Require(_ZNK3sce2np4User7IsEmptyEv(&userCopy), "a cleared user must be empty");

    UserObject initialUser;
    _ZN3sce2np4UserC1Ei(&initialUser, USER_SERVICE_INITIAL_USER_ID);
    Require(_ZNK3sce2np4User10IsLoggedInEv(&initialUser), "the initial user must be logged in");
    Require(!_ZNK3sce2np4User7IsGuestEv(&initialUser), "the initial user must not be a guest");
    accountId = 0x5a5a;
    Require(_ZNK3sce2np4User12GetAccountIdEPm(&initialUser, &accountId) == AccountIdMissing && accountId == 0, "a user without an account id must report the missing id");
    hasAccountId = true;
    Require(_ZNK3sce2np4User12HasAccountIdEPb(&initialUser, &hasAccountId) == 0 && !hasAccountId, "a user without an account id must report no account");
    Require(_ZNK3sce2np4User25GetAccountIdFromRegistoryEPm(&initialUser, &accountId) == 0 && accountId == 0, "the registry lookup must return the empty account id");

    UserObject fetchedUser;
    Require(_ZN3sce2np4User7GetUserEiPS1_(USER_SERVICE_USER_ID_INVALID, &fetchedUser) == UserInvalid, "GetUser must refuse the invalid id");
    Require(_ZN3sce2np4User7GetUserEiPS1_(123, &fetchedUser) == UserInvalid, "GetUser must refuse a user that is not registered");
    Require(_ZN3sce2np4User7GetUserEiPS1_(USER_SERVICE_INITIAL_USER_ID, &fetchedUser) == 0 && fetchedUser.userId == USER_SERVICE_INITIAL_USER_ID, "GetUser must return the registered user");
    Require(_ZN3sce2np4User7GetUserEiPS1_(USER_SERVICE_INITIAL_USER_ID, nullptr) == InvalidArgument, "GetUser must refuse a null output");

    SceNpOnlineId onlineIdOut {};
    Require(_ZN3sce2np10NpOnlineId13GetNpOnlineIdERKNS0_4UserEP13SceNpOnlineId(&user, &onlineIdOut) == UserNotFound, "GetNpOnlineId must refuse the invalid user");
    Require(_ZN3sce2np10NpOnlineId13GetNpOnlineIdERKNS0_4UserEP13SceNpOnlineId(&guestUser, &onlineIdOut) == SignedOut, "GetNpOnlineId must report a signed-out user");
    Require(_ZN3sce2np10NpOnlineId13GetNpOnlineIdERKNS0_4UserEP13SceNpOnlineId(&guestUser, nullptr) == InvalidArgument, "GetNpOnlineId must refuse a null SDK output");
    NpOnlineIdObject onlineIdObjectOut;
    Require(_ZN3sce2np10NpOnlineId13GetNpOnlineIdERKNS0_4UserEPS1_(&guestUser, &onlineIdObjectOut) == SignedOut, "GetNpOnlineId must report a signed-out object output");
    Require(_ZN3sce2np10NpOnlineId13GetNpOnlineIdERKNS0_4UserEPS1_(nullptr, &onlineIdObjectOut) == InvalidArgument, "GetNpOnlineId must refuse a null user");

    NpTitleTokenObject titleToken;
    _ZN3sce2np12NpTitleTokenC1Ev(&titleToken);
    Require(_ZNK3sce2np12NpTitleToken6GetStrEv(&titleToken) == titleToken.data, "GetStr must point at the token storage");
    Require(titleToken.data[0] == '\0' && titleToken.data[0x40f] == '\0', "a default title token must be empty");
    NpTitleTokenObject titleTokenCopy;
    _ZN3sce2np12NpTitleTokenC1ERKS1_(&titleTokenCopy, &titleToken);
    Require(std::memcmp(titleTokenCopy.data, titleToken.data, sizeof(titleToken.data)) == 0, "a title token must copy the full block");
    _ZN3sce2np12NpTitleToken5ClearEv(&titleTokenCopy);
    Require(titleTokenCopy.data[0] == '\0', "a cleared title token must be empty");

    NpAccessTokenObject accessToken;
    _ZN3sce2np13NpAccessTokenC1Ev(&accessToken);
    Require(_ZNK3sce2np13NpAccessToken7IsEmptyEv(&accessToken), "a default access token must be empty");
    accessToken.data[0] = 1;
    Require(!_ZNK3sce2np13NpAccessToken7IsEmptyEv(&accessToken), "a set access token must not be empty");
    NpAccessTokenObject accessTokenCopy;
    _ZN3sce2np13NpAccessTokenC1ERKS1_(&accessTokenCopy, &accessToken);
    Require(std::memcmp(accessTokenCopy.data, accessToken.data, sizeof(accessToken.data)) == 0, "an access token must copy the full block");
    _ZN3sce2np13NpAccessToken5ClearEv(&accessTokenCopy);
    Require(_ZNK3sce2np13NpAccessToken7IsEmptyEv(&accessTokenCopy), "a cleared access token must be empty");

    NpTicketObject ticket {};
    std::uint8_t ticketData[4] {};
    ticket.data = ticketData;
    ticket.size = sizeof(ticketData);
    Require(_ZNK3sce2np8NpTicket13GetTicketDataEv(&ticket) == ticketData, "GetTicketData must return the stored data");
    Require(_ZNK3sce2np8NpTicket13GetTicketSizeEv(&ticket) == sizeof(ticketData), "GetTicketSize must return the stored size");
    ticket.data = nullptr;
    try {
        _ZNK3sce2np8NpTicket13GetTicketDataEv(&ticket);
        Require(false, "GetTicketData must throw without data");
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()) == "NpTicket data is empty", "GetTicketData threw");
    }
}

void CheckNpOfflineGetters() {
    const char accountId[16] = {};
    char language[8];
    char untouchedLanguage[8];
    std::memset(language, 0x5a, sizeof(language));
    std::memset(untouchedLanguage, 0x5a, sizeof(untouchedLanguage));
    Require(sceNpManagerIntGetAccountLanguage(-1, accountId, language) == InvalidArgument, "sceNpManagerIntGetAccountLanguage must reject user -1");
    Require(sceNpManagerIntGetAccountLanguage(0x10000, nullptr, language) == InvalidArgument, "sceNpManagerIntGetAccountLanguage must reject a null account id");
    Require(sceNpManagerIntGetAccountLanguage(0x10000, accountId, nullptr) == InvalidArgument, "sceNpManagerIntGetAccountLanguage must reject a null output");
    Require(sceNpManagerIntGetAccountLanguage(0x10000, accountId, language) == SignedOut, "sceNpManagerIntGetAccountLanguage must report NP as signed out");
    Require(std::memcmp(language, untouchedLanguage, sizeof(language)) == 0, "sceNpManagerIntGetAccountLanguage must leave the output untouched");
    std::uint32_t status = 0x5a5a5a5a;
    Require(sceNpManagerIntGetCommunicationRestrictionStatus(0x10000, -1, &status) == InvalidArgument, "sceNpManagerIntGetCommunicationRestrictionStatus must reject -1");
    Require(sceNpManagerIntGetCommunicationRestrictionStatus(0x10000, 0, nullptr) == InvalidArgument, "sceNpManagerIntGetCommunicationRestrictionStatus must reject a null output");
    Require(sceNpManagerIntGetCommunicationRestrictionStatus(0x10000, 0, &status) == SignedOut, "sceNpManagerIntGetCommunicationRestrictionStatus must report NP as signed out");
    Require(status == 0x5a5a5a5a, "sceNpManagerIntGetCommunicationRestrictionStatus must leave the output untouched");
    int flag = 0x5a5a5a5a;
    Require(sceNpManagerIntIsSubAccount(-1, &flag) == InvalidArgument, "sceNpManagerIntIsSubAccount must reject user -1");
    Require(sceNpManagerIntIsSubAccount(0x10000, nullptr) == InvalidArgument, "sceNpManagerIntIsSubAccount must reject a null output");
    Require(sceNpManagerIntIsSubAccount(0x10000, &flag) == SignedOut, "sceNpManagerIntIsSubAccount must report NP as signed out");
    Require(flag == 0x5a5a5a5a, "sceNpManagerIntIsSubAccount must leave the output untouched");
    Require(sceNpManagerIntIsTemporarySignout(0x10000, nullptr) == InvalidArgument, "sceNpManagerIntIsTemporarySignout must reject a null output");
    Require(sceNpManagerIntIsTemporarySignout(0x10000, &flag) == SignedOut, "sceNpManagerIntIsTemporarySignout must report NP as signed out");
    Require(flag == 0x5a5a5a5a, "sceNpManagerIntIsTemporarySignout must leave the output untouched");
}

void CheckAsmLifecycle() {
    constexpr int AsmAlreadyInitialized = static_cast<int>(0x8055A201u);
    constexpr int AsmNotInitialized = static_cast<int>(0x8055A202u);
    constexpr int AsmInvalidArgument = static_cast<int>(0x8055A203u);
    constexpr int ClientAlreadyInitialized = static_cast<int>(0x8055A281u);
    constexpr int ClientNotInitialized = static_cast<int>(0x8055A282u);
    constexpr int ClientInvalidArgument = static_cast<int>(0x8055A283u);
    int connection = 0;
    int request = 0;
    int result = 0;
    Require(sceNpAsmTerminate() == AsmNotInitialized, "sceNpAsmTerminate must reject an uninitialized service");
    Require(sceNpAsmAbort() == AsmNotInitialized, "sceNpAsmAbort must reject an uninitialized service");
    Require(sceNpAsmCreateConnection(1, &connection) == AsmNotInitialized, "sceNpAsmCreateConnection must reject an uninitialized service");
    Require(sceNpAsmDeleteConnection(nullptr) == AsmNotInitialized, "sceNpAsmDeleteConnection must reject an uninitialized service");
    Require(sceNpAsmCreateRequest(nullptr, nullptr, nullptr) == AsmNotInitialized, "sceNpAsmCreateRequest must reject an uninitialized service");
    Require(sceNpAsmDeleteRequest(nullptr) == AsmNotInitialized, "sceNpAsmDeleteRequest must reject an uninitialized service");
    Require(sceNpAsmInitialize() == 0, "sceNpAsmInitialize failed");
    Require(sceNpAsmInitialize() == AsmAlreadyInitialized, "sceNpAsmInitialize must reject a second initialization");
    Require(sceNpAsmAbort() == 0, "sceNpAsmAbort failed");
    Require(sceNpAsmCreateConnection(1, nullptr) == AsmInvalidArgument, "sceNpAsmCreateConnection must reject a null connection");
    Require(sceNpAsmDeleteConnection(nullptr) == AsmInvalidArgument, "sceNpAsmDeleteConnection must reject a null connection");
    Require(sceNpAsmCreateRequest(nullptr, &request, &result) == AsmInvalidArgument, "sceNpAsmCreateRequest must reject a null connection");
    Require(sceNpAsmCreateConnection(1, &connection) == 0, "sceNpAsmCreateConnection failed");
    Require(sceNpAsmCreateRequest(&connection, &request, &result) == 0, "sceNpAsmCreateRequest failed");
    Require(sceNpAsmDeleteRequest(&request) == 0, "sceNpAsmDeleteRequest failed");
    Require(sceNpAsmDeleteConnection(&connection) == 0, "sceNpAsmDeleteConnection failed");
    Require(sceNpAsmDeleteConnection(&connection) == 0, "sceNpAsmDeleteConnection was not idempotent");
    Require(sceNpAsmTerminate() == 0, "sceNpAsmTerminate failed");
    Require(sceNpAsmTerminate() == AsmNotInitialized, "sceNpAsmTerminate must reject a second termination");
    int client = 0;
    const char param[4] = {};
    Require(sceNpAsmClientTerminate(&client) == ClientNotInitialized, "sceNpAsmClientTerminate must reject an uninitialized client");
    Require(sceNpAsmClientInitialize(&client, nullptr) == ClientInvalidArgument, "sceNpAsmClientInitialize must reject a null parameter");
    Require(sceNpAsmClientInitialize(&client, param) == 0, "sceNpAsmClientInitialize failed");
    Require(sceNpAsmClientInitialize(&client, nullptr) == ClientAlreadyInitialized, "sceNpAsmClientInitialize must reject a second initialization");
    char titleId[16] = "PPSA00000_00";
    Require(sceNpAsmClientSetNpTitleId(nullptr, titleId) == ClientInvalidArgument, "sceNpAsmClientSetNpTitleId must reject a null client");
    Require(sceNpAsmClientSetNpTitleId(&client, nullptr) == ClientInvalidArgument, "sceNpAsmClientSetNpTitleId must reject a null title id");
    Require(sceNpAsmClientSetNpTitleId(&client, titleId) == 0, "sceNpAsmClientSetNpTitleId failed");
    Require(sceNpAsmClientTerminate(&client) == 0, "sceNpAsmClientTerminate failed");
    Require(sceNpAsmClientSetNpTitleId(&client, titleId) == ClientNotInitialized, "sceNpAsmClientSetNpTitleId must reject a terminated client");
    Require(sceNpAsmClientTerminate(&client) == ClientNotInitialized, "sceNpAsmClientTerminate must reject a second termination");
}

void CheckEmptyDestructors() {
    unsigned char object[0x40];
    unsigned char untouched[sizeof(object)];
    std::memset(object, 0xa5, sizeof(object));
    std::memcpy(untouched, object, sizeof(object));
    for (auto destructor : {_ZN3sce2np10NpOnlineIdD1Ev, _ZN3sce2np10NpOnlineIdD2Ev, _ZN3sce2np12NpTitleTokenD1Ev, _ZN3sce2np12NpTitleTokenD2Ev,
                            _ZN3sce2np13NpAccessTokenD1Ev, _ZN3sce2np13NpAccessTokenD2Ev, _ZN3sce2np4NpIdD1Ev, _ZN3sce2np4NpIdD2Ev,
                            _ZN3sce2np4UserD1Ev, _ZN3sce2np4UserD2Ev}) {
        destructor(object);
        Require(std::memcmp(object, untouched, sizeof(object)) == 0, "an empty destructor wrote the object");
    }
    libSceNpManagerUnknown_3sU9cWXBN98_nid_no_patch();
    libSceNpManagerUnknown_7d3y0wXbeGY_nid_no_patch();
    libSceNpManagerUnknown_vWxlv7_minus_lH78_nid_no_patch();
    libSceNpManagerUnknown_z8eiw8_plus_UfEo_nid_no_patch();
}

int main() {
    CheckNpOfflineGetters();
    CheckAsmLifecycle();
    CheckEmptyDestructors();
    CheckSandboxInfo();
    CheckTicketTeardown();
    CheckPushLifecycle();
    CheckValueClasses();
    CheckUnregisteredClientError();
    CheckAccountQueriesAndPremiumCallback();
    CheckOnlineIdQueries();
    CheckUserIdQueries();
    CheckStateCallbacksA();
    CheckLegacyStateCallback();
    CheckGamePresenceCallbacksA();
    CheckRequestTimeouts();
    CheckAsyncRequestLifecycle();
    CheckWaitAsync();
    CheckIdMapperRequests();
    CheckManagerIntRequests();
    CheckIdMapperLookup("sceNpIdMapperNpIdToAccountId", sceNpIdMapperNpIdToAccountId);
    CheckIdMapperLookup("sceNpIdMapperOnlineIdToAccountId", sceNpIdMapperOnlineIdToAccountId);
    CheckIdMapperLookup("sceNpIdMapperAccountIdToNpId", sceNpIdMapperAccountIdToNpId);
    CheckIdMapperLookup("sceNpIdMapperAccountIdToOnlineId", sceNpIdMapperAccountIdToOnlineId);
    CheckManagerIntProfileGetters();
    CheckManagerIntUserLookups();
    CheckManagerIntAuthorization();
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
    Require(sceNpGetAccountCountryA(InitialUserId, countryCode) == SignedOut, "sceNpGetAccountCountryA must report the user as signed out");
    Require(std::memcmp(countryCode, untouchedCountryCode, sizeof(countryCode)) == 0, "sceNpGetAccountCountryA must leave the country code untouched");
    Require(sceNpGetAccountCountryA(0x10000, countryCode) == UserNotFound, "sceNpGetAccountCountryA must find no user with an id that is not registered");
    Require(std::memcmp(countryCode, untouchedCountryCode, sizeof(countryCode)) == 0, "sceNpGetAccountCountryA must leave the country code untouched for an unknown user");
    Require(sceNpGetAccountCountryA(0x10000, nullptr) == InvalidArgument, "sceNpGetAccountCountryA must reject a null country code");
    NpOnlineId onlineId{};
    std::memset(&onlineId, 0x5a, sizeof(onlineId));
    NpOnlineId untouchedOnlineId{};
    std::memset(&untouchedOnlineId, 0x5a, sizeof(untouchedOnlineId));
    Require(sceNpGetOnlineId(0x10000, &onlineId) == SignedOut, "sceNpGetOnlineId must report the user as signed out");
    Require(std::memcmp(&onlineId, &untouchedOnlineId, sizeof(onlineId)) == 0, "sceNpGetOnlineId must leave the online id untouched");
    Require(sceNpGetOnlineId(0x10000, nullptr) == InvalidArgument, "sceNpGetOnlineId must reject a null online id");
    std::uint64_t accountIdA = 0x5a5a5a5a5a5a5a5aull;
    Require(sceNpGetAccountIdA(0x10000, &accountIdA) == SignedOut, "sceNpGetAccountIdA must report the user as signed out");
    Require(accountIdA == 0x5a5a5a5a5a5a5a5aull, "sceNpGetAccountIdA must leave the account id untouched");
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
    std::uint32_t state = 0x5a5a;
    Require(sceNpGetState(0x10000, &state) == 0 && state == StateSignedOut, "sceNpGetState must report the user as signed out");
    Require(sceNpGetState(0x10000, nullptr) == InvalidArgument, "sceNpGetState must reject a null state");
    bool hasSignedUp = true;
    Require(sceNpHasSignedUp(0x10000, &hasSignedUp) == 0 && !hasSignedUp, "sceNpHasSignedUp must report no sign-up");
    Require(sceNpHasSignedUp(0x10000, nullptr) == InvalidArgument, "sceNpHasSignedUp must reject a null output");
    Require(sceNpCheckNpReachability(1, 0x10000) == SignedOut, "sceNpCheckNpReachability must report the user as signed out");
    NpCheckPremiumParameter premiumParameter{};
    NpCheckPremiumResult premiumResult{};
    std::memset(&premiumResult, 0x5a, sizeof(premiumResult));
    NpCheckPremiumResult untouchedPremiumResult{};
    std::memset(&untouchedPremiumResult, 0x5a, sizeof(untouchedPremiumResult));
    Require(sceNpCheckPremium(1, &premiumParameter, &premiumResult) == SignedOut, "sceNpCheckPremium must report the user as signed out");
    Require(std::memcmp(&premiumResult, &untouchedPremiumResult, sizeof(premiumResult)) == 0, "sceNpCheckPremium must leave the result untouched");
    const int messageContext = sceNpInGameMessageInitialize(0x4000, nullptr);
    Require(messageContext > 0, "sceNpInGameMessageInitialize must return a context id");
    const int otherMessageContext = sceNpInGameMessageInitialize(0x4000, nullptr);
    Require(otherMessageContext > 0 && otherMessageContext != messageContext, "sceNpInGameMessageInitialize must return a new id for each context");
    Require(sceNpInGameMessageCreateHandle(-1) == InGameMessageContextNotFound, "sceNpInGameMessageCreateHandle must reject an unknown context");
    const int messageHandle = sceNpInGameMessageCreateHandle(messageContext);
    Require(messageHandle > 0, "sceNpInGameMessageCreateHandle must return a handle id");
    const int secondMessageHandle = sceNpInGameMessageCreateHandle(messageContext);
    Require(secondMessageHandle > 0 && secondMessageHandle != messageHandle, "sceNpInGameMessageCreateHandle must return a new id for each handle");
    Require(sceNpInGameMessageDeleteHandle(messageContext, secondMessageHandle) == 0, "sceNpInGameMessageDeleteHandle must delete the second handle");
    Require(sceNpInGameMessageDeleteHandle(otherMessageContext, messageHandle) == InGameMessageHandleNotFound, "a handle must belong to the context that created it");
    Require(sceNpInGameMessageAbortHandle(-1, messageHandle) == InGameMessageContextNotFound, "sceNpInGameMessageAbortHandle must reject an unknown context");
    Require(sceNpInGameMessageAbortHandle(messageContext, -1) == InGameMessageHandleNotFound, "sceNpInGameMessageAbortHandle must reject an unknown handle");
    Require(sceNpInGameMessageAbortHandle(messageContext, messageHandle) == 0, "sceNpInGameMessageAbortHandle must accept a live handle");
    std::uint8_t statistics[0x20];
    std::memset(statistics, 0x5a, sizeof(statistics));
    Require(sceNpInGameMessageGetMemoryPoolStatistics(-1, statistics) == InGameMessageContextNotFound, "GetMemoryPoolStatistics must reject an unknown context");
    Require(sceNpInGameMessageGetMemoryPoolStatistics(messageContext, nullptr) == InGameMessageInvalidArgument, "GetMemoryPoolStatistics must reject a null output");
    Require(sceNpInGameMessageGetMemoryPoolStatistics(messageContext, statistics) == 0, "GetMemoryPoolStatistics must answer a live context");
    for (const std::uint8_t byte : statistics) Require(byte == 0, "GetMemoryPoolStatistics must zero the statistics block");
    Require(sceNpInGameMessageCheckCallback() == 0, "CheckCallback must answer without queued messages");
    int callbackUserdata = 0;
    Require(sceNpInGameMessagePrepare(-1, messageHandle, 0, 0x1234, &callbackUserdata) == InGameMessageContextNotFound, "Prepare must reject an unknown context");
    Require(sceNpInGameMessagePrepare(messageContext, -1, 0, 0x1234, &callbackUserdata) == InGameMessageHandleNotFound, "Prepare must reject an unknown handle");
    Require(sceNpInGameMessagePrepare(messageContext, messageHandle, 0, 0, &callbackUserdata) == InGameMessageInvalidArgument, "Prepare must reject a null callback");
    Require(sceNpInGameMessagePrepare(messageContext, messageHandle, 0, 0x1234, &callbackUserdata) == 0, "Prepare must register the callback");
    Require(sceNpInGameMessagePrepareA(-1, messageHandle, 0, 0x1234, &callbackUserdata) == InGameMessageContextNotFound, "PrepareA must reject an unknown context");
    Require(sceNpInGameMessagePrepareA(messageContext, -1, 0, 0x1234, &callbackUserdata) == InGameMessageHandleNotFound, "PrepareA must reject an unknown handle");
    Require(sceNpInGameMessagePrepareA(messageContext, messageHandle, 0, 0, nullptr) == InGameMessageInvalidArgument, "PrepareA must reject a null callback");
    Require(sceNpInGameMessagePrepareA(messageContext, messageHandle, 0, 0x5678, &callbackUserdata) == 0, "PrepareA must register the callback");
    std::uint8_t message[0x88] {};
    *reinterpret_cast<std::uint64_t*>(message + 0x80) = 4;
    std::uint8_t destination[0x18] {};
    std::uint8_t source[0x18] {};
    Require(sceNpInGameMessageSendData(-1, destination, source, message) == InGameMessageContextNotFound, "SendData must reject an unknown context");
    Require(sceNpInGameMessageSendData(messageContext, nullptr, source, message) == InGameMessageInvalidArgument, "SendData must reject a null destination");
    Require(sceNpInGameMessageSendData(messageContext, destination, nullptr, message) == InGameMessageInvalidArgument, "SendData must reject a null source");
    Require(sceNpInGameMessageSendData(messageContext, destination, source, nullptr) == InGameMessageInvalidArgument, "SendData must reject a null message");
    *reinterpret_cast<std::uint64_t*>(message + 0x80) = 0;
    Require(sceNpInGameMessageSendData(messageContext, destination, source, message) == InGameMessageInvalidArgument, "SendData must reject an empty message");
    *reinterpret_cast<std::uint64_t*>(message + 0x80) = 4;
    Require(sceNpInGameMessageSendData(messageContext, destination, source, message) == InGameMessageNotConnected, "SendData must report the offline transport");
    std::uint8_t messageA[0x208] {};
    *reinterpret_cast<std::uint64_t*>(messageA + 0x200) = 4;
    Require(sceNpInGameMessageSendDataA(-1, destination, source, messageA) == InGameMessageContextNotFound, "SendDataA must reject an unknown context");
    Require(sceNpInGameMessageSendDataA(messageContext, nullptr, source, messageA) == InGameMessageInvalidArgument, "SendDataA must reject a null destination");
    *reinterpret_cast<std::uint64_t*>(messageA + 0x200) = 0;
    Require(sceNpInGameMessageSendDataA(messageContext, destination, source, messageA) == InGameMessageInvalidArgument, "SendDataA must reject an empty message");
    *reinterpret_cast<std::uint64_t*>(messageA + 0x200) = 4;
    Require(sceNpInGameMessageSendDataA(messageContext, destination, source, messageA) == InGameMessageNotConnected, "SendDataA must report the offline transport");
    Require(sceNpInGameMessageDeleteHandle(messageContext, messageHandle) == 0, "sceNpInGameMessageDeleteHandle must delete the handle");
    Require(sceNpInGameMessagePrepare(messageContext, messageHandle, 0, 0x1234, nullptr) == InGameMessageHandleNotFound, "Prepare must reject a deleted handle");
    Require(sceNpInGameMessageDeleteHandle(messageContext, messageHandle) == InGameMessageHandleNotFound, "a deleted handle must not be found");
    Require(sceNpInGameMessageTerminate(messageContext) == 0, "sceNpInGameMessageTerminate must end the context");
    Require(sceNpInGameMessageTerminate(messageContext) == InGameMessageContextNotFound, "a terminated context must not be found");
    Require(sceNpInGameMessageCreateHandle(messageContext) == InGameMessageContextNotFound, "a terminated context must not create handles");
    Require(sceNpInGameMessageTerminate(otherMessageContext) == 0, "sceNpInGameMessageTerminate must end the second context");
    std::int32_t nextMessageId = std::numeric_limits<std::int32_t>::max() - 1;
    Require(TakeInGameMessageId(nextMessageId, "ids") == std::numeric_limits<std::int32_t>::max() - 1, "the last in-game message id below the limit must be handed out");
    try {
        TakeInGameMessageId(nextMessageId, "ids");
        Require(false, "the in-game message ids must not pass the 32-bit limit");
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()) == "ids: the in-game message ids are exhausted", "the exhausted in-game message ids threw");
    }
    Require(nextMessageId == std::numeric_limits<std::int32_t>::max(), "an exhausted in-game message id must not wrap");
    return 0;
}
