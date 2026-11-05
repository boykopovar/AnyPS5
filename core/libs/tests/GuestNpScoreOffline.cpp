#include "prx/libSceNpScoreRanking/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

constexpr int SignedOut = static_cast<int>(0x80550006u);
constexpr int InvalidArgument = static_cast<int>(0x80550704u);
constexpr int TooManyObjects = static_cast<int>(0x80550706u);
constexpr int Aborted = static_cast<int>(0x80550707u);
constexpr int InsufficientArgument = static_cast<int>(0x8055070Cu);
constexpr int InvalidId = static_cast<int>(0x8055070Eu);
constexpr int InvalidAlignment = static_cast<int>(0x80550714u);
constexpr int TooManyNpId = static_cast<int>(0x80550719u);
constexpr int PollFinished = 0;
constexpr int PollRunning = 1;

int failures = 0;
int context = 0;
unsigned char selfNpId[36];
unsigned char npIds[2 * 36];
unsigned char ranks[2 * 0x88];
unsigned char comments[2 * 64];
unsigned char infos[2 * 200];
unsigned char board[24];
unsigned char manyNpIds[101 * 36];
unsigned char manyRanks[101 * 0x88];
unsigned char ranksA[2 * 0x98];
std::uint64_t accountIds[2] = {1, 2};
unsigned char npIdPcIds[2 * 44];
unsigned char accountIdPcIds[2 * 16];

using RankingFn = int (APS5_VABI*)(int, std::uint32_t, const void*, std::uint64_t, void*, std::uint64_t, void*, std::uint64_t, void*, std::uint64_t, std::uint64_t, void*, void*, void*);
using AccountRankingFn = int (APS5_VABI*)(int, std::uint32_t, const std::uint64_t*, std::uint64_t, void*, std::uint64_t, void*, std::uint64_t, void*, std::uint64_t, std::uint64_t, void*, void*, void*);
using RangeFn = int (APS5_VABI*)(int, std::uint32_t, std::uint32_t, void*, std::uint64_t, void*, std::uint64_t, void*, std::uint64_t, std::uint64_t, void*, void*, void*);
using FriendsFn = int (APS5_VABI*)(int, std::uint32_t, int, void*, std::uint64_t, void*, std::uint64_t, void*, std::uint64_t, std::uint64_t, void*, void*, const NpScoreFriendRankingOption*);
using GameDataFn = int (APS5_VABI*)(int, std::uint32_t, const void*, std::uint64_t*, std::uint64_t, void*, void*);
using GameDataByAccountIdFn = int (APS5_VABI*)(int, std::uint32_t, std::uint64_t, std::uint64_t*, std::uint64_t, void*, void*);
using BoardInfoFn = int (APS5_VABI*)(int, std::uint32_t, void*, void*);
using CreateTitleCtxAFn = int (APS5_VABI*)(std::uint32_t, int);
using SetPlayerCharacterIdFn = int (APS5_VABI*)(int, int);
using WaitAsyncFn = int (APS5_VABI*)(int, int*);
using SetTimeoutFn = int (APS5_VABI*)(int, int, int, int, int, int);
using SetThreadParamFn = int (APS5_VABI*)(int, std::uint64_t);
using CreateTitleCtxFn = int (APS5_VABI*)(const void*, const void*, const void*);
using CensorFn = int (APS5_VABI*)(int, const char*, void*);
using SanitizeFn = int (APS5_VABI*)(int, const char*, char*, void*);
using RecordGameDataFn = int (APS5_VABI*)(int, std::uint32_t, std::int64_t, std::uint64_t, std::uint64_t, const void*, void*);
using ChangeModeFn = int (APS5_VABI*)(int, std::uint32_t);

template <typename Function>
Function Resolve(const char* name) {
#ifdef _WIN32
    void* symbol = reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("libSceNpScoreRanking.prx"), name));
#else
    void* symbol = dlsym(RTLD_DEFAULT, name);
#endif
    if (!symbol) {
        std::fprintf(stderr, "%s is not exported by libSceNpScoreRanking\n", name);
        std::abort();
    }
    return reinterpret_cast<Function>(symbol);
}

void Require(bool value, const char* name, const char* what) {
    if (value) return;
    std::fprintf(stderr, "%s: %s\n", name, what);
    ++failures;
}

int NewRequest() {
    return sceNpScoreCreateRequest(context);
}

bool Untouched() {
    for (const unsigned char value : ranks) if (value != 0xA5) return false;
    for (const unsigned char value : comments) if (value != 0xA5) return false;
    for (const unsigned char value : infos) if (value != 0xA5) return false;
    for (const unsigned char value : board) if (value != 0xA5) return false;
    for (const unsigned char value : ranksA) if (value != 0xA5) return false;
    return true;
}

void CheckOffline(const char* name, int status, int req) {
    Require(status == SignedOut, name, "valid arguments did not return SIGNED_OUT");
    int result = 0;
    Require(sceNpScorePollAsync(req, &result) == PollFinished && result == SignedOut, name, "the request did not finish with SIGNED_OUT");
}

