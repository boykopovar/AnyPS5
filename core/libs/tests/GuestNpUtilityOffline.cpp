#include "prx/libSceNpUtility/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

constexpr int SignedOut = static_cast<int>(0x80550006u);
constexpr int InvalidArgument = static_cast<int>(0x80550704u);
constexpr int TooManyObjects = static_cast<int>(0x80550706u);
constexpr int Aborted = static_cast<int>(0x80550707u);
constexpr int InsufficientArgument = static_cast<int>(0x8055070Cu);
constexpr int InvalidId = static_cast<int>(0x8055070Eu);
constexpr int InvalidType = static_cast<int>(0x80550711u);
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

}

int main() {
    std::memset(selfNpId, 0, sizeof(selfNpId));
    std::strcpy(reinterpret_cast<char*>(selfNpId), "player_1");
    CheckLookup();
    CheckWordFilter();
    if (failures != 0) return 1;
    std::puts("NpUtility offline tests passed");
    return 0;
}
