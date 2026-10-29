#include "prx/libSceNpAuth/include/Api.hpp"
#include "SceTypes.hpp"

#include <cstdint>
#include <cstdlib>
#include <initializer_list>

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

static void RequireUnstarted(int request) {
    constexpr int invalidId = static_cast<int>(0x80550307);
    constexpr int sentinel = 0x13579bdf;
    for (int attempt = 0; attempt != 3; ++attempt) {
        int result = sentinel;
        Require(sceNpAuthPollAsync(request, &result) == invalidId);
        Require(result == sentinel);
        Require(sceNpAuthWaitAsync(request, &result) == invalidId);
        Require(result == sentinel);
    }
}

static void CheckUnstartedAborts() {
    constexpr int aborted = static_cast<int>(0x80550304);
    alignas(8) char parameter[16]{};
    alignas(8) char output[16]{};
    for (const bool async : {false, true}) {
        const int request = async ? sceNpAuthCreateAsyncRequest(nullptr) : sceNpAuthCreateRequest();
        Require(request > 0);
        RequireUnstarted(request);
        Require(sceNpAuthAbortRequest(request) == 0);
        Require(sceNpAuthAbortRequest(request) == 0);
        RequireUnstarted(request);
        for (auto getter : {sceNpAuthGetIdToken, sceNpAuthGetIdTokenV3, sceNpAuthGetAuthorizationCode}) {
            Require(getter(request, parameter, output) == aborted);
            RequireUnstarted(request);
        }
        int issuer = 0x2468ace;
        Require(sceNpAuthGetAuthorizationCodeV3(request, parameter, output, &issuer) == aborted);
        Require(issuer == 0x2468ace);
        RequireUnstarted(request);
        Require(sceNpAuthDeleteRequest(request) == 0);
    }
}