void CheckContexts() {
    Require(sceNpScoreCreateNpTitleCtx(0xFFFFFFFFu, nullptr) == InvalidArgument, "sceNpScoreCreateNpTitleCtx", "the invalid service label was not refused before the null NpId");
    Require(sceNpScoreCreateNpTitleCtx(0, nullptr) == InsufficientArgument, "sceNpScoreCreateNpTitleCtx", "a null NpId was taken");
    context = sceNpScoreCreateNpTitleCtx(0, selfNpId);
    Require(context == 1, "sceNpScoreCreateNpTitleCtx", "the first context is not id 1");
    Require(sceNpScoreCreateRequest(context + 100) == InvalidId, "sceNpScoreCreateRequest", "an unknown context was taken");
    int extra[31];
    for (int& entry : extra) entry = sceNpScoreCreateNpTitleCtx(1, selfNpId);
    Require(extra[30] == 32, "sceNpScoreCreateNpTitleCtx", "32 contexts could not be created");
    Require(sceNpScoreCreateNpTitleCtx(1, selfNpId) == TooManyObjects, "sceNpScoreCreateNpTitleCtx", "a 33rd context did not return TOO_MANY_OBJECTS");
    const int kept = sceNpScoreCreateRequest(extra[0]);
    Require(kept == 1, "sceNpScoreCreateRequest", "the first request is not id 1");
    for (const int entry : extra) Require(sceNpScoreDeleteNpTitleCtx(entry) == 0, "sceNpScoreDeleteNpTitleCtx", "deleting a context failed");
    Require(sceNpScoreDeleteRequest(kept) == InvalidId, "sceNpScoreDeleteNpTitleCtx", "a request of a deleted context was kept");
    Require(sceNpScoreDeleteNpTitleCtx(extra[0]) == InvalidId, "sceNpScoreDeleteNpTitleCtx", "a deleted context was found");
    int requests[32];
    for (int& entry : requests) entry = NewRequest();
    Require(requests[31] > 0, "sceNpScoreCreateRequest", "32 requests could not be created");
    Require(NewRequest() == TooManyObjects, "sceNpScoreCreateRequest", "a 33rd request did not return TOO_MANY_OBJECTS");
    for (const int entry : requests) sceNpScoreDeleteRequest(entry);
    Require(sceNpScoreDeleteRequest(requests[0]) == InvalidId, "sceNpScoreDeleteRequest", "a deleted request was found");
}

void CheckRequests() {
    int req = NewRequest();
    int result = 7;
    Require(sceNpScorePollAsync(req + 100, &result) == InvalidId, "sceNpScorePollAsync", "an unknown request was taken");
    Require(sceNpScorePollAsync(req, &result) == PollRunning && result == 7, "sceNpScorePollAsync", "an unstarted request was not running");
    Require(sceNpScoreAbortRequest(req + 100) == InvalidId, "sceNpScoreAbortRequest", "an unknown request was taken");
    Require(sceNpScoreAbortRequest(req) == 0, "sceNpScoreAbortRequest", "aborting failed");
    Require(sceNpScorePollAsync(req, nullptr) == PollFinished, "sceNpScorePollAsync", "a null result was not accepted");
    Require(sceNpScorePollAsync(req, &result) == PollFinished && result == Aborted, "sceNpScoreAbortRequest", "an aborted request did not finish with ABORTED");
    Require(sceNpScoreRecordScore(req, 1, 5, nullptr, nullptr, nullptr, nullptr, nullptr) == Aborted, "sceNpScoreRecordScore", "an aborted request did not return ABORTED");
    sceNpScoreDeleteRequest(req);

    req = NewRequest();
    Require(sceNpScoreRecordScore(req, 1, 5, nullptr, nullptr, nullptr, nullptr, nullptr) == SignedOut, "sceNpScoreRecordScore", "the first call did not return SIGNED_OUT");
    Require(sceNpScoreAbortRequest(req) == 0, "sceNpScoreAbortRequest", "aborting a finished request failed");
    Require(sceNpScorePollAsync(req, &result) == PollFinished && result == SignedOut, "sceNpScoreAbortRequest", "aborting a finished request changed its result");
    Require(sceNpScoreRecordScore(req, 1, 5, nullptr, nullptr, nullptr, nullptr, nullptr) == SignedOut, "sceNpScoreRecordScore", "a finished request was not reused");
    sceNpScoreDeleteRequest(req);

}

void CheckRecord() {
    NpScoreGameInfo info{};
    std::uint32_t tmpRank = 77;
    const int req = NewRequest();
    info.infoSize = 190;
    Require(sceNpScoreRecordScore(req, 1, 5, nullptr, &info, &tmpRank, nullptr, &info) == InvalidArgument, "sceNpScoreRecordScore", "an option was taken");
    Require(sceNpScoreRecordScore(req, 1, 5, nullptr, &info, &tmpRank, nullptr, nullptr) == InvalidArgument, "sceNpScoreRecordScore", "190 bytes of game info were taken");
    Require(sceNpScoreRecordScoreAsync(req, 1, 5, nullptr, &info, &tmpRank, nullptr, nullptr) == InvalidArgument, "sceNpScoreRecordScoreAsync", "190 bytes of game info were taken");
    info.infoSize = 189;
    Require(sceNpScoreRecordScore(req + 100, 1, 5, nullptr, &info, &tmpRank, nullptr, nullptr) == InvalidId, "sceNpScoreRecordScore", "an unknown request was taken");
    Require(sceNpScorePollAsync(req, nullptr) == PollRunning, "sceNpScoreRecordScore", "a refused call finished the request");
    CheckOffline("sceNpScoreRecordScoreAsync", sceNpScoreRecordScoreAsync(req, 1, 5, nullptr, &info, &tmpRank, nullptr, nullptr), req);
    Require(tmpRank == 77, "sceNpScoreRecordScoreAsync", "a rank was written");
    sceNpScoreDeleteRequest(req);

}

