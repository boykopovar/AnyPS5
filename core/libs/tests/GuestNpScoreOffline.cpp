#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" {
int APS5_VABI sceNpScoreCreateNpTitleCtx(std::uint32_t serviceLabel, const void* selfNpId);
int APS5_VABI sceNpScoreDeleteNpTitleCtx(int titleCtxId);
int APS5_VABI sceNpScoreCreateRequest(int titleCtxId);
int APS5_VABI sceNpScoreDeleteRequest(int reqId);
int APS5_VABI sceNpScoreAbortRequest(int reqId);
int APS5_VABI sceNpScorePollAsync(int reqId, int* result);
int APS5_VABI sceNpScoreRecordScore(int reqId, std::uint32_t boardId, std::int64_t score, const void* scoreComment, const void* gameInfo, std::uint32_t* tmpRank, const void* compareDate, void* option);
int APS5_VABI sceNpScoreRecordScoreAsync(int reqId, std::uint32_t boardId, std::int64_t score, const void* scoreComment, const void* gameInfo, std::uint32_t* tmpRank, const void* compareDate, void* option);
int APS5_VABI sceNpScoreGetRankingByRange(int reqId, std::uint32_t boardId, std::uint32_t startSerialRank, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option);
int APS5_VABI sceNpScoreGetRankingByRangeAsync(int reqId, std::uint32_t boardId, std::uint32_t startSerialRank, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option);
int APS5_VABI sceNpScoreGetRankingByNpId(int reqId, std::uint32_t boardId, const void* npIdArray, std::uint64_t npIdArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option);
int APS5_VABI sceNpScoreGetRankingByNpIdAsync(int reqId, std::uint32_t boardId, const void* npIdArray, std::uint64_t npIdArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option);
int APS5_VABI sceNpScoreGetFriendsRanking(int reqId, std::uint32_t boardId, int includeSelf, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, const void* option);
int APS5_VABI sceNpScoreGetFriendsRankingAsync(int reqId, std::uint32_t boardId, int includeSelf, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, const void* option);
int APS5_VABI sceNpScoreGetBoardInfo(int reqId, std::uint32_t boardId, void* boardInfo, void* option);
}

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

struct GameInfo {
    std::uint64_t infoSize;
    std::uint8_t data[189];
    std::uint8_t pad[3];
};

struct FriendOption {
    std::uint64_t size;
    const std::uint32_t* startSerialRank;
    const std::uint32_t* hits;
};

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
    GameInfo info{};
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
    const FriendOption shortOption{16, nullptr, nullptr};
    const FriendOption rankZero{24, &zero, nullptr};
    const FriendOption rankFive{24, &five, nullptr};
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

}

int main() {
    std::memset(ranks, 0xA5, sizeof(ranks));
    std::memset(comments, 0xA5, sizeof(comments));
    std::memset(infos, 0xA5, sizeof(infos));
    std::memset(board, 0xA5, sizeof(board));
    CheckContexts();
    CheckRequests();
    CheckRecord();
    CheckRange();
    CheckNpId();
    CheckFriends();
    CheckBoard();
    Require(Untouched(), "libSceNpScoreRanking", "an output array was written");
    if (failures != 0) return 1;
    std::puts("NpScoreRanking offline tests passed");
    return 0;
}
