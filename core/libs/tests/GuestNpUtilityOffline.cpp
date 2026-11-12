#include "prx/libSceNpUtility/include/Api.hpp"
#include "SceTypes.hpp"
#include <cerrno>
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
    errno = 71;
    Require(sceNpWordFilterPollAsync(async, reinterpret_cast<int*>(0x10)) == InvalidArgument, "sceNpWordFilterPollAsync", "unwritable result was taken");
    Require(errno == 71, "sceNpWordFilterPollAsync", "errno was modified");
    errno = 71;
    Require(sceNpWordFilterWaitAsync(async, reinterpret_cast<int*>(0x10)) == InvalidArgument, "sceNpWordFilterWaitAsync", "unwritable result was taken");
    Require(errno == 71, "sceNpWordFilterWaitAsync", "errno was modified");
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

template <typename Init, typename Finalize, typename Create, typename Abort, typename Destroy, typename GetNum, typename GetList>
void CheckAppLaunchLink(const char* name, Init init, Finalize finalize, Create create, Abort abort, Destroy destroy, GetNum getNum, GetList getList, int notInitialized, int requestNotFound, int invalidArgument) {
    const int param[4] = {};
    std::int32_t request = 0;
    Require(init(0) == 0, name, "initialization failed");
    Require(create(-1, param, &request) == invalidArgument, name, "user -1 was taken");
    Require(create(1, nullptr, &request) == invalidArgument, name, "a null parameter was taken");
    Require(create(1, param, nullptr) == invalidArgument, name, "a null output was taken");
    Require(create(1, param, &request) == 0 && request == 1, name, "the first request is not id 1");
    std::int32_t num = -1;
    Require(getNum(request, &num) == 0 && num == 0, name, "the compatible title count was not zero");
    Require(getNum(request, nullptr) == invalidArgument, name, "a null count was taken");
    Require(getNum(request + 100, &num) == requestNotFound, name, "an unknown request was taken");
    std::int32_t list[4] = {};
    Require(getList(request, list, &num) == 0 && num == 0, name, "the compatible title list was not empty");
    Require(getList(request, nullptr, &num) == invalidArgument, name, "a null list with a count was taken");
    Require(abort(request) == 0, name, "aborting the request failed");
    Require(abort(request) == requestNotFound, name, "an aborted request was found");
    Require(create(1, param, &request) == 0 && request == 2, name, "the second request is not id 2");
    Require(destroy(request) == 0, name, "destroying the request failed");
    Require(getNum(request, &num) == requestNotFound, name, "a destroyed request was found");
    finalize();
    Require(create(1, param, &request) == notInitialized, name, "a request was created after finalization");
}

