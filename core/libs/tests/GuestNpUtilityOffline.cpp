#include "prx/libSceNpUtility/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <chrono>
#include <thread>

namespace {

constexpr int SignedOut = static_cast<int>(0x80550006u);
constexpr int InvalidArgument = static_cast<int>(0x80550704u);
constexpr int TooManyObjects = static_cast<int>(0x80550706u);
constexpr int Aborted = static_cast<int>(0x80550707u);
constexpr int InsufficientArgument = static_cast<int>(0x8055070Cu);
constexpr int InvalidId = static_cast<int>(0x8055070Eu);
constexpr int InvalidType = static_cast<int>(0x80550711u);
constexpr int LookupSynchronousRequest = static_cast<int>(0x80550307u);
constexpr int PollFinished = 0;
constexpr int PollRunning = 1;
constexpr int WordFilterTitleCtxBase = 0x01000000;
constexpr int WordFilterRequestBase = 0x02000000;

int failures = 0;
unsigned char selfNpId[36];
unsigned char npId[36];
char sanitized[0x402];
char longComment[0x402];
const std::uint64_t asyncParam[3] = {0x18, 0, 0};
const std::uint64_t wrongAsyncParam[3] = {0x10, 0, 0};

void Require(bool value, const char* name, const char* what) {
    if (value) return;
    std::fprintf(stderr, "%s: %s\n", name, what);
    ++failures;
}

void CheckLookup() {
    NpOnlineId onlineId{};
    std::strcpy(onlineId.data, "player_1");
    Require(sceNpLookupCreateTitleCtx(nullptr) == InsufficientArgument, "sceNpLookupCreateTitleCtx", "a null NpId was taken");
    const int context = sceNpLookupCreateTitleCtx(selfNpId);
    Require(context == 1, "sceNpLookupCreateTitleCtx", "the first lookup context is not id 1");
    Require(sceNpLookupCreateRequest(context + 100) == InvalidId, "sceNpLookupCreateRequest", "an unknown context was taken");
    const int req = sceNpLookupCreateRequest(context);
    Require(req == 1, "sceNpLookupCreateRequest", "the first lookup request is not id 1");
    std::memset(npId, 0xA5, sizeof(npId));
    Require(sceNpLookupNpId(req, nullptr, nullptr, &onlineId) == InvalidArgument, "sceNpLookupNpId", "an option was not refused before the null arguments");
    Require(sceNpLookupNpId(req, nullptr, npId, nullptr) == InsufficientArgument, "sceNpLookupNpId", "a null online ID was taken");
    Require(sceNpLookupNpId(req, &onlineId, nullptr, nullptr) == InsufficientArgument, "sceNpLookupNpId", "a null NpId output was taken");
    Require(sceNpLookupNpId(req + 100, &onlineId, npId, nullptr) == InvalidId, "sceNpLookupNpId", "an unknown request was taken");
    Require(sceNpLookupNpId(req, &onlineId, npId, nullptr) == SignedOut, "sceNpLookupNpId", "the lookup did not return SIGNED_OUT");
    Require(sceNpLookupNpId(req, &onlineId, npId, nullptr) == InvalidType, "sceNpLookupNpId", "a used request did not return INVALID_TYPE");
    Require(npId[0] == 0xA5 && npId[35] == 0xA5, "sceNpLookupNpId", "an NpId was written");
    Require(sceNpLookupDeleteRequest(req) == 0, "sceNpLookupDeleteRequest", "deleting the request failed");
    Require(sceNpLookupDeleteRequest(req) == InvalidId, "sceNpLookupDeleteRequest", "a deleted request was found");
    int requests[32];
    for (int& request : requests) request = sceNpLookupCreateRequest(context);
    Require(requests[31] > 0, "sceNpLookupCreateRequest", "32 requests could not be created");
    Require(sceNpLookupCreateRequest(context) == TooManyObjects, "sceNpLookupCreateRequest", "a 33rd request did not return TOO_MANY_OBJECTS");
    Require(sceNpLookupDeleteTitleCtx(context) == 0, "sceNpLookupDeleteTitleCtx", "deleting the context failed");
    Require(sceNpLookupDeleteRequest(requests[0]) == InvalidId, "sceNpLookupDeleteTitleCtx", "a request of the deleted context was kept");
    Require(sceNpLookupDeleteTitleCtx(context) == InvalidId, "sceNpLookupDeleteTitleCtx", "a deleted context was found");
    int contexts[32];
    for (int& entry : contexts) entry = sceNpLookupCreateTitleCtx(selfNpId);
    Require(contexts[0] == 2 && contexts[31] == 33, "sceNpLookupCreateTitleCtx", "lookup context ids do not keep counting from 2");
    Require(sceNpLookupCreateTitleCtx(selfNpId) == TooManyObjects, "sceNpLookupCreateTitleCtx", "a 33rd context did not return TOO_MANY_OBJECTS");
    for (const int entry : contexts) sceNpLookupDeleteTitleCtx(entry);
}

void CheckWordFilter() {
    Require(sceNpWordFilterCreateTitleCtx(nullptr) == InsufficientArgument, "sceNpWordFilterCreateTitleCtx", "a null NpId was taken");
    const int context = sceNpWordFilterCreateTitleCtx(selfNpId);
    Require(context == WordFilterTitleCtxBase + 1, "sceNpWordFilterCreateTitleCtx", "the first word filter context is not 0x01000001");
    Require(sceNpWordFilterCreateAsyncRequest(context, nullptr) == InvalidArgument, "sceNpWordFilterCreateAsyncRequest", "a null parameter was taken");
    Require(sceNpWordFilterCreateAsyncRequest(context, wrongAsyncParam) == InvalidArgument, "sceNpWordFilterCreateAsyncRequest", "a parameter size other than 0x18 was taken");
    Require(sceNpWordFilterCreateAsyncRequest(context + 100, asyncParam) == InvalidId, "sceNpWordFilterCreateAsyncRequest", "an unknown context was taken");
    const int sync = sceNpWordFilterCreateRequest(context);
    Require(sync == WordFilterRequestBase + 1, "sceNpWordFilterCreateRequest", "the first word filter request is not 0x02000001");
    std::memset(longComment, 'a', 0x400);
    longComment[0x400] = 0;
    Require(sceNpWordFilterCensorComment(sync, nullptr, &sanitized) == InsufficientArgument, "sceNpWordFilterCensorComment", "a null comment was not refused before the option");
    Require(sceNpWordFilterCensorComment(sync, "word", &sanitized) == InvalidArgument, "sceNpWordFilterCensorComment", "an option was taken");
    longComment[0x400] = 'a';
    Require(sceNpWordFilterCensorComment(sync, longComment, nullptr) == InvalidArgument, "sceNpWordFilterCensorComment", "a 0x401-byte comment was taken");
    Require(sceNpWordFilterCensorComment(sync + 100, "word", nullptr) == InvalidId, "sceNpWordFilterCensorComment", "an unknown request was taken");
    longComment[0x400] = 0;
    Require(sceNpWordFilterCensorComment(sync, longComment, nullptr) == SignedOut, "sceNpWordFilterCensorComment", "a 0x400-byte comment did not return SIGNED_OUT");
    Require(sceNpWordFilterCensorComment(sync, "word", nullptr) == InvalidType, "sceNpWordFilterCensorComment", "a used request did not return INVALID_TYPE");
    int result = 7;
    Require(sceNpWordFilterPollAsync(sync, &result) == InvalidId && result == 7, "sceNpWordFilterPollAsync", "a synchronous request was polled");
    Require(sceNpWordFilterDeleteRequest(sync) == 0, "sceNpWordFilterDeleteRequest", "deleting the request failed");

    const int async = sceNpWordFilterCreateAsyncRequest(context, asyncParam);
    Require(async == WordFilterRequestBase + 2, "sceNpWordFilterCreateAsyncRequest", "the asynchronous request did not take the next id");
    Require(sceNpWordFilterPollAsync(async, nullptr) == InvalidArgument, "sceNpWordFilterPollAsync", "a null result was taken");
    Require(sceNpWordFilterPollAsync(async + 100, &result) == InvalidId, "sceNpWordFilterPollAsync", "an unknown request was taken");
    Require(sceNpWordFilterPollAsync(async, &result) == PollRunning && result == 7, "sceNpWordFilterPollAsync", "an unstarted request was not running");
    std::memset(sanitized, 'z', sizeof(sanitized));
    Require(sceNpWordFilterSanitizeComment(async, "word", nullptr, nullptr) == InsufficientArgument, "sceNpWordFilterSanitizeComment", "a null output was taken");
    Require(sceNpWordFilterSanitizeComment(async, nullptr, sanitized, &result) == InsufficientArgument, "sceNpWordFilterSanitizeComment", "a null comment was not refused before the option");
    Require(sceNpWordFilterSanitizeComment(async, "word", sanitized, &result) == InvalidArgument, "sceNpWordFilterSanitizeComment", "an option was taken");
    Require(sceNpWordFilterSanitizeComment(async, "word", sanitized, nullptr) == 0, "sceNpWordFilterSanitizeComment", "the asynchronous request did not start");
    Require(sceNpWordFilterPollAsync(async, &result) == PollFinished && result == SignedOut, "sceNpWordFilterPollAsync", "the request did not finish with SIGNED_OUT");
    Require(sanitized[0] == 'z' && sanitized[4] == 'z', "sceNpWordFilterSanitizeComment", "a sanitized comment was written");
    Require(sceNpWordFilterAbortRequest(async) == 0, "sceNpWordFilterAbortRequest", "aborting a finished request failed");
    Require(sceNpWordFilterPollAsync(async, &result) == PollFinished && result == SignedOut, "sceNpWordFilterAbortRequest", "aborting a finished request changed its result");

    const int aborted = sceNpWordFilterCreateAsyncRequest(context, asyncParam);
    Require(sceNpWordFilterAbortRequest(aborted + 100) == InvalidId, "sceNpWordFilterAbortRequest", "an unknown request was taken");
    Require(sceNpWordFilterAbortRequest(aborted) == 0, "sceNpWordFilterAbortRequest", "aborting failed");
    Require(sceNpWordFilterPollAsync(aborted, &result) == PollFinished && result == Aborted, "sceNpWordFilterAbortRequest", "an aborted request did not finish with ABORTED");
    Require(sceNpWordFilterCensorComment(aborted, "word", nullptr) == Aborted, "sceNpWordFilterCensorComment", "an aborted request did not return ABORTED");

    const int kept = sceNpWordFilterCreateRequest(context);
    Require(sceNpWordFilterDeleteTitleCtx(context) == 0, "sceNpWordFilterDeleteTitleCtx", "deleting the context failed");
    Require(sceNpWordFilterDeleteRequest(kept) == InvalidId, "sceNpWordFilterDeleteTitleCtx", "a request of the deleted context was kept");
    Require(sceNpWordFilterDeleteTitleCtx(context) == InvalidId, "sceNpWordFilterDeleteTitleCtx", "a deleted context was found");
    Require(sceNpLookupDeleteTitleCtx(context) == InvalidId, "sceNpLookupDeleteTitleCtx", "a word filter context was found among the lookup contexts");
}

void CheckBandwidthTestInitStart() {
    Require(sceNpBandwidthTestInitStart() == SignedOut, "sceNpBandwidthTestInitStart", "starting a test while signed out did not return SIGNED_OUT");
    Require(sceNpBandwidthTestInitStart() == sceNpBandwidthTestInitStartDownload(nullptr), "sceNpBandwidthTestInitStart", "the answer differs from sceNpBandwidthTestInitStartDownload");
    Require(sceNpBandwidthTestInitStart() == sceNpBandwidthTestInitStartUpload(nullptr), "sceNpBandwidthTestInitStart", "the answer differs from sceNpBandwidthTestInitStartUpload");
}

void CheckBandwidthTestShutdownWithDetailedInfo() {
    constexpr int npInvalidArgument = static_cast<int>(0x80550003u);
    Require(sceNpBandwidthTestShutdownWithDetailedInfo() == npInvalidArgument, "sceNpBandwidthTestShutdownWithDetailedInfo", "shutting down a test that never started did not return INVALID_ARGUMENT");
    Require(sceNpBandwidthTestShutdownWithDetailedInfo() == sceNpBandwidthTestShutdown(0, nullptr), "sceNpBandwidthTestShutdownWithDetailedInfo", "the answer differs from sceNpBandwidthTestShutdown");
    Require(sceNpBandwidthTestAbort(0) == npInvalidArgument, "sceNpBandwidthTestAbort", "aborting a test that never started did not return INVALID_ARGUMENT");
    int status = -1;
    Require(sceNpBandwidthTestGetStatus(0, &status) == npInvalidArgument, "sceNpBandwidthTestGetStatus", "the status of a test that never started did not return INVALID_ARGUMENT");
    Require(status == 0, "sceNpBandwidthTestGetStatus", "the status of a test that never started was not cleared to 0");
    Require(sceNpBandwidthTestGetStatus(0, nullptr) == npInvalidArgument, "sceNpBandwidthTestGetStatus", "a null status did not return INVALID_ARGUMENT");
}

}