void CheckRange() {
    const int req = NewRequest();
    Require(sceNpScoreGetRankingByRange(req, 1, 1, ranks, 0x80, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, board) == InvalidArgument, "sceNpScoreGetRankingByRange", "an option was taken");
    Require(sceNpScoreGetRankingByRange(req, 1, 1, ranks, 101 * 0x80, nullptr, 0, nullptr, 0, 101, nullptr, nullptr, nullptr) == InvalidArgument, "sceNpScoreGetRankingByRange", "101 ranks were taken");
    Require(sceNpScoreGetRankingByRange(req, 1, 0, nullptr, 0x80, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, nullptr) == InvalidArgument, "sceNpScoreGetRankingByRange", "rank 0 was not refused before the null array");
    Require(sceNpScoreGetRankingByRange(req, 1, 1, nullptr, 3, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, nullptr) == InsufficientArgument, "sceNpScoreGetRankingByRange", "a null rank array was not refused before its size");
    Require(sceNpScoreGetRankingByRange(req, 1, 1, ranks, 0, nullptr, 0, nullptr, 0, 0, nullptr, nullptr, nullptr) == InsufficientArgument, "sceNpScoreGetRankingByRange", "zero ranks were taken");
    Require(sceNpScoreGetRankingByRange(req, 1, 1, ranks, 0x88, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, nullptr) == InvalidAlignment, "sceNpScoreGetRankingByRange", "a rank array size other than 0x80 per rank was taken");
    Require(sceNpScoreGetRankingByRange(req, 1, 1, ranks, 0x100, comments, 64, nullptr, 0, 2, nullptr, nullptr, nullptr) == InvalidAlignment, "sceNpScoreGetRankingByRange", "a comment array size other than 64 per rank was taken");
    Require(sceNpScoreGetRankingByRange(req, 1, 1, ranks, 0x100, comments, 128, infos, 200, 2, nullptr, nullptr, nullptr) == InvalidAlignment, "sceNpScoreGetRankingByRange", "an info array size other than 200 per rank was taken");
    Require(sceNpScoreGetRankingByRange(req + 100, 1, 1, ranks, 0x100, comments, 128, infos, 400, 2, nullptr, nullptr, nullptr) == InvalidId, "sceNpScoreGetRankingByRange", "an unknown request was taken");
    CheckOffline("sceNpScoreGetRankingByRange", sceNpScoreGetRankingByRange(req, 1, 1, ranks, 0x100, comments, 128, infos, 400, 2, nullptr, nullptr, nullptr), req);
    Require(sceNpScoreGetRankingByRangeAsync(req, 1, 1, ranks, 0x100, nullptr, 0, nullptr, 0, 2, nullptr, nullptr, nullptr) == SignedOut, "sceNpScoreGetRankingByRangeAsync", "valid arguments did not return SIGNED_OUT");
    Require(sceNpScoreGetRankingByRangeAsync(req, 1, 0, ranks, 0x100, nullptr, 0, nullptr, 0, 2, nullptr, nullptr, nullptr) == InvalidArgument, "sceNpScoreGetRankingByRangeAsync", "rank 0 was taken");
    Require(sceNpScoreGetRankingByRangeAsync(req, 1, 1, manyRanks, 100 * 0x80, nullptr, 0, nullptr, 0, 100, nullptr, nullptr, nullptr) == SignedOut, "sceNpScoreGetRankingByRangeAsync", "100 ranks did not return SIGNED_OUT");
    sceNpScoreDeleteRequest(req);

}

void CheckNpId() {
    const int req = NewRequest();
    Require(sceNpScoreGetRankingByNpId(req, 1, npIds, 36, ranks, 0x88, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, board) == InvalidArgument, "sceNpScoreGetRankingByNpId", "an option was taken");
    Require(sceNpScoreGetRankingByNpId(req, 1, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0, 102, nullptr, nullptr, nullptr) == TooManyNpId, "sceNpScoreGetRankingByNpId", "102 NpIds were not refused before the null arrays");
    Require(sceNpScoreGetRankingByNpId(req, 1, nullptr, 36, ranks, 0x88, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, nullptr) == InsufficientArgument, "sceNpScoreGetRankingByNpId", "a null NpId array was taken");
    Require(sceNpScoreGetRankingByNpId(req, 1, npIds, 36, nullptr, 0x88, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, nullptr) == InsufficientArgument, "sceNpScoreGetRankingByNpId", "a null rank array was taken");
    Require(sceNpScoreGetRankingByNpId(req, 1, npIds, 24, ranks, 0x88, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, nullptr) == InvalidAlignment, "sceNpScoreGetRankingByNpId", "an NpId array size other than 36 per NpId was taken");
    Require(sceNpScoreGetRankingByNpId(req, 1, npIds, 36, ranks, 0x80, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, nullptr) == InvalidAlignment, "sceNpScoreGetRankingByNpId", "a rank array size other than 0x88 per NpId was taken");
    Require(sceNpScoreGetRankingByNpId(req, 1, npIds, 36, ranks, 0x88, comments, 128, nullptr, 0, 1, nullptr, nullptr, nullptr) == InvalidAlignment, "sceNpScoreGetRankingByNpId", "a comment array size other than 64 per NpId was taken");
    CheckOffline("sceNpScoreGetRankingByNpId", sceNpScoreGetRankingByNpId(req, 1, npIds, 72, ranks, 0x110, comments, 128, infos, 400, 2, nullptr, nullptr, nullptr), req);
    Require(sceNpScoreGetRankingByNpIdAsync(req, 1, npIds, 0, ranks, 0, nullptr, 0, nullptr, 0, 0, nullptr, nullptr, nullptr) == SignedOut, "sceNpScoreGetRankingByNpIdAsync", "zero NpIds did not return SIGNED_OUT");
    Require(sceNpScoreGetRankingByNpIdAsync(req, 1, manyNpIds, sizeof(manyNpIds), manyRanks, sizeof(manyRanks), nullptr, 0, nullptr, 0, 101, nullptr, nullptr, nullptr) == SignedOut, "sceNpScoreGetRankingByNpIdAsync", "101 NpIds did not return SIGNED_OUT");
    sceNpScoreDeleteRequest(req);

}