void CheckNpUtilityPayloads() {
    constexpr int AppInfoNotInitialized = static_cast<int>(0x8055B202u);
    constexpr int AppInfoInvalid = static_cast<int>(0x8055B204u);
    constexpr int LaunchNotInitialized = static_cast<int>(0x8055AE02u);
    constexpr int LaunchNotFound = static_cast<int>(0x8055AE06u);
    constexpr int LaunchInvalid = static_cast<int>(0x8055AE0Cu);
    constexpr int Launch2NotInitialized = static_cast<int>(0x8055C602u);
    constexpr int Launch2NotFound = static_cast<int>(0x8055C606u);
    constexpr int Launch2Invalid = static_cast<int>(0x8055C60Cu);
    constexpr int ServiceNotInitialized = static_cast<int>(0x8055AD02u);
    constexpr int ServiceNotFound = static_cast<int>(0x8055AD06u);
    constexpr int ServiceInvalid = static_cast<int>(0x8055AD0Bu);
    std::int32_t request = 0;
    Require(sceNpAppInfoIntCreateRequest(&request) == AppInfoNotInitialized, "sceNpAppInfoIntCreateRequest", "a request was created before initialization");
    Require(sceNpAppInfoIntInitialize(0) == 0, "sceNpAppInfoIntInitialize", "initialization failed");
    Require(sceNpAppInfoIntCreateRequest(nullptr) == AppInfoInvalid, "sceNpAppInfoIntCreateRequest", "a null output was taken");
    Require(sceNpAppInfoIntCreateRequest(&request) == 0 && request == 1, "sceNpAppInfoIntCreateRequest", "the first request is not id 1");
    Require(sceNpAppInfoIntAbortRequest() == 0, "sceNpAppInfoIntAbortRequest", "aborting the request failed");
    Require(sceNpAppInfoIntAbortRequest() == AppInfoInvalid, "sceNpAppInfoIntAbortRequest", "an aborted request was found");
    Require(sceNpAppInfoIntCreateRequest(&request) == 0 && request == 2, "sceNpAppInfoIntCreateRequest", "the second request is not id 2");
    Require(sceNpAppInfoIntDestroyRequest() == 0, "sceNpAppInfoIntDestroyRequest", "destroying the request failed");
    Require(sceNpAppInfoIntAbortRequest() == AppInfoInvalid, "sceNpAppInfoIntAbortRequest", "a destroyed request was found");
    sceNpAppInfoIntFinalize();
    Require(sceNpAppInfoIntCreateRequest(&request) == AppInfoNotInitialized, "sceNpAppInfoIntCreateRequest", "a request was created after finalization");
    char npTitleId[16] = {};
    Require(sceNpTitleMetadataIntGetNpTitleId(nullptr, 0, npTitleId) == static_cast<int>(0x8055AA03u), "sceNpTitleMetadataIntGetNpTitleId", "a null title id was taken");
    Require(sceNpTitleMetadataIntGetNpTitleId("PPSA00000", 9, nullptr) == static_cast<int>(0x8055AA03u), "sceNpTitleMetadataIntGetNpTitleId", "a null output was taken");
    Require(sceNpTitleMetadataIntGetNpTitleId("PPSA000000", 10, npTitleId) == static_cast<int>(0x8055AA04u), "sceNpTitleMetadataIntGetNpTitleId", "a title id above 9 bytes was taken");
    Require(sceNpTitleMetadataIntGetNpTitleId("PPSA00000", 9, npTitleId) == 0 && std::strcmp(npTitleId, "PPSA00000_00") == 0, "sceNpTitleMetadataIntGetNpTitleId", "the np title id was not built");
    const char embeddedTitle[] = {'A', 'B', 0, 'C'};
    Require(sceNpTitleMetadataIntGetNpTitleId(embeddedTitle, 4, npTitleId) == 0 && std::strcmp(npTitleId, "AB_00") == 0, "sceNpTitleMetadataIntGetNpTitleId", "an embedded terminator was not honoured");
    CheckAppLaunchLink("sceNpAppLaunchLink", sceNpAppLaunchLinkIntInitialize, sceNpAppLaunchLinkIntFinalize, sceNpAppLaunchLinkIntCreateRequest, sceNpAppLaunchLinkIntAbortRequest, sceNpAppLaunchLinkIntDestroyRequest, sceNpAppLaunchLinkIntGetCompatibleTitleIdNum, sceNpAppLaunchLinkIntGetCompatibleTitleIdList, LaunchNotInitialized, LaunchNotFound, LaunchInvalid);
    CheckAppLaunchLink("sceNpAppLaunchLink2", sceNpAppLaunchLink2IntInitialize, sceNpAppLaunchLink2IntFinalize, sceNpAppLaunchLink2IntCreateRequest, sceNpAppLaunchLink2IntAbortRequest, sceNpAppLaunchLink2IntDestroyRequest, sceNpAppLaunchLink2IntGetCompatibleTitleIdNum, sceNpAppLaunchLink2IntGetCompatibleTitleIdList, Launch2NotInitialized, Launch2NotFound, Launch2Invalid);
    Require(sceNpServiceCheckerIntInitialize(0) == 0, "sceNpServiceCheckerIntInitialize", "initialization failed");
    Require(sceNpServiceCheckerIntCreateRequest(selfNpId, &request) == 0 && request == 1, "sceNpServiceCheckerIntCreateRequest", "the first request is not id 1");
    std::uint32_t availability = 0xA5A5A5A5;
    Require(sceNpServiceCheckerIntGetAvailability(request, nullptr, &availability) == ServiceInvalid, "sceNpServiceCheckerIntGetAvailability", "a null NpId was taken");
    Require(sceNpServiceCheckerIntGetAvailability(request, selfNpId, nullptr) == ServiceInvalid, "sceNpServiceCheckerIntGetAvailability", "a null result was taken");
    Require(sceNpServiceCheckerIntGetAvailability(request + 100, selfNpId, &availability) == ServiceNotFound, "sceNpServiceCheckerIntGetAvailability", "an unknown request was taken");
    Require(sceNpServiceCheckerIntGetAvailability(request, selfNpId, &availability) == SignedOut, "sceNpServiceCheckerIntGetAvailability", "the availability did not return SIGNED_OUT");
    const void* npIds[2] = {selfNpId, selfNpId};
    std::uint32_t results[2] = {0xA5A5A5A5, 0xA5A5A5A5};
    Require(sceNpServiceCheckerIntGetAvailabilityList(request, npIds, 2, results) == SignedOut, "sceNpServiceCheckerIntGetAvailabilityList", "the list did not return SIGNED_OUT");
    Require(sceNpServiceCheckerIntGetAvailabilityList(request, nullptr, 0, nullptr) == 0, "sceNpServiceCheckerIntGetAvailabilityList", "an empty list was refused");
    Require(sceNpServiceCheckerIntGetAvailabilityList(request, nullptr, 1, results) == ServiceInvalid, "sceNpServiceCheckerIntGetAvailabilityList", "a null list with a count was taken");
    std::uint8_t cached = 0xA5;
    Require(sceNpServiceCheckerIntIsCached(request, &cached) == 0 && cached == 0, "sceNpServiceCheckerIntIsCached", "the request was reported cached");
    Require(sceNpServiceCheckerIntIsCached(request, nullptr) == ServiceInvalid, "sceNpServiceCheckerIntIsCached", "a null output was taken");
    Require(sceNpServiceCheckerIntIsCached(request + 100, &cached) == ServiceNotFound, "sceNpServiceCheckerIntIsCached", "an unknown request was taken");
    sceNpServiceCheckerIntFinalize();
    Require(sceNpServiceCheckerIntGetAvailability(request, selfNpId, &availability) == ServiceNotInitialized, "sceNpServiceCheckerIntGetAvailability", "availability was checked after finalization");
}