int WaitInThread(const char* name, int (APS5_VABI *wait)(int, int*), int reqId, int* result, void (*finish)(int)) {
    int status = 7;
    std::atomic<bool> entered{false};
    std::atomic<bool> returned{false};
    std::thread waiter([&] {
        entered = true;
        status = wait(reqId, result);
        returned = true;
    });
    while (!entered) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    Require(!returned, name, "the wait returned before the request finished");
    finish(reqId);
    waiter.join();
    return status;
}

void StartLookup(int reqId) {
    NpOnlineId onlineId{};
    std::strcpy(onlineId.data, "player_2");
    sceNpLookupNpId(reqId, &onlineId, npId, nullptr);
}

void AbortLookup(int reqId) {
    sceNpLookupAbortRequest(reqId);
}

void DeleteLookup(int reqId) {
    sceNpLookupDeleteRequest(reqId);
}

void CheckLookupAsync() {
    const int context = sceNpLookupCreateTitleCtx(selfNpId);
    Require(sceNpLookupCreateAsyncRequest(context, nullptr) == InvalidArgument, "sceNpLookupCreateAsyncRequest", "a null parameter was taken");
    Require(sceNpLookupCreateAsyncRequest(context, wrongAsyncParam) == InvalidArgument, "sceNpLookupCreateAsyncRequest", "a parameter size other than 0x18 was taken");
    Require(sceNpLookupCreateAsyncRequest(context + 100, asyncParam) == InvalidId, "sceNpLookupCreateAsyncRequest", "an unknown context was taken");

    Require(sceNpLookupSetTimeout(context, -1, 0, 0, 0, 0) == InvalidArgument, "sceNpLookupSetTimeout", "a negative retry count was taken");
    Require(sceNpLookupSetTimeout(context, 0, 0, 0, 0, 0) == InvalidArgument, "sceNpLookupSetTimeout", "settings without any effect were taken");
    Require(sceNpLookupSetTimeout(context, 0, 999999, 0, 0, 0) == InvalidArgument, "sceNpLookupSetTimeout", "a resolve timeout under 1 s was taken");
    Require(sceNpLookupSetTimeout(context, 0, 0, 9999999, 0, 0) == InvalidArgument, "sceNpLookupSetTimeout", "a connect timeout under 10 s was taken");
    Require(sceNpLookupSetTimeout(context, 0, 0, 0, 9999999, 0) == InvalidArgument, "sceNpLookupSetTimeout", "a send timeout under 10 s was taken");
    Require(sceNpLookupSetTimeout(context, 0, 0, 0, 0, 9999999) == InvalidArgument, "sceNpLookupSetTimeout", "a receive timeout under 10 s was taken");
    Require(sceNpLookupSetTimeout(context + 100, 1, 0, 0, 0, 0) == InvalidId, "sceNpLookupSetTimeout", "an unknown id was taken");
    Require(sceNpLookupSetTimeout(context, 1, 0, 0, 0, 0) == 0, "sceNpLookupSetTimeout", "a context timeout was refused");

    const int async = sceNpLookupCreateAsyncRequest(context, asyncParam);
    Require(async > 0, "sceNpLookupCreateAsyncRequest", "creating an asynchronous request failed");
    Require(sceNpLookupSetTimeout(async, 0, 1000000, 10000000, 10000000, 10000000) == 0, "sceNpLookupSetTimeout", "a request timeout was refused");
    int result = 7;
    Require(sceNpLookupPollAsync(async + 100, &result) == InvalidId, "sceNpLookupPollAsync", "an unknown request was taken");
    Require(sceNpLookupPollAsync(async, nullptr) == InvalidArgument, "sceNpLookupPollAsync", "a null result was taken");
    Require(sceNpLookupPollAsync(async + 100, nullptr) == InvalidArgument, "sceNpLookupPollAsync", "an unknown request was checked before a null result");
    Require(sceNpLookupWaitAsync(async, nullptr) == InvalidArgument, "sceNpLookupWaitAsync", "a null result was taken");
    Require(sceNpLookupPollAsync(async, &result) == PollRunning && result == 7, "sceNpLookupPollAsync", "an unstarted request was not running");
    Require(WaitInThread("sceNpLookupWaitAsync", sceNpLookupWaitAsync, async, &result, StartLookup) == 0 && result == SignedOut, "sceNpLookupWaitAsync", "waiting did not end with SIGNED_OUT when the lookup started");
    Require(sceNpLookupPollAsync(async, nullptr) == InvalidArgument, "sceNpLookupPollAsync", "a null result was taken on a finished request");
    result = 7;
    Require(sceNpLookupPollAsync(async, &result) == PollFinished && result == SignedOut, "sceNpLookupPollAsync", "the request did not finish with SIGNED_OUT");

    const int sync = sceNpLookupCreateRequest(context);
    StartLookup(sync);
    result = 7;
    Require(sceNpLookupWaitAsync(sync, &result) == LookupSynchronousRequest && result == 7, "sceNpLookupWaitAsync", "a synchronous request was not refused with 0x80550307");
    Require(sceNpLookupPollAsync(sync, &result) == LookupSynchronousRequest && result == 7, "sceNpLookupPollAsync", "a synchronous request was not refused with 0x80550307");

    const int aborted = sceNpLookupCreateAsyncRequest(context, asyncParam);
    Require(sceNpLookupAbortRequest(aborted + 100) == InvalidId, "sceNpLookupAbortRequest", "an unknown request was taken");
    Require(WaitInThread("sceNpLookupWaitAsync", sceNpLookupWaitAsync, aborted, &result, AbortLookup) == 0 && result == Aborted, "sceNpLookupAbortRequest", "waiting did not end with ABORTED");
    Require(sceNpLookupNpId(aborted, nullptr, npId, nullptr) == InsufficientArgument, "sceNpLookupNpId", "arguments were not checked before the abort");

    const int deleted = sceNpLookupCreateAsyncRequest(context, asyncParam);
    Require(WaitInThread("sceNpLookupWaitAsync", sceNpLookupWaitAsync, deleted, &result, DeleteLookup) == InvalidId, "sceNpLookupWaitAsync", "waiting on a deleted request did not return INVALID_ID");
    Require(sceNpLookupWaitAsync(deleted, &result) == InvalidId, "sceNpLookupWaitAsync", "an unknown request was taken");
    sceNpLookupDeleteTitleCtx(context);
}