void CheckFriends() {
    const int req = NewRequest();
    const std::uint32_t zero = 0;
    const std::uint32_t five = 5;
    const NpScoreFriendRankingOption shortOption{16, nullptr, nullptr};
    const NpScoreFriendRankingOption rankZero{24, &zero, nullptr};
    const NpScoreFriendRankingOption rankFive{24, &five, nullptr};
    Require(sceNpScoreGetFriendsRanking(req, 1, 0, nullptr, 0x80, nullptr, 0, nullptr, 0, 0, nullptr, nullptr, &shortOption) == InvalidArgument, "sceNpScoreGetFriendsRanking", "a null rank array was taken");
    Require(sceNpScoreGetFriendsRanking(req, 1, 0, ranks, 0x88, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, &shortOption) == InvalidAlignment, "sceNpScoreGetFriendsRanking", "the rank array size was not checked before the option");
    Require(sceNpScoreGetFriendsRanking(req, 1, 0, ranks, 0x80, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, &rankFive) == InvalidArgument, "sceNpScoreGetFriendsRanking", "an option without includeSelf was taken");
    Require(sceNpScoreGetFriendsRanking(req, 1, 1, ranks, 0x80, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, &shortOption) == InvalidArgument, "sceNpScoreGetFriendsRanking", "an option size other than 24 was taken");
    Require(sceNpScoreGetFriendsRanking(req, 1, 1, ranks, 0x80, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, &rankZero) == InvalidArgument, "sceNpScoreGetFriendsRanking", "start rank 0 was taken");
    Require(sceNpScoreGetFriendsRanking(req, 1, 1, ranks, 101 * 0x80, nullptr, 0, nullptr, 0, 101, nullptr, nullptr, nullptr) == InvalidArgument, "sceNpScoreGetFriendsRanking", "101 friends were taken");
    Require(sceNpScoreGetFriendsRanking(req, 1, 1, ranks, 0, nullptr, 0, nullptr, 0, 0, nullptr, nullptr, nullptr) == InsufficientArgument, "sceNpScoreGetFriendsRanking", "zero friends were taken");
    Require(sceNpScoreGetFriendsRanking(req, 1, 1, ranks, 0x80, nullptr, 0, infos, 400, 1, nullptr, nullptr, nullptr) == InvalidAlignment, "sceNpScoreGetFriendsRanking", "an info array size other than 200 per friend was taken");
    CheckOffline("sceNpScoreGetFriendsRanking", sceNpScoreGetFriendsRanking(req, 1, 1, ranks, 0x80, comments, 64, infos, 200, 1, nullptr, nullptr, &rankFive), req);
    Require(sceNpScoreGetFriendsRankingAsync(req, 1, 0, ranks, 0x100, nullptr, 0, nullptr, 0, 2, nullptr, nullptr, nullptr) == SignedOut, "sceNpScoreGetFriendsRankingAsync", "valid arguments did not return SIGNED_OUT");
    Require(sceNpScoreGetFriendsRankingAsync(req, 1, 0, manyRanks, 100 * 0x80, nullptr, 0, nullptr, 0, 100, nullptr, nullptr, nullptr) == SignedOut, "sceNpScoreGetFriendsRankingAsync", "100 friends did not return SIGNED_OUT");
    sceNpScoreDeleteRequest(req);

}

void CheckBoard() {
    const int req = NewRequest();
    Require(sceNpScoreGetBoardInfo(req, 1, board, board) == InvalidArgument, "sceNpScoreGetBoardInfo", "an option was taken");
    Require(sceNpScoreGetBoardInfo(req + 100, 1, board, nullptr) == InvalidId, "sceNpScoreGetBoardInfo", "an unknown request was taken");
    CheckOffline("sceNpScoreGetBoardInfo", sceNpScoreGetBoardInfo(req, 1, board, nullptr), req);
    sceNpScoreDeleteRequest(req);

}

void CheckRangeAndFriendsA() {
    const auto rangeA = Resolve<RangeFn>("sceNpScoreGetRankingByRangeA");
    const auto rangeAAsync = Resolve<RangeFn>("sceNpScoreGetRankingByRangeAAsync");
    const auto friendsA = Resolve<FriendsFn>("sceNpScoreGetFriendsRankingA");
    const auto friendsAAsync = Resolve<FriendsFn>("sceNpScoreGetFriendsRankingAAsync");
    const int req = NewRequest();
    Require(rangeA(req, 1, 0, ranksA, 0x90, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, nullptr) == InvalidArgument, "sceNpScoreGetRankingByRangeA", "rank 0 was taken");
    Require(rangeA(req, 1, 1, ranksA, 0x80, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, nullptr) == InvalidAlignment, "sceNpScoreGetRankingByRangeA", "a rank array size other than 0x90 per rank was taken");
    Require(rangeA(req, 1, 1, ranksA, 0x120, comments, 64, nullptr, 0, 2, nullptr, nullptr, nullptr) == InvalidAlignment, "sceNpScoreGetRankingByRangeA", "a comment array size other than 64 per rank was taken");
    CheckOffline("sceNpScoreGetRankingByRangeA", rangeA(req, 1, 1, ranksA, 0x120, comments, 128, infos, 400, 2, nullptr, nullptr, nullptr), req);
    Require(rangeAAsync(req, 1, 1, ranksA, 0x90, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, nullptr) == SignedOut, "sceNpScoreGetRankingByRangeAAsync", "valid arguments did not return SIGNED_OUT");
    Require(friendsA(req, 1, 1, nullptr, 0x90, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, nullptr) == InvalidArgument, "sceNpScoreGetFriendsRankingA", "a null rank array was taken");
    Require(friendsA(req, 1, 1, ranksA, 0x80, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, nullptr) == InvalidAlignment, "sceNpScoreGetFriendsRankingA", "a rank array size other than 0x90 per friend was taken");
    Require(friendsA(req, 1, 1, ranksA, 0x90, nullptr, 0, infos, 400, 1, nullptr, nullptr, nullptr) == InvalidAlignment, "sceNpScoreGetFriendsRankingA", "an info array size other than 200 per friend was taken");
    CheckOffline("sceNpScoreGetFriendsRankingA", friendsA(req, 1, 1, ranksA, 0x120, nullptr, 0, nullptr, 0, 2, nullptr, nullptr, nullptr), req);
    Require(friendsAAsync(req, 1, 0, ranksA, 0x90, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, nullptr) == SignedOut, "sceNpScoreGetFriendsRankingAAsync", "valid arguments did not return SIGNED_OUT");
    sceNpScoreDeleteRequest(req);
}