void CheckNpUtilityLifecycles() {
    constexpr int AppInfoAlreadyInitialized = static_cast<int>(0x8055B201u);
    constexpr int AppLaunchLinkAlreadyInitialized = static_cast<int>(0x8055AE01u);
    constexpr int AppLaunchLink2AlreadyInitialized = static_cast<int>(0x8055C601u);
    constexpr int ServiceCheckerAlreadyInitialized = static_cast<int>(0x8055AD01u);
    constexpr int ServiceCheckerNotInitialized = static_cast<int>(0x8055AD02u);
    constexpr int ServiceCheckerRequestNotFound = static_cast<int>(0x8055AD06u);
    constexpr int ServiceCheckerInvalidArgument = static_cast<int>(0x8055AD0Bu);
    Require(sceNpUtilityInit() == 0, "sceNpUtilityInit", "utility initialization failed");
    Require(sceNpUtilityTerm() == 0, "sceNpUtilityTerm", "utility termination failed");
    Require(sceNpAppInfoIntInitialize(0) == 0, "sceNpAppInfoIntInitialize", "initialization failed");
    Require(sceNpAppInfoIntInitialize(0) == AppInfoAlreadyInitialized, "sceNpAppInfoIntInitialize", "a second initialization was taken");
    sceNpAppInfoIntFinalize();
    Require(sceNpAppInfoIntInitialize(0) == 0, "sceNpAppInfoIntInitialize", "reinitialization failed");
    sceNpAppInfoIntFinalize();
    Require(sceNpAppLaunchLinkIntInitialize(0) == 0, "sceNpAppLaunchLinkIntInitialize", "initialization failed");
    Require(sceNpAppLaunchLinkIntInitialize(0) == AppLaunchLinkAlreadyInitialized, "sceNpAppLaunchLinkIntInitialize", "a second initialization was taken");
    sceNpAppLaunchLinkIntFinalize();
    Require(sceNpAppLaunchLink2IntInitialize(0) == 0, "sceNpAppLaunchLink2IntInitialize", "initialization failed");
    Require(sceNpAppLaunchLink2IntInitialize(0) == AppLaunchLink2AlreadyInitialized, "sceNpAppLaunchLink2IntInitialize", "a second initialization was taken");
    sceNpAppLaunchLink2IntFinalize();
    std::int32_t request = 0;
    Require(sceNpServiceCheckerIntCreateRequest(nullptr, &request) == ServiceCheckerInvalidArgument, "sceNpServiceCheckerIntCreateRequest", "a null NpId was taken");
    Require(sceNpServiceCheckerIntCreateRequest(selfNpId, nullptr) == ServiceCheckerInvalidArgument, "sceNpServiceCheckerIntCreateRequest", "a null output was taken");
    Require(sceNpServiceCheckerIntCreateRequest(selfNpId, &request) == ServiceCheckerNotInitialized, "sceNpServiceCheckerIntCreateRequest", "a request was created before initialization");
    Require(sceNpServiceCheckerIntInitialize(0) == 0, "sceNpServiceCheckerIntInitialize", "initialization failed");
    Require(sceNpServiceCheckerIntInitialize(0) == ServiceCheckerAlreadyInitialized, "sceNpServiceCheckerIntInitialize", "a second initialization was taken");
    Require(sceNpServiceCheckerIntCreateRequest(selfNpId, &request) == 0 && request == 1, "sceNpServiceCheckerIntCreateRequest", "the first request is not id 1");
    std::int32_t second = 0;
    Require(sceNpServiceCheckerIntCreateRequest(selfNpId, &second) == 0 && second == 2, "sceNpServiceCheckerIntCreateRequest", "the second request is not id 2");
    Require(sceNpServiceCheckerIntAbortRequest(second) == 0, "sceNpServiceCheckerIntAbortRequest", "aborting the request failed");
    Require(sceNpServiceCheckerIntAbortRequest(second) == ServiceCheckerRequestNotFound, "sceNpServiceCheckerIntAbortRequest", "an aborted request was found");
    Require(sceNpServiceCheckerIntDestroyRequest(request) == 0, "sceNpServiceCheckerIntDestroyRequest", "destroying the request failed");
    Require(sceNpServiceCheckerIntDestroyRequest(request) == 0, "sceNpServiceCheckerIntDestroyRequest", "the destroy wrapper rejected an unknown request");
    sceNpServiceCheckerIntFinalize();
    Require(sceNpServiceCheckerIntCreateRequest(selfNpId, &request) == ServiceCheckerNotInitialized, "sceNpServiceCheckerIntCreateRequest", "a request was created after finalization");
    Require(sceNpServiceCheckerIntAbortRequest(1) == ServiceCheckerNotInitialized, "sceNpServiceCheckerIntAbortRequest", "an abort was accepted after finalization");
    Require(sceNpServiceCheckerIntDestroyRequest(1) == ServiceCheckerNotInitialized, "sceNpServiceCheckerIntDestroyRequest", "a destroy was accepted after finalization");
}