void CheckWordFilterAsync() {
    const int context = sceNpWordFilterCreateTitleCtx(selfNpId);
    Require(sceNpWordFilterSetTimeout(context, 0, 0, 0, 0, 0) == InvalidArgument, "sceNpWordFilterSetTimeout", "settings without any effect were taken");
    Require(sceNpWordFilterSetTimeout(context, 0, 0, 0, 0, 9999999) == InvalidArgument, "sceNpWordFilterSetTimeout", "a receive timeout under 10 s was taken");
    Require(sceNpWordFilterSetTimeout(context, 1, 0, 0, 0, 0) == 0, "sceNpWordFilterSetTimeout", "a context timeout was refused");
    Require(sceNpWordFilterSetTimeout(1, 1, 0, 0, 0, 0) == InvalidId, "sceNpWordFilterSetTimeout", "a lookup id was taken");
    const int sync = sceNpWordFilterCreateRequest(context);
    const int async = sceNpWordFilterCreateAsyncRequest(context, asyncParam);
    Require(sceNpWordFilterSetTimeout(async, 2, 1000000, 0, 0, 0) == 0, "sceNpWordFilterSetTimeout", "a request timeout was refused");
    int result = 7;
    Require(sceNpWordFilterWaitAsync(async, nullptr) == InvalidArgument, "sceNpWordFilterWaitAsync", "a null result was taken");
    Require(sceNpWordFilterWaitAsync(sync, &result) == InvalidId, "sceNpWordFilterWaitAsync", "a synchronous request was taken");
    Require(sceNpWordFilterWaitAsync(async + 100, &result) == InvalidId, "sceNpWordFilterWaitAsync", "an unknown request was taken");
    Require(sceNpWordFilterCensorComment(async, "word", nullptr) == 0, "sceNpWordFilterCensorComment", "the asynchronous request did not start");
    Require(sceNpWordFilterWaitAsync(async, &result) == 0 && result == SignedOut, "sceNpWordFilterWaitAsync", "the request did not finish with SIGNED_OUT");
    sceNpWordFilterDeleteTitleCtx(context);
}