void CheckAccountIds() {
    const auto byAccountId = Resolve<AccountRankingFn>("sceNpScoreGetRankingByAccountId");
    const auto byAccountIdAsync = Resolve<AccountRankingFn>("sceNpScoreGetRankingByAccountIdAsync");
    const auto byAccountIdPcId = Resolve<RankingFn>("sceNpScoreGetRankingByAccountIdPcId");
    const auto byAccountIdPcIdAsync = Resolve<RankingFn>("sceNpScoreGetRankingByAccountIdPcIdAsync");
    int req = NewRequest();
    Require(byAccountId(req, 1, accountIds, 16, ranksA, 0x130, nullptr, 0, nullptr, 0, 2, nullptr, nullptr, board) == InvalidArgument, "sceNpScoreGetRankingByAccountId", "an option was taken");
    Require(byAccountId(req, 1, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0, 102, nullptr, nullptr, nullptr) == TooManyNpId, "sceNpScoreGetRankingByAccountId", "102 account ids were not refused before the null arrays");
    Require(byAccountId(req, 1, accountIds, 0, ranksA, 0, nullptr, 0, nullptr, 0, 0, nullptr, nullptr, nullptr) == InsufficientArgument, "sceNpScoreGetRankingByAccountId", "zero account ids were taken");
    Require(byAccountId(req, 1, nullptr, 16, ranksA, 0x130, nullptr, 0, nullptr, 0, 2, nullptr, nullptr, nullptr) == InsufficientArgument, "sceNpScoreGetRankingByAccountId", "a null account id array was taken");
    Require(byAccountId(req, 1, accountIds, 8, ranksA, 0x130, nullptr, 0, nullptr, 0, 2, nullptr, nullptr, nullptr) == InvalidAlignment, "sceNpScoreGetRankingByAccountId", "an account id array size other than 8 per id was taken");
    Require(byAccountId(req, 1, accountIds, 16, ranksA, 0x110, nullptr, 0, nullptr, 0, 2, nullptr, nullptr, nullptr) == InvalidAlignment, "sceNpScoreGetRankingByAccountId", "a rank array size other than 0x98 per id was taken");
    Require(byAccountId(req + 100, 1, accountIds, 16, ranksA, 0x130, nullptr, 0, nullptr, 0, 2, nullptr, nullptr, nullptr) == InvalidId, "sceNpScoreGetRankingByAccountId", "an unknown request was taken");
    CheckOffline("sceNpScoreGetRankingByAccountId", byAccountId(req, 1, accountIds, 16, ranksA, 0x130, comments, 128, infos, 400, 2, nullptr, nullptr, nullptr), req);
    Require(byAccountIdAsync(req, 1, accountIds, 8, ranksA, 0x98, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, nullptr) == SignedOut, "sceNpScoreGetRankingByAccountIdAsync", "valid arguments did not return SIGNED_OUT");
    sceNpScoreDeleteRequest(req);

    req = NewRequest();
    Require(byAccountIdPcId(req, 1, accountIdPcIds, 16, ranksA, 0x130, nullptr, 0, nullptr, 0, 2, nullptr, nullptr, nullptr) == InvalidAlignment, "sceNpScoreGetRankingByAccountIdPcId", "an id array size other than 16 per id was taken");
    Require(byAccountIdPcId(req, 1, accountIdPcIds, 32, ranksA, 0x120, nullptr, 0, nullptr, 0, 2, nullptr, nullptr, nullptr) == InvalidAlignment, "sceNpScoreGetRankingByAccountIdPcId", "a rank array size other than 0x98 per id was taken");
    CheckOffline("sceNpScoreGetRankingByAccountIdPcId", byAccountIdPcId(req, 1, accountIdPcIds, 32, ranksA, 0x130, nullptr, 0, nullptr, 0, 2, nullptr, nullptr, nullptr), req);
    Require(byAccountIdPcIdAsync(req, 1, accountIdPcIds, 16, ranksA, 0x98, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, nullptr) == SignedOut, "sceNpScoreGetRankingByAccountIdPcIdAsync", "valid arguments did not return SIGNED_OUT");
    sceNpScoreDeleteRequest(req);
}

void CheckNpIdPcId() {
    const auto byNpIdPcId = Resolve<RankingFn>("sceNpScoreGetRankingByNpIdPcId");
    const auto byNpIdPcIdAsync = Resolve<RankingFn>("sceNpScoreGetRankingByNpIdPcIdAsync");
    const int req = NewRequest();
    Require(byNpIdPcId(req, 1, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0, 102, nullptr, nullptr, nullptr) == InsufficientArgument, "sceNpScoreGetRankingByNpIdPcId", "null arrays were not refused before 102 ids");
    Require(byNpIdPcId(req, 1, npIdPcIds, 102 * 44, manyRanks, 102 * 0x88, nullptr, 0, nullptr, 0, 102, nullptr, nullptr, nullptr) == TooManyNpId, "sceNpScoreGetRankingByNpIdPcId", "102 ids were taken");
    Require(byNpIdPcId(req, 1, npIdPcIds, 72, ranks, 0x110, nullptr, 0, nullptr, 0, 2, nullptr, nullptr, nullptr) == InvalidAlignment, "sceNpScoreGetRankingByNpIdPcId", "an id array size other than 44 per id was taken");
    Require(byNpIdPcId(req, 1, npIdPcIds, 88, ranks, 0x100, nullptr, 0, nullptr, 0, 2, nullptr, nullptr, nullptr) == InvalidAlignment, "sceNpScoreGetRankingByNpIdPcId", "a rank array size other than 0x88 per id was taken");
    CheckOffline("sceNpScoreGetRankingByNpIdPcId", byNpIdPcId(req, 1, npIdPcIds, 88, ranks, 0x110, comments, 128, infos, 400, 2, nullptr, nullptr, nullptr), req);
    Require(byNpIdPcIdAsync(req, 1, npIdPcIds, 44, ranks, 0x88, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, nullptr) == SignedOut, "sceNpScoreGetRankingByNpIdPcIdAsync", "valid arguments did not return SIGNED_OUT");
    sceNpScoreDeleteRequest(req);
}