void CheckServiceChecker2() {
    constexpr int AlreadyInitialized = static_cast<int>(0x8055B301u);
    constexpr int NotInitialized = static_cast<int>(0x8055B302u);
    constexpr int RequestNotFound = static_cast<int>(0x8055B304u);
    constexpr int CheckerInvalidArgument = static_cast<int>(0x8055B305u);
    int request = 0;
    Require(sceNpServiceChecker2IntCreateRequest(&request) == NotInitialized, "sceNpServiceChecker2IntCreateRequest", "a request was created before initialization");
    Require(sceNpServiceChecker2IntInitialize() == 0, "sceNpServiceChecker2IntInitialize", "initialization failed");
    Require(sceNpServiceChecker2IntInitialize() == AlreadyInitialized, "sceNpServiceChecker2IntInitialize", "a second initialization was taken");
    Require(sceNpServiceChecker2IntCreateRequest(nullptr) == CheckerInvalidArgument, "sceNpServiceChecker2IntCreateRequest", "a null output was taken");
    Require(sceNpServiceChecker2IntCreateRequest(reinterpret_cast<int*>(1)) == CheckerInvalidArgument, "sceNpServiceChecker2IntCreateRequest", "an unmapped low output was taken");
    Require(sceNpServiceChecker2IntCreateRequest(reinterpret_cast<int*>(~0xFu)) == CheckerInvalidArgument, "sceNpServiceChecker2IntCreateRequest", "an unmapped high output was taken");
    Require(sceNpServiceChecker2IntCreateRequest(&request) == 0 && request == 1, "sceNpServiceChecker2IntCreateRequest", "the first request is not id 1");
    std::uint32_t availability = 0xA5A5A5A5;
    Require(sceNpServiceChecker2IntGetServiceAvailability(request, nullptr, &availability) == CheckerInvalidArgument, "sceNpServiceChecker2IntGetServiceAvailability", "a null NpId was taken");
    Require(sceNpServiceChecker2IntGetServiceAvailability(request, selfNpId, nullptr) == CheckerInvalidArgument, "sceNpServiceChecker2IntGetServiceAvailability", "a null output was taken");
    Require(sceNpServiceChecker2IntGetServiceAvailability(request + 100, selfNpId, &availability) == RequestNotFound, "sceNpServiceChecker2IntGetServiceAvailability", "an unknown request was taken");
    Require(sceNpServiceChecker2IntGetServiceAvailability(request, selfNpId, &availability) == SignedOut, "sceNpServiceChecker2IntGetServiceAvailability", "the lookup did not return SIGNED_OUT");
    Require(availability == 0xA5A5A5A5, "sceNpServiceChecker2IntGetServiceAvailability", "an availability was written");
    Require(sceNpServiceChecker2IntGetServiceAvailabilityA(request, -1, &availability) == CheckerInvalidArgument, "sceNpServiceChecker2IntGetServiceAvailabilityA", "user -1 was taken");
    Require(sceNpServiceChecker2IntGetServiceAvailabilityA(request, 1, nullptr) == CheckerInvalidArgument, "sceNpServiceChecker2IntGetServiceAvailabilityA", "a null output was taken");
    Require(sceNpServiceChecker2IntGetServiceAvailabilityA(request + 100, 1, &availability) == RequestNotFound, "sceNpServiceChecker2IntGetServiceAvailabilityA", "an unknown request was taken");
    Require(sceNpServiceChecker2IntGetServiceAvailabilityA(request, 1, &availability) == SignedOut, "sceNpServiceChecker2IntGetServiceAvailabilityA", "the lookup did not return SIGNED_OUT");
    Require(availability == 0xA5A5A5A5, "sceNpServiceChecker2IntGetServiceAvailabilityA", "an availability was written");
    int second = 0;
    Require(sceNpServiceChecker2IntCreateRequest(&second) == 0 && second == 2, "sceNpServiceChecker2IntCreateRequest", "the second request is not id 2");
    Require(sceNpServiceChecker2IntDestroyRequest(second) == 0, "sceNpServiceChecker2IntDestroyRequest", "destroying the request failed");
    Require(sceNpServiceChecker2IntGetServiceAvailability(second, selfNpId, &availability) == RequestNotFound, "sceNpServiceChecker2IntDestroyRequest", "a destroyed request was found");
    Require(sceNpServiceChecker2IntAbortRequest(request) == 0, "sceNpServiceChecker2IntAbortRequest", "aborting the request failed");
    Require(sceNpServiceChecker2IntAbortRequest(request + 100) == RequestNotFound, "sceNpServiceChecker2IntAbortRequest", "an unknown abort request was accepted");
    Require(sceNpServiceChecker2IntDestroyRequest(request + 100) == 0, "sceNpServiceChecker2IntDestroyRequest", "the destroy wrapper rejected an unknown request");
    const std::uint32_t serviceTypes[1] = {0x5};
    Require(sceNpServiceChecker2IntIsSetServiceType(serviceTypes, 0) == 1, "sceNpServiceChecker2IntIsSetServiceType", "bit 0 was not set");
    Require(sceNpServiceChecker2IntIsSetServiceType(serviceTypes, 1) == 0, "sceNpServiceChecker2IntIsSetServiceType", "bit 1 was set");
    Require(sceNpServiceChecker2IntIsSetServiceType(serviceTypes, 2) == 1, "sceNpServiceChecker2IntIsSetServiceType", "bit 2 was not set");
    Require(sceNpServiceChecker2IntIsSetServiceType(serviceTypes, 10) == 0, "sceNpServiceChecker2IntIsSetServiceType", "type 10 was taken");
    Require(sceNpServiceChecker2IntIsSetServiceType(nullptr, 0) == 0, "sceNpServiceChecker2IntIsSetServiceType", "a null mask was taken");
    Require(sceNpServiceChecker2IntFinalize() == 0, "sceNpServiceChecker2IntFinalize", "finalization failed");
    Require(sceNpServiceChecker2IntInitialize() == 0, "sceNpServiceChecker2IntInitialize", "reinitialization failed");
    int boundaryId = 0;
    for (std::uint32_t index = 0; index < 0x1000; ++index) {
        Require(sceNpServiceChecker2IntCreateRequest(&boundaryId) == 0, "sceNpServiceChecker2IntCreateRequest", "a request below the host limit was refused");
    }
    const int lastId = boundaryId;
    Require(sceNpServiceChecker2IntCreateRequest(&boundaryId) == TooManyObjects, "sceNpServiceChecker2IntCreateRequest", "the host request limit was not enforced");
    Require(boundaryId == lastId, "sceNpServiceChecker2IntCreateRequest", "the request output changed on refusal");
    Require(sceNpServiceChecker2IntGetServiceAvailability(lastId, selfNpId, &availability) == SignedOut, "sceNpServiceChecker2IntGetServiceAvailability", "an existing request was lost at the host limit");
    Require(sceNpServiceChecker2IntDestroyRequest(lastId) == 0, "sceNpServiceChecker2IntDestroyRequest", "destroying a request at the host limit failed");
    for (std::uint32_t index = 0; index < 0x2000; ++index) {
        Require(sceNpServiceChecker2IntCreateRequest(&boundaryId) == 0, "sceNpServiceChecker2IntCreateRequest", "the host limit counted destroyed requests");
        Require(sceNpServiceChecker2IntDestroyRequest(boundaryId) == 0, "sceNpServiceChecker2IntDestroyRequest", "destroying a recycled request failed");
    }
    Require(sceNpServiceChecker2IntFinalize() == 0, "sceNpServiceChecker2IntFinalize", "finalization after the host limit failed");
    Require(sceNpServiceChecker2IntCreateRequest(&request) == NotInitialized, "sceNpServiceChecker2IntCreateRequest", "a request was created after finalization");
}