void CheckTitleContextsA() {
    int lookups[32];
    int created = 0;
    while (created < 32) {
        const int context = sceNpLookupCreateTitleCtxA(created == 0 ? -1 : 1);
        if (context == TooManyObjects) break;
        Require(context > 0 && context < WordFilterTitleCtxBase, "sceNpLookupCreateTitleCtxA", "a lookup context id was not returned");
        lookups[created++] = context;
    }
    Require(created > 0, "sceNpLookupCreateTitleCtxA", "no context was created");
    Require(sceNpLookupCreateTitleCtxA(1) == TooManyObjects, "sceNpLookupCreateTitleCtxA", "the 32-context limit was not applied");
    Require(sceNpLookupCreateTitleCtx(selfNpId) == TooManyObjects, "sceNpLookupCreateTitleCtxA", "the A contexts do not share the lookup limit");
    for (int index = 0; index < created; ++index) Require(sceNpLookupDeleteTitleCtx(lookups[index]) == 0, "sceNpLookupCreateTitleCtxA", "an A context could not be deleted");

    const int filter = sceNpWordFilterCreateTitleCtxA(-1);
    Require(filter > WordFilterTitleCtxBase, "sceNpWordFilterCreateTitleCtxA", "a word filter context id was not returned");
    Require(sceNpLookupDeleteTitleCtx(filter) == InvalidId, "sceNpWordFilterCreateTitleCtxA", "the context was created among the lookup contexts");
    Require(sceNpWordFilterDeleteTitleCtx(filter) == 0, "sceNpWordFilterCreateTitleCtxA", "the context could not be deleted");
}

int main() {
    std::memset(selfNpId, 0, sizeof(selfNpId));
    std::strcpy(reinterpret_cast<char*>(selfNpId), "player_1");
    CheckBandwidthTestInitStart();
    CheckBandwidthTestShutdownWithDetailedInfo();
    CheckLookup();
    CheckWordFilter();
    CheckLookupAsync();
    CheckWordFilterAsync();
    CheckTitleContextsA();
    if (failures != 0) return 1;
    std::puts("NpUtility offline tests passed");
    return 0;
}