void CheckGameDataAndBoard() {
    const auto gameData = Resolve<GameDataFn>("sceNpScoreGetGameData");
    const auto gameDataAsync = Resolve<GameDataFn>("sceNpScoreGetGameDataAsync");
    const auto byAccountId = Resolve<GameDataByAccountIdFn>("sceNpScoreGetGameDataByAccountId");
    const auto byAccountIdAsync = Resolve<GameDataByAccountIdFn>("sceNpScoreGetGameDataByAccountIdAsync");
    const auto boardInfoAsync = Resolve<BoardInfoFn>("sceNpScoreGetBoardInfoAsync");
    unsigned char npId[36] = {'p', 'l', 'a', 'y', 'e', 'r'};
    const unsigned char emptyNpId[36] = {};
    std::uint64_t totalSize = 9;
    unsigned char data[16];
    int req = NewRequest();
    Require(gameData(req, 1, nullptr, &totalSize, sizeof(data), data, board) == InsufficientArgument, "sceNpScoreGetGameData", "a null NpId was not refused before the option");
    Require(gameData(req, 1, npId, &totalSize, sizeof(data), data, board) == InvalidArgument, "sceNpScoreGetGameData", "an option was taken");
    Require(gameData(req + 100, 1, npId, &totalSize, sizeof(data), data, nullptr) == InvalidId, "sceNpScoreGetGameData", "an unknown request was taken");
    CheckOffline("sceNpScoreGetGameData", gameData(req, 1, npId, &totalSize, sizeof(data), data, nullptr), req);
    Require(gameDataAsync(req, 1, npId, &totalSize, sizeof(data), data, nullptr) == SignedOut, "sceNpScoreGetGameDataAsync", "valid arguments did not return SIGNED_OUT");
    Require(totalSize == 9, "sceNpScoreGetGameData", "the total size was written");
    sceNpScoreDeleteRequest(req);
    req = NewRequest();
    int result = 0;
    Require(gameData(req, 1, emptyNpId, &totalSize, sizeof(data), data, nullptr) == InvalidArgument, "sceNpScoreGetGameData", "an empty online id was taken");
    Require(sceNpScorePollAsync(req, &result) == PollFinished && result == InvalidArgument, "sceNpScoreGetGameData", "an empty online id did not finish the request with INVALID_ARGUMENT");
    sceNpScoreDeleteRequest(req);

    req = NewRequest();
    Require(byAccountId(req, 1, 5, &totalSize, sizeof(data), data, board) == InvalidArgument, "sceNpScoreGetGameDataByAccountId", "an option was taken");
    Require(byAccountId(req, 1, 0, &totalSize, sizeof(data), data, nullptr) == InvalidArgument, "sceNpScoreGetGameDataByAccountId", "account id 0 was taken");
    Require(sceNpScorePollAsync(req, nullptr) == PollRunning, "sceNpScoreGetGameDataByAccountId", "a refused call finished the request");
    CheckOffline("sceNpScoreGetGameDataByAccountId", byAccountId(req, 1, 5, &totalSize, sizeof(data), data, nullptr), req);
    Require(byAccountIdAsync(req, 1, 5, &totalSize, sizeof(data), data, nullptr) == SignedOut, "sceNpScoreGetGameDataByAccountIdAsync", "valid arguments did not return SIGNED_OUT");
    sceNpScoreDeleteRequest(req);

    req = NewRequest();
    Require(boardInfoAsync(req, 1, board, board) == InvalidArgument, "sceNpScoreGetBoardInfoAsync", "an option was taken");
    CheckOffline("sceNpScoreGetBoardInfoAsync", boardInfoAsync(req, 1, board, nullptr), req);
    sceNpScoreDeleteRequest(req);
}

void CheckContextsAndWaiting() {
    const auto createA = Resolve<CreateTitleCtxAFn>("sceNpScoreCreateNpTitleCtxA");
    const auto setPcId = Resolve<SetPlayerCharacterIdFn>("sceNpScoreSetPlayerCharacterId");
    const auto waitAsync = Resolve<WaitAsyncFn>("sceNpScoreWaitAsync");
    Require(createA(0, 1) == SignedOut, "sceNpScoreCreateNpTitleCtxA", "a context was created for a signed-out user");
    Require(createA(0xFFFFFFFFu, 1) == SignedOut, "sceNpScoreCreateNpTitleCtxA", "the service label was checked before the sign-in");
    const int req = NewRequest();
    Require(setPcId(context, -1) == InvalidArgument, "sceNpScoreSetPlayerCharacterId", "a negative character id was taken");
    Require(setPcId(context, 3) == 0, "sceNpScoreSetPlayerCharacterId", "a title context was not found");
    Require(setPcId(req, 3) == 0, "sceNpScoreSetPlayerCharacterId", "a request was not found");
    Require(setPcId(999, 3) == InvalidId, "sceNpScoreSetPlayerCharacterId", "an unknown id was taken");
    int result = 0;
    Require(waitAsync(req + 100, &result) == InvalidId, "sceNpScoreWaitAsync", "an unknown request was taken");
    Require(sceNpScoreRecordScoreAsync(req, 1, 5, nullptr, nullptr, nullptr, nullptr, nullptr) == SignedOut, "sceNpScoreRecordScoreAsync", "valid arguments did not return SIGNED_OUT");
    Require(waitAsync(req, &result) == 0 && result == SignedOut, "sceNpScoreWaitAsync", "a finished request did not report SIGNED_OUT");
    Require(waitAsync(req, nullptr) == 0, "sceNpScoreWaitAsync", "a null result was not accepted");
    sceNpScoreDeleteRequest(req);
}