void CheckLookupNet() {
    constexpr int NotInitialized = static_cast<int>(0x80550702u);
    constexpr int InsufficientArgumentNet = static_cast<int>(0x8055070Cu);
    constexpr int InvalidIdNet = static_cast<int>(0x8055070Eu);
    constexpr int InvalidTypeNet = static_cast<int>(0x80550711u);
    int initialized = -1;
    Require(sceNpLookupNetIsInit(&initialized) == 0 && initialized == 0, "sceNpLookupNetIsInit", "an uninitialized service reported itself initialized");
    Require(sceNpLookupNetIsInit(nullptr) == 0, "sceNpLookupNetIsInit", "a null output was taken");
    Require(sceNpLookupNetTerm() == NotInitialized, "sceNpLookupNetTerm", "terminating an uninitialized service was taken");
    Require(sceNpLookupNetCreateTitleCtx(nullptr, selfNpId) == NotInitialized, "sceNpLookupNetCreateTitleCtx", "a context was created before initialization");
    Require(sceNpLookupNetInit() == 0, "sceNpLookupNetInit", "initialization failed");
    Require(sceNpLookupNetIsInit(&initialized) == 0 && initialized == 1, "sceNpLookupNetIsInit", "the service did not report itself initialized");
    Require(sceNpLookupNetInit() == 0, "sceNpLookupNetInit", "a second initialization was refused");
    Require(sceNpLookupNetTerm() == 0, "sceNpLookupNetTerm", "the first termination failed");
    Require(sceNpLookupNetIsInit(&initialized) == 0 && initialized == 1, "sceNpLookupNetTerm", "the reference count did not keep the service initialized");
    Require(sceNpLookupNetInitWithFunctionPointer(nullptr, nullptr) == 0, "sceNpLookupNetInitWithFunctionPointer", "the function-pointer initialization failed");
    Require(sceNpLookupNetIsInit(&initialized) == 0 && initialized == 1, "sceNpLookupNetInitWithFunctionPointer", "the service dropped out of the initialized state");
    Require(sceNpLookupNetInitWithMemoryPool(nullptr, nullptr) == 0, "sceNpLookupNetInitWithMemoryPool", "the memory-pool initialization failed");
    Require(sceNpLookupNetTerm() == 0, "sceNpLookupNetTerm", "terminating one wrapper reference was refused");
    Require(sceNpLookupNetIsInit(&initialized) == 0 && initialized == 1, "sceNpLookupNetTerm", "a wrapper reference did not join the shared count");
    Require(sceNpLookupNetTerm() == 0, "sceNpLookupNetTerm", "terminating the second wrapper reference was refused");
    Require(sceNpLookupNetIsInit(&initialized) == 0 && initialized == 1, "sceNpLookupNetTerm", "the service left the initialized state while a plain reference remained");
    Require(sceNpLookupNetCreateTitleCtx(nullptr, nullptr) == InsufficientArgumentNet, "sceNpLookupNetCreateTitleCtx", "a null NpId was taken");
    const int firstContext = sceNpLookupNetCreateTitleCtx(nullptr, selfNpId);
    Require(firstContext == 1, "sceNpLookupNetCreateTitleCtx", "the first context is not id 1");
    const int secondContext = sceNpLookupNetCreateTitleCtx(selfNpId, npId);
    Require(secondContext == 2, "sceNpLookupNetCreateTitleCtx", "the second context is not id 2");
    Require(sceNpLookupNetCreateRequest(firstContext + 100) == InvalidIdNet, "sceNpLookupNetCreateRequest", "an unknown context was taken");
    const int firstRequest = sceNpLookupNetCreateRequest(firstContext);
    Require(firstRequest == 1, "sceNpLookupNetCreateRequest", "the first request is not id 1");
    const int secondRequest = sceNpLookupNetCreateRequest(firstContext);
    Require(secondRequest == 2, "sceNpLookupNetCreateRequest", "the second request is not id 2");
    Require(sceNpLookupNetSetTimeout(999, 1, 0, 0, 0, 0) == InvalidIdNet, "sceNpLookupNetSetTimeout", "an unknown id was taken");
    Require(sceNpLookupNetSetTimeout(firstContext, -1, 0, 0, 0, 0) == InvalidArgument, "sceNpLookupNetSetTimeout", "a negative retry count was taken");
    Require(sceNpLookupNetSetTimeout(firstContext, 1, 0, 0, 0, 0) == 0, "sceNpLookupNetSetTimeout", "a valid timeout was refused");
    Require(sceNpLookupNetNpId(999, selfNpId, npId, nullptr) == InvalidIdNet, "sceNpLookupNetNpId", "an unknown request was taken");
    Require(sceNpLookupNetNpId(firstRequest, nullptr, npId, nullptr) == InsufficientArgumentNet, "sceNpLookupNetNpId", "a null online ID was taken");
    Require(sceNpLookupNetNpId(firstRequest, selfNpId, nullptr, nullptr) == InsufficientArgumentNet, "sceNpLookupNetNpId", "a null NpId output was taken");
    Require(sceNpLookupNetNpId(firstRequest, selfNpId, npId, nullptr) == SignedOut, "sceNpLookupNetNpId", "the lookup did not return SIGNED_OUT");
    Require(sceNpLookupNetNpId(firstRequest, selfNpId, npId, nullptr) == InvalidTypeNet, "sceNpLookupNetNpId", "a used request did not return INVALID_TYPE");
    Require(sceNpLookupNetDeleteRequest(secondRequest) == 0, "sceNpLookupNetDeleteRequest", "deleting the request failed");
    Require(sceNpLookupNetDeleteRequest(secondRequest) == InvalidIdNet, "sceNpLookupNetDeleteRequest", "a deleted request was found");
    Require(sceNpLookupNetAbortRequest(firstRequest) == 0, "sceNpLookupNetAbortRequest", "aborting the request failed");
    Require(sceNpLookupNetDeleteTitleCtx(firstContext) == 0, "sceNpLookupNetDeleteTitleCtx", "deleting the context failed");
    Require(sceNpLookupNetDeleteTitleCtx(firstContext) == InvalidIdNet, "sceNpLookupNetDeleteTitleCtx", "a deleted context was found");
    const int censorRequest = sceNpLookupNetCreateRequest(secondContext);
    Require(censorRequest == 3, "sceNpLookupNetCreateRequest", "the censor request is not id 3");
    Require(sceNpLookupNetCensorComment(censorRequest, selfNpId, nullptr, nullptr) == InsufficientArgumentNet, "sceNpLookupNetCensorComment", "a null comment was taken");
    Require(sceNpLookupNetCensorComment(censorRequest, selfNpId, "word", selfNpId) == InvalidArgument, "sceNpLookupNetCensorComment", "an option was taken");
    const std::string longComment(0x401, 'a');
    Require(sceNpLookupNetCensorComment(censorRequest, selfNpId, longComment.c_str(), nullptr) == InvalidArgument, "sceNpLookupNetCensorComment", "a 0x401-byte comment was taken");
    Require(sceNpLookupNetCensorComment(censorRequest, nullptr, "word", nullptr) == InsufficientArgumentNet, "sceNpLookupNetCensorComment", "a null NpId was taken");
    Require(sceNpLookupNetCensorComment(censorRequest + 100, selfNpId, "word", nullptr) == InvalidIdNet, "sceNpLookupNetCensorComment", "an unknown request was taken");
    Require(sceNpLookupNetCensorComment(censorRequest, selfNpId, "word", nullptr) == SignedOut, "sceNpLookupNetCensorComment", "the censor did not return SIGNED_OUT");
    Require(sceNpLookupNetCensorComment(censorRequest, selfNpId, "word", nullptr) == InvalidTypeNet, "sceNpLookupNetCensorComment", "a used request did not return INVALID_TYPE");
    const int sanitizeRequest = sceNpLookupNetCreateRequest(secondContext);
    Require(sanitizeRequest == 4, "sceNpLookupNetCreateRequest", "the sanitize request is not id 4");
    char sanitized[0x402] = {};
    Require(sceNpLookupNetSanitizeComment(sanitizeRequest, selfNpId, "word", nullptr, nullptr) == InsufficientArgumentNet, "sceNpLookupNetSanitizeComment", "a null output was taken");
    Require(sceNpLookupNetSanitizeComment(sanitizeRequest, selfNpId, nullptr, sanitized, nullptr) == InsufficientArgumentNet, "sceNpLookupNetSanitizeComment", "a null comment was taken");
    Require(sceNpLookupNetSanitizeComment(sanitizeRequest, selfNpId, "word", sanitized, selfNpId) == InvalidArgument, "sceNpLookupNetSanitizeComment", "an option was taken");
    Require(sceNpLookupNetSanitizeComment(sanitizeRequest, selfNpId, longComment.c_str(), sanitized, nullptr) == InvalidArgument, "sceNpLookupNetSanitizeComment", "a 0x401-byte comment was taken");
    Require(sceNpLookupNetSanitizeComment(sanitizeRequest, nullptr, "word", sanitized, nullptr) == InsufficientArgumentNet, "sceNpLookupNetSanitizeComment", "a null NpId was taken");
    Require(sceNpLookupNetSanitizeComment(sanitizeRequest + 100, selfNpId, "word", sanitized, nullptr) == InvalidIdNet, "sceNpLookupNetSanitizeComment", "an unknown request was taken");
    Require(sceNpLookupNetSanitizeComment(sanitizeRequest, selfNpId, "word", sanitized, nullptr) == SignedOut, "sceNpLookupNetSanitizeComment", "the sanitize did not return SIGNED_OUT");
    Require(sceNpLookupNetSanitizeComment(sanitizeRequest, selfNpId, "word", sanitized, nullptr) == InvalidTypeNet, "sceNpLookupNetSanitizeComment", "a used request did not return INVALID_TYPE");
    Require(sceNpLookupNetDeleteTitleCtx(secondContext) == 0, "sceNpLookupNetDeleteTitleCtx", "deleting the second context failed");
    Require(sceNpLookupNetTerm() == 0, "sceNpLookupNetTerm", "the second termination failed");
    Require(sceNpLookupNetIsInit(&initialized) == 0 && initialized == 0, "sceNpLookupNetTerm", "the service stayed initialized");
    Require(sceNpLookupNetTerm() == NotInitialized, "sceNpLookupNetTerm", "a terminated service was terminated again");
}