int main() {
    CheckUnstartedAborts();
    constexpr int invalidArgument = static_cast<int>(0x80550301);
    constexpr int signedOut = static_cast<int>(0x80550006);
    constexpr int aborted = static_cast<int>(0x80550304);
    constexpr int requestNotFound = static_cast<int>(0x80550306);
    constexpr int invalidId = static_cast<int>(0x80550307);
    constexpr int pollRunning = 1;

    const int syncReq = sceNpAuthCreateRequest();
    const int syncReq2 = sceNpAuthCreateRequest();
    Require(syncReq > 0);
    Require(syncReq2 > syncReq);

    const int asyncReq = sceNpAuthCreateAsyncRequest(nullptr);
    const int asyncReq2 = sceNpAuthCreateAsyncRequest(nullptr);
    Require(asyncReq > syncReq2);
    Require(asyncReq2 > asyncReq);

    alignas(8) char param[16] = {};
    alignas(8) char token[16] = {};
    alignas(8) char authCode[16] = {};
    int issuerId = 0;

    Require(sceNpAuthSetTimeout(syncReq, 2, 5000000, 10000000, 10000000, 10000000) == 0);
    Require(sceNpAuthSetTimeout(asyncReq, 0, 0, 0, 0, 0) == invalidArgument);

    Require(sceNpAuthSetTimeout(syncReq, -1, 1000000, 10000000, 10000000, 10000000) == invalidArgument);
    Require(sceNpAuthSetTimeout(syncReq, 1, 999999, 10000000, 10000000, 10000000) == invalidArgument);
    Require(sceNpAuthSetTimeout(syncReq, 1, 1000000, 9999999, 10000000, 10000000) == invalidArgument);
    Require(sceNpAuthSetTimeout(syncReq, 1, 1000000, 10000000, 9999999, 10000000) == invalidArgument);
    Require(sceNpAuthSetTimeout(syncReq, 1, 1000000, 10000000, 10000000, 9999999) == invalidArgument);
    Require(sceNpAuthSetTimeout(syncReq, 1, 1000000, 10000000, 10000000, 10000000) == 0);
    Require(sceNpAuthSetTimeout(syncReq, 1, 0, 0, 0, 0) == 0);
    Require(sceNpAuthSetTimeout(syncReq, 0, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu) == 0);
    Require(sceNpAuthSetTimeout(-1, 0, 0, 0, 0, 0) == invalidArgument);

    int pollUnprocessed = 0;
    Require(sceNpAuthPollAsync(asyncReq, &pollUnprocessed) == invalidId);
    int waitUnprocessed = 0;
    Require(sceNpAuthWaitAsync(asyncReq, &waitUnprocessed) == invalidId);

    Require(sceNpAuthGetAuthorizedAppCode(syncReq, nullptr, nullptr, nullptr, nullptr) == invalidArgument);

    Require(sceNpAuthGetIdToken(syncReq, nullptr, token) == invalidArgument);
    Require(sceNpAuthGetIdToken(syncReq, param, nullptr) == invalidArgument);
    Require(sceNpAuthGetIdToken(syncReq, nullptr, nullptr) == invalidArgument);
    Require(sceNpAuthGetIdTokenV3(syncReq, nullptr, token) == invalidArgument);
    Require(sceNpAuthGetIdTokenV3(syncReq, param, nullptr) == invalidArgument);
    Require(sceNpAuthGetIdTokenV3(syncReq, nullptr, nullptr) == invalidArgument);
    Require(sceNpAuthGetIdToken(syncReq, param, token) == signedOut);
    Require(sceNpAuthGetIdToken(syncReq, param, token) == invalidArgument);
    Require(sceNpAuthGetIdTokenV3(syncReq, param, token) == invalidArgument);

    Require(sceNpAuthGetAuthorizationCode(syncReq2, nullptr, authCode) == invalidArgument);
    Require(sceNpAuthGetAuthorizationCode(syncReq2, param, nullptr) == invalidArgument);
    Require(sceNpAuthGetAuthorizationCodeV3(syncReq, nullptr, authCode, &issuerId) == invalidArgument);
    Require(sceNpAuthGetAuthorizationCodeV3(syncReq, param, nullptr, &issuerId) == invalidArgument);
    Require(sceNpAuthGetAuthorizationCode(syncReq2, param, authCode) == signedOut);
    Require(sceNpAuthGetAuthorizationCode(syncReq2, param, authCode) == invalidArgument);
    Require(sceNpAuthGetAuthorizationCodeV3(syncReq2, param, authCode, &issuerId) == invalidArgument);

    Require(sceNpAuthGetIdTokenV3(asyncReq, param, token) == 0);
    Require(sceNpAuthGetIdTokenV3(asyncReq, param, token) == invalidArgument);
    int asyncPoll = 0;
    Require(sceNpAuthPollAsync(asyncReq, &asyncPoll) == pollRunning);
    Require(asyncPoll == 0);
    Require(sceNpAuthPollAsync(asyncReq, &asyncPoll) == 0);
    Require(asyncPoll == signedOut);
    Require(sceNpAuthPollAsync(asyncReq, &asyncPoll) == 0);
    Require(asyncPoll == signedOut);
    int asyncWait = 0;
    Require(sceNpAuthWaitAsync(asyncReq, &asyncWait) == 0);
    Require(asyncWait == signedOut);

    Require(sceNpAuthGetAuthorizationCodeV3(asyncReq2, param, authCode, &issuerId) == 0);
    Require(sceNpAuthGetAuthorizationCodeV3(asyncReq2, param, authCode, &issuerId) == invalidArgument);
    int asyncWait2 = 0;
    Require(sceNpAuthWaitAsync(asyncReq2, &asyncWait2) == 0);
    Require(asyncWait2 == signedOut);

    constexpr int unknownReq = 999999;
    Require(sceNpAuthGetIdToken(unknownReq, param, token) == requestNotFound);
    Require(sceNpAuthGetIdTokenV3(unknownReq, param, token) == requestNotFound);
    Require(sceNpAuthGetAuthorizationCode(unknownReq, param, authCode) == requestNotFound);
    Require(sceNpAuthGetAuthorizationCodeV3(unknownReq, param, authCode, &issuerId) == requestNotFound);
    int pollResult = 0;
    Require(sceNpAuthPollAsync(unknownReq, &pollResult) == requestNotFound);
    int waitResult = 0;
    Require(sceNpAuthWaitAsync(unknownReq, &waitResult) == requestNotFound);
    Require(sceNpAuthSetTimeout(unknownReq, 2, 5000000, 10000000, 10000000, 10000000) == requestNotFound);

    Require(sceNpAuthWaitAsync(syncReq, nullptr) == invalidArgument);
    Require(sceNpAuthWaitAsync(syncReq, &waitResult) == invalidId);
    Require(sceNpAuthPollAsync(syncReq, &pollResult) == invalidId);

    Require(sceNpAuthAbortRequest(asyncReq) == 0);
    Require(sceNpAuthGetIdTokenV3(asyncReq, param, token) == invalidArgument);
    Require(sceNpAuthAbortRequest(unknownReq) == requestNotFound);

    const int abortedReq = sceNpAuthCreateAsyncRequest(nullptr);
    Require(sceNpAuthAbortRequest(abortedReq) == 0);
    Require(sceNpAuthGetIdTokenV3(abortedReq, param, token) == aborted);
    Require(sceNpAuthGetAuthorizationCodeV3(abortedReq, param, authCode, &issuerId) == aborted);
    int abortedWait = 0;
    Require(sceNpAuthWaitAsync(abortedReq, &abortedWait) == invalidId);
    Require(abortedWait == 0);

    const int syncReqA = sceNpAuthCreateRequest();
    Require(sceNpAuthGetIdTokenA(syncReqA, nullptr, token) == invalidArgument);
    Require(sceNpAuthGetIdTokenA(syncReqA, param, nullptr) == invalidArgument);
    Require(sceNpAuthGetIdTokenA(syncReqA, param, token) == signedOut);
    Require(sceNpAuthGetIdTokenA(syncReqA, param, token) == invalidArgument);
    Require(sceNpAuthGetIdTokenA(unknownReq, param, token) == requestNotFound);

    const int syncReqAuthA = sceNpAuthCreateRequest();
    Require(sceNpAuthGetAuthorizationCodeA(syncReqAuthA, nullptr, authCode, &issuerId) == invalidArgument);
    Require(sceNpAuthGetAuthorizationCodeA(syncReqAuthA, param, nullptr, &issuerId) == invalidArgument);
    Require(sceNpAuthGetAuthorizationCodeA(syncReqAuthA, param, authCode, nullptr) == signedOut);
    Require(sceNpAuthGetAuthorizationCodeA(syncReqAuthA, param, authCode, &issuerId) == invalidArgument);
    Require(sceNpAuthGetAuthorizationCodeA(unknownReq, param, authCode, &issuerId) == requestNotFound);

    const int asyncReqA = sceNpAuthCreateAsyncRequest(nullptr);
    Require(sceNpAuthGetIdTokenA(asyncReqA, param, token) == 0);
    int asyncPollA = 0;
    Require(sceNpAuthPollAsync(asyncReqA, &asyncPollA) == pollRunning);
    Require(sceNpAuthPollAsync(asyncReqA, &asyncPollA) == 0);
    Require(asyncPollA == signedOut);

    const int asyncReqAuthA = sceNpAuthCreateAsyncRequest(nullptr);
    Require(sceNpAuthGetAuthorizationCodeA(asyncReqAuthA, param, authCode, &issuerId) == 0);
    int asyncWaitA = 0;
    Require(sceNpAuthWaitAsync(asyncReqAuthA, &asyncWaitA) == 0);
    Require(asyncWaitA == signedOut);

    const int abortedReqA = sceNpAuthCreateAsyncRequest(nullptr);
    Require(sceNpAuthAbortRequest(abortedReqA) == 0);
    Require(sceNpAuthGetIdTokenA(abortedReqA, param, token) == aborted);
    Require(sceNpAuthGetAuthorizationCodeA(abortedReqA, param, authCode, &issuerId) == aborted);

    Require(sceNpAuthDeleteRequest(syncReq) == 0);
    Require(sceNpAuthGetIdTokenV3(syncReq, param, token) == requestNotFound);
    Require(sceNpAuthWaitAsync(syncReq, &waitResult) == requestNotFound);
    Require(sceNpAuthSetTimeout(syncReq, 2, 5000000, 10000000, 10000000, 10000000) == requestNotFound);

    return 0;
}