void CheckCrossSave() {
    const auto byRange = Resolve<RangeFn>("sceNpScoreGetRankingByRangeForCrossSave");
    const auto byRangeAsync = Resolve<RangeFn>("sceNpScoreGetRankingByRangeForCrossSaveAsync");
    const auto friends = Resolve<FriendsFn>("sceNpScoreGetFriendsRankingForCrossSave");
    const auto friendsAsync = Resolve<FriendsFn>("sceNpScoreGetFriendsRankingForCrossSaveAsync");
    const auto byAccountId = Resolve<AccountRankingFn>("sceNpScoreGetRankingByAccountIdForCrossSave");
    const auto byAccountIdAsync = Resolve<AccountRankingFn>("sceNpScoreGetRankingByAccountIdForCrossSaveAsync");
    const auto byAccountIdPcId = Resolve<RankingFn>("sceNpScoreGetRankingByAccountIdPcIdForCrossSave");
    const auto byAccountIdPcIdAsync = Resolve<RankingFn>("sceNpScoreGetRankingByAccountIdPcIdForCrossSaveAsync");
    int req = NewRequest();
    Require(sceNpScoreGetRankingByRange(req, 1, 1, ranksA, 2 * 0x90, nullptr, 0, nullptr, 0, 2, nullptr, nullptr, nullptr) == SignedOut, "sceNpScoreGetRankingByRange", "0x90 bytes per rank were not taken");
    Require(byRange(req, 1, 1, ranksA, 0x88, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, nullptr) == InvalidAlignment, "sceNpScoreGetRankingByRangeForCrossSave", "0x88 bytes per rank were taken");
    Require(byRange(req, 1, 0, ranksA, 0x80, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, nullptr) == InvalidArgument, "sceNpScoreGetRankingByRangeForCrossSave", "rank 0 was taken");
    CheckOffline("sceNpScoreGetRankingByRangeForCrossSave", byRange(req, 1, 1, ranksA, 0x90, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, nullptr), req);
    Require(byRangeAsync(req, 1, 1, ranks, 0x80, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, nullptr) == SignedOut, "sceNpScoreGetRankingByRangeForCrossSaveAsync", "valid arguments did not return SIGNED_OUT");
    sceNpScoreDeleteRequest(req);

    req = NewRequest();
    Require(friends(req, 1, 1, ranks, 0x88, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, nullptr) == InvalidAlignment, "sceNpScoreGetFriendsRankingForCrossSave", "0x88 bytes per friend were taken");
    CheckOffline("sceNpScoreGetFriendsRankingForCrossSave", friends(req, 1, 1, ranks, 0x80, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, nullptr), req);
    Require(friendsAsync(req, 1, 0, ranks, 0x80, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, nullptr) == SignedOut, "sceNpScoreGetFriendsRankingForCrossSaveAsync", "valid arguments did not return SIGNED_OUT");
    sceNpScoreDeleteRequest(req);

    req = NewRequest();
    Require(byAccountId(req, 1, accountIds, 4, ranksA, 0x98, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, nullptr) == InvalidAlignment, "sceNpScoreGetRankingByAccountIdForCrossSave", "an account id array size other than 8 per id was taken");
    CheckOffline("sceNpScoreGetRankingByAccountIdForCrossSave", byAccountId(req, 1, accountIds, 16, ranksA, 0x130, nullptr, 0, nullptr, 0, 2, nullptr, nullptr, nullptr), req);
    Require(byAccountIdAsync(req, 1, accountIds, 8, ranksA, 0x98, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, nullptr) == SignedOut, "sceNpScoreGetRankingByAccountIdForCrossSaveAsync", "valid arguments did not return SIGNED_OUT");
    Require(byAccountIdPcId(req, 1, accountIdPcIds, 8, ranksA, 0x98, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, nullptr) == InvalidAlignment, "sceNpScoreGetRankingByAccountIdPcIdForCrossSave", "an id array size other than 16 per id was taken");
    Require(byAccountIdPcId(req, 1, accountIdPcIds, 16, ranksA, 0x98, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, nullptr) == SignedOut, "sceNpScoreGetRankingByAccountIdPcIdForCrossSave", "valid arguments did not return SIGNED_OUT");
    Require(byAccountIdPcIdAsync(req, 1, accountIdPcIds, 16, ranksA, 0x98, nullptr, 0, nullptr, 0, 1, nullptr, nullptr, nullptr) == SignedOut, "sceNpScoreGetRankingByAccountIdPcIdForCrossSaveAsync", "valid arguments did not return SIGNED_OUT");
    sceNpScoreDeleteRequest(req);
}

void CheckSettings() {
    const auto setTimeout = Resolve<SetTimeoutFn>("sceNpScoreSetTimeout");
    const auto setThreadParam = Resolve<SetThreadParamFn>("sceNpScoreSetThreadParam");
    const auto createTitleCtx = Resolve<CreateTitleCtxFn>("sceNpScoreCreateTitleCtx");
    const int req = NewRequest();
    Require(setTimeout(context, 0, 0, 0, 0, 0) == 0, "sceNpScoreSetTimeout", "a title context was not found");
    Require(setTimeout(req, 1, 1000000, 10000000, 10000000, 10000000) == 0, "sceNpScoreSetTimeout", "the minimum timeouts were refused for a request");
    Require(setTimeout(999, 0, 0, 0, 0, 0) == InvalidId, "sceNpScoreSetTimeout", "an unknown id was taken");
    Require(setTimeout(999, -1, 0, 0, 0, 0) == InvalidArgument, "sceNpScoreSetTimeout", "a negative retry count was not refused before the id");
    Require(setTimeout(req, 0, 999999, 0, 0, 0) == InvalidArgument, "sceNpScoreSetTimeout", "a resolve timeout below 1 s was taken");
    Require(setTimeout(req, 0, -1, 0, 0, 0) == InvalidArgument, "sceNpScoreSetTimeout", "a negative resolve timeout was taken");
    Require(setTimeout(req, 0, 0, 9999999, 0, 0) == InvalidArgument, "sceNpScoreSetTimeout", "a connect timeout below 10 s was taken");
    Require(setTimeout(req, 0, 0, 0, 9999999, 0) == InvalidArgument, "sceNpScoreSetTimeout", "a send timeout below 10 s was taken");
    Require(setTimeout(req, 0, 0, 0, 0, 9999999) == InvalidArgument, "sceNpScoreSetTimeout", "a receive timeout below 10 s was taken");
    Require(setThreadParam(700, 0) == 0, "sceNpScoreSetThreadParam", "setting the worker thread parameters failed");
    unsigned char titleId[12] = {};
    unsigned char titleSecret[128] = {};
    Require(createTitleCtx(nullptr, titleSecret, selfNpId) == InsufficientArgument, "sceNpScoreCreateTitleCtx", "a null title id was taken");
    Require(createTitleCtx(titleId, nullptr, selfNpId) == InsufficientArgument, "sceNpScoreCreateTitleCtx", "a null title secret was taken");
    Require(createTitleCtx(titleId, titleSecret, nullptr) == InsufficientArgument, "sceNpScoreCreateTitleCtx", "a null NpId was taken");
    const int created = createTitleCtx(titleId, titleSecret, selfNpId);
    Require(created > 0, "sceNpScoreCreateTitleCtx", "no title context was created");
    Require(sceNpScoreCreateRequest(created) > 0, "sceNpScoreCreateTitleCtx", "the context does not take requests");
    Require(sceNpScoreDeleteNpTitleCtx(created) == 0, "sceNpScoreCreateTitleCtx", "the context could not be deleted");
    sceNpScoreDeleteRequest(req);
}

}