void CheckLookupNetInitVariants() {
    constexpr int NotInitialized = static_cast<int>(0x80550702u);
    int initialized = -1;
    Require(sceNpLookupNetIsInit(&initialized) == 0 && initialized == 0, "sceNpLookupNetIsInit", "a terminated service reported itself initialized");
    Require(sceNpLookupNetTerm() == NotInitialized, "sceNpLookupNetTerm", "terminating an uninitialized service was taken");
    Require(sceNpLookupNetInitWithFunctionPointer(nullptr, nullptr) == 0, "sceNpLookupNetInitWithFunctionPointer", "initialization with null arguments failed");
    Require(sceNpLookupNetIsInit(&initialized) == 0 && initialized == 1, "sceNpLookupNetInitWithFunctionPointer", "the service did not report itself initialized");
    Require(sceNpLookupNetInitWithMemoryPool(nullptr, nullptr) == 0, "sceNpLookupNetInitWithMemoryPool", "a second initialization through the memory pool variant was refused");
    Require(sceNpLookupNetIsInit(&initialized) == 0 && initialized == 1, "sceNpLookupNetInitWithMemoryPool", "the service did not report itself initialized after both variants");
    Require(sceNpLookupNetTerm() == 0, "sceNpLookupNetTerm", "the first termination failed");
    Require(sceNpLookupNetIsInit(&initialized) == 0 && initialized == 1, "sceNpLookupNetTerm", "the reference count did not keep the service initialized after one variant init");
    Require(sceNpLookupNetTerm() == 0, "sceNpLookupNetTerm", "the second termination failed");
    Require(sceNpLookupNetIsInit(&initialized) == 0 && initialized == 0, "sceNpLookupNetTerm", "the service stayed initialized after both terminations");
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
    errno = 0;
    Require(sceNpBandwidthTestGetStatus(0, reinterpret_cast<int*>(0x10)) == npInvalidArgument, "sceNpBandwidthTestGetStatus", "unwritable status was taken");
    Require(errno == 0, "sceNpBandwidthTestGetStatus", "errno was modified");
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
    CheckLookupNet();
    CheckLookupNetInitVariants();
    CheckBandwidthTestInitStart();
    CheckBandwidthTestShutdownWithDetailedInfo();
    CheckNpUtilityLifecycles();
    CheckNpUtilityPayloads();
    CheckServiceChecker2();
    CheckLookup();
    CheckWordFilter();
    CheckLookupAsync();
    CheckWordFilterAsync();
    CheckTitleContextsA();
    if (failures != 0) return 1;
    std::puts("NpUtility offline tests passed");
    return 0;
}