void CheckCommentsAndGameData() {
    const auto censor = Resolve<CensorFn>("sceNpScoreCensorComment");
    const auto censorAsync = Resolve<CensorFn>("sceNpScoreCensorCommentAsync");
    const auto sanitize = Resolve<SanitizeFn>("sceNpScoreSanitizeComment");
    const auto sanitizeAsync = Resolve<SanitizeFn>("sceNpScoreSanitizeCommentAsync");
    const auto recordGameData = Resolve<RecordGameDataFn>("sceNpScoreRecordGameData");
    const auto recordGameDataAsync = Resolve<RecordGameDataFn>("sceNpScoreRecordGameDataAsync");
    const auto changeMode = Resolve<ChangeModeFn>("sceNpScoreChangeModeForOtherSaveDataOwners");
    char longest[256];
    std::memset(longest, 'a', 255);
    longest[255] = '\0';
    char tooLong[257];
    std::memset(tooLong, 'a', 256);
    tooLong[256] = '\0';
    char sanitized[256];
    std::memset(sanitized, 0xA5, sizeof(sanitized));
    int option = 0;
    const unsigned char data[16] = {};

    for (const CensorFn call : {censor, censorAsync}) {
        const int req = NewRequest();
        Require(call(req, longest, &option) == InvalidArgument, "sceNpScoreCensorComment", "an option was taken");
        Require(call(req + 100, nullptr, nullptr) == InsufficientArgument, "sceNpScoreCensorComment", "a null comment was not refused before the request lookup");
        Require(call(req + 100, tooLong, nullptr) == InvalidId, "sceNpScoreCensorComment", "the request was not looked up before the length");
        Require(call(req, tooLong, nullptr) == InvalidArgument, "sceNpScoreCensorComment", "a 256-character comment was taken");
        Require(sceNpScorePollAsync(req, nullptr) == PollRunning, "sceNpScoreCensorComment", "a refused call finished the request");
        CheckOffline("sceNpScoreCensorComment", call(req, longest, nullptr), req);
        sceNpScoreDeleteRequest(req);
    }
    for (const SanitizeFn call : {sanitize, sanitizeAsync}) {
        const int req = NewRequest();
        Require(call(req, longest, sanitized, &option) == InvalidArgument, "sceNpScoreSanitizeComment", "an option was taken");
        Require(call(req + 100, nullptr, sanitized, nullptr) == InsufficientArgument, "sceNpScoreSanitizeComment", "a null comment was not refused before the request lookup");
        Require(call(req, tooLong, sanitized, nullptr) == InvalidArgument, "sceNpScoreSanitizeComment", "a 256-character comment was taken");
        CheckOffline("sceNpScoreSanitizeComment", call(req, longest, nullptr, nullptr), req);
        sceNpScoreDeleteRequest(req);
    }
    for (const unsigned char value : sanitized) Require(value == 0xA5, "sceNpScoreSanitizeComment", "the sanitized comment was written");
    for (const RecordGameDataFn call : {recordGameData, recordGameDataAsync}) {
        const int req = NewRequest();
        Require(call(req, 1, 5, 16, 16, data, &option) == InvalidArgument, "sceNpScoreRecordGameData", "an option was taken");
        Require(call(req + 100, 1, 5, 16, 16, nullptr, nullptr) == InsufficientArgument, "sceNpScoreRecordGameData", "null data was not refused before the request lookup");
        Require(call(req + 100, 1, 5, 16, 16, data, nullptr) == InvalidId, "sceNpScoreRecordGameData", "an unknown request was taken");
        CheckOffline("sceNpScoreRecordGameData", call(req, 1, 5, 4, 16, data, nullptr), req);
        sceNpScoreDeleteRequest(req);
    }
    const int req = NewRequest();
    Require(changeMode(req + 100, 4) == InvalidArgument && changeMode(req + 100, 0xFFFFFFFFu) == InvalidArgument, "sceNpScoreChangeModeForOtherSaveDataOwners", "a mode past 3 was not refused before the id");
    Require(changeMode(req + 100, 3) == InvalidId, "sceNpScoreChangeModeForOtherSaveDataOwners", "an unknown id was taken");
    Require(changeMode(context, 3) == 0 && changeMode(req, 0) == 0, "sceNpScoreChangeModeForOtherSaveDataOwners", "a title context or request was refused");
    Require(sceNpScorePollAsync(req, nullptr) == PollRunning, "sceNpScoreChangeModeForOtherSaveDataOwners", "changing the mode finished the request");
    sceNpScoreDeleteRequest(req);
}

int main() {
    std::memset(ranks, 0xA5, sizeof(ranks));
    std::memset(comments, 0xA5, sizeof(comments));
    std::memset(infos, 0xA5, sizeof(infos));
    std::memset(board, 0xA5, sizeof(board));
    std::memset(ranksA, 0xA5, sizeof(ranksA));
    CheckContexts();
    CheckRequests();
    CheckRecord();
    CheckRange();
    CheckNpId();
    CheckFriends();
    CheckBoard();
    CheckRangeAndFriendsA();
    CheckAccountIds();
    CheckNpIdPcId();
    CheckGameDataAndBoard();
    CheckContextsAndWaiting();
    CheckCrossSave();
    CheckSettings();
    CheckCommentsAndGameData();
    Require(Untouched(), "libSceNpScoreRanking", "an output array was written");
    if (failures != 0) return 1;
    std::puts("NpScoreRanking offline tests passed");
    return 0;
}
