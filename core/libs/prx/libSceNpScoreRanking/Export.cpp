#include "prx/libSceNpScoreRanking/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static constexpr int SCE_NP_ERROR_SIGNED_OUT = static_cast<int>(0x80550006);
static constexpr int SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80550704);
static constexpr int SCE_NP_COMMUNITY_ERROR_TOO_MANY_OBJECTS = static_cast<int>(0x80550706);
static constexpr int SCE_NP_COMMUNITY_ERROR_ABORTED = static_cast<int>(0x80550707);
static constexpr int SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT = static_cast<int>(0x8055070C);
static constexpr int SCE_NP_COMMUNITY_ERROR_INVALID_ID = static_cast<int>(0x8055070E);
static constexpr int SCE_NP_COMMUNITY_ERROR_INVALID_ALIGNMENT = static_cast<int>(0x80550714);
static constexpr int SCE_NP_COMMUNITY_ERROR_TOO_MANY_NPID = static_cast<int>(0x80550719);
static constexpr std::uint32_t INVALID_SERVICE_LABEL = 0xFFFFFFFF;
static constexpr std::size_t MAX_OBJECTS = 32;
static constexpr int RESOLVE_TIMEOUT_MIN = 1000000;
static constexpr int TRANSFER_TIMEOUT_MIN = 10000000;
static constexpr std::uint64_t MAX_RANGE_PER_REQUEST = 100;
static constexpr std::uint64_t MAX_NPID_PER_REQUEST = 101;
static constexpr std::uint64_t GAME_INFO_MAX_SIZE = 189;
static constexpr std::uint64_t NP_ID_SIZE = 36;
static constexpr std::uint64_t RANK_DATA_SIZE = 0x80;
static constexpr std::uint64_t PLAYER_RANK_DATA_SIZE = 0x88;
static constexpr std::uint64_t RANK_DATA_A_SIZE = 0x90;
static constexpr std::uint64_t PLAYER_RANK_DATA_A_SIZE = 0x98;
static constexpr std::uint64_t ACCOUNT_ID_SIZE = 8;
static constexpr std::uint64_t NP_ID_PC_ID_SIZE = 44;
static constexpr std::uint64_t ACCOUNT_ID_PC_ID_SIZE = 16;
static constexpr std::uint64_t COMMENT_SIZE = 64;
static constexpr std::size_t COMMENT_MAX_LENGTH = 255;
static constexpr std::uint32_t SAVE_DATA_OWNER_MODE_MAX = 3;
static constexpr std::uint64_t GAME_INFO_SIZE = 200;
static constexpr std::uint64_t FRIEND_RANKING_OPTION_SIZE = 24;
static constexpr int POLL_FINISHED = 0;
static constexpr int POLL_RUNNING = 1;

static_assert(sizeof(NpScoreGameInfo) == GAME_INFO_SIZE);

static_assert(sizeof(NpScoreFriendRankingOption) == FRIEND_RANKING_OPTION_SIZE);

namespace {
struct Request {
    int titleCtxId = 0;
    std::optional<int> result;
};

std::mutex scoreMutex;
std::unordered_set<int> titleContexts;
std::unordered_map<int, Request> requests;
int nextTitleContext = 1;
int nextRequest = 1;

int CheckArrays(std::uint64_t arrayNum, const void* commentArray, std::uint64_t commentArraySize, const void* infoArray, std::uint64_t infoArraySize) {
    if (commentArray && commentArraySize != arrayNum * COMMENT_SIZE) return SCE_NP_COMMUNITY_ERROR_INVALID_ALIGNMENT;
    if (infoArray && infoArraySize != arrayNum * GAME_INFO_SIZE) return SCE_NP_COMMUNITY_ERROR_INVALID_ALIGNMENT;
    return 0;
}

int AddTitleContext() {
    std::lock_guard lock(scoreMutex);
    if (titleContexts.size() >= MAX_OBJECTS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_OBJECTS;
    const int titleCtxId = nextTitleContext++;
    titleContexts.insert(titleCtxId);
    return titleCtxId;
}

int Finish(int reqId, int error) {
    std::lock_guard lock(scoreMutex);
    const auto request = requests.find(reqId);
    if (request == requests.end()) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    if (request->second.result == SCE_NP_COMMUNITY_ERROR_ABORTED) return SCE_NP_COMMUNITY_ERROR_ABORTED;
    if (!request->second.result) request->second.result = error;
    return error;
}

int Offline(int reqId) {
    return Finish(reqId, SCE_NP_ERROR_SIGNED_OUT);
}

bool KnownRequest(int reqId) {
    std::lock_guard lock(scoreMutex);
    return requests.contains(reqId);
}

int CheckComment(int reqId, const char* comment, void* option) {
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (!comment) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (!KnownRequest(reqId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    if (strnlen(comment, COMMENT_MAX_LENGTH + 1) > COMMENT_MAX_LENGTH) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return Offline(reqId);
}

int RecordGameData(int reqId, const void* data, void* option) {
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (!data) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (!KnownRequest(reqId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    return Offline(reqId);
}

int RecordScore(int reqId, const NpScoreGameInfo* gameInfo, void* option) {
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (gameInfo && gameInfo->infoSize > GAME_INFO_MAX_SIZE) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return Offline(reqId);
}

int GetRankingByRange(int reqId, std::uint32_t startSerialRank, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* option, std::uint64_t rankDataSize = RANK_DATA_SIZE, std::uint64_t otherRankDataSize = RANK_DATA_A_SIZE) {
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (arrayNum > MAX_RANGE_PER_REQUEST) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (startSerialRank == 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (!rankArray || arrayNum == 0) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (rankArraySize != arrayNum * rankDataSize && rankArraySize != arrayNum * otherRankDataSize) return SCE_NP_COMMUNITY_ERROR_INVALID_ALIGNMENT;
    if (const int error = CheckArrays(arrayNum, commentArray, commentArraySize, infoArray, infoArraySize)) return error;
    return Offline(reqId);
}

int GetRankingByNpId(int reqId, const void* npIdArray, std::uint64_t npIdArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* option) {
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (arrayNum > MAX_NPID_PER_REQUEST) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_NPID;
    if (!npIdArray || !rankArray) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (npIdArraySize != arrayNum * NP_ID_SIZE || rankArraySize != arrayNum * PLAYER_RANK_DATA_SIZE) return SCE_NP_COMMUNITY_ERROR_INVALID_ALIGNMENT;
    if (const int error = CheckArrays(arrayNum, commentArray, commentArraySize, infoArray, infoArraySize)) return error;
    return Offline(reqId);
}

int GetFriendsRanking(int reqId, int includeSelf, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, const NpScoreFriendRankingOption* option, std::uint64_t rankDataSize = RANK_DATA_SIZE) {
    if (!rankArray) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (rankArraySize != arrayNum * rankDataSize) return SCE_NP_COMMUNITY_ERROR_INVALID_ALIGNMENT;
    std::uint32_t startSerialRank = 1;
    if (includeSelf == 0) {
        if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    } else if (option) {
        if (option->size != FRIEND_RANKING_OPTION_SIZE) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
        if (option->startSerialRank) startSerialRank = *option->startSerialRank;
    }
    if (arrayNum > MAX_RANGE_PER_REQUEST) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (startSerialRank == 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (arrayNum == 0) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (const int error = CheckArrays(arrayNum, commentArray, commentArraySize, infoArray, infoArraySize)) return error;
    return Offline(reqId);
}

int GetRankingByAccountId(int reqId, const void* accountIdArray, std::uint64_t accountIdArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* option, std::uint64_t idSize) {
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (arrayNum > MAX_NPID_PER_REQUEST) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_NPID;
    if (arrayNum == 0 || !rankArray || !accountIdArray) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (accountIdArraySize != arrayNum * idSize) return SCE_NP_COMMUNITY_ERROR_INVALID_ALIGNMENT;
    if (rankArraySize != arrayNum * PLAYER_RANK_DATA_A_SIZE) return SCE_NP_COMMUNITY_ERROR_INVALID_ALIGNMENT;
    if (const int error = CheckArrays(arrayNum, commentArray, commentArraySize, infoArray, infoArraySize)) return error;
    return Offline(reqId);
}

int GetRankingByNpIdPcId(int reqId, const void* idArray, std::uint64_t idArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* option) {
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (!idArray || arrayNum == 0 || !rankArray) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (arrayNum > MAX_NPID_PER_REQUEST) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_NPID;
    if (idArraySize != arrayNum * NP_ID_PC_ID_SIZE || rankArraySize != arrayNum * PLAYER_RANK_DATA_SIZE) return SCE_NP_COMMUNITY_ERROR_INVALID_ALIGNMENT;
    if (const int error = CheckArrays(arrayNum, commentArray, commentArraySize, infoArray, infoArraySize)) return error;
    return Offline(reqId);
}

int GetGameData(int reqId, const void* npId, void* option) {
    if (!npId) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (*static_cast<const char*>(npId) == '\0') return Finish(reqId, SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT);
    return Offline(reqId);
}

int GetGameDataByAccountId(int reqId, std::uint64_t accountId, void* option) {
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (accountId == 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return Offline(reqId);
}
}

extern "C" {

int APS5_VABI sceNpScoreAbortRequest(int reqId) {
    std::lock_guard lock(scoreMutex);
    const auto request = requests.find(reqId);
    if (request == requests.end()) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    if (!request->second.result) request->second.result = SCE_NP_COMMUNITY_ERROR_ABORTED;
    return 0;
}

int APS5_VABI sceNpScoreCreateNpTitleCtx(std::uint32_t serviceLabel, const void* selfNpId) {
    if (serviceLabel == INVALID_SERVICE_LABEL) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (!selfNpId) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    return AddTitleContext();
}

int APS5_VABI sceNpScoreCreateRequest(int titleCtxId) {
    std::lock_guard lock(scoreMutex);
    if (!titleContexts.contains(titleCtxId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    if (requests.size() >= MAX_OBJECTS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_OBJECTS;
    const int reqId = nextRequest++;
    requests.emplace(reqId, Request{titleCtxId});
    return reqId;
}

int APS5_VABI sceNpScoreDeleteNpTitleCtx(int titleCtxId) {
    std::lock_guard lock(scoreMutex);
    if (!titleContexts.erase(titleCtxId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    std::erase_if(requests, [titleCtxId](const auto& entry) { return entry.second.titleCtxId == titleCtxId; });
    return 0;
}

int APS5_VABI sceNpScoreDeleteRequest(int reqId) {
    std::lock_guard lock(scoreMutex);
    return requests.erase(reqId) ? 0 : SCE_NP_COMMUNITY_ERROR_INVALID_ID;
}

int APS5_VABI sceNpScoreGetBoardInfo(int reqId, std::uint32_t boardId, void* boardInfo, void* option) {
    (void)boardId;
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (!boardInfo) throw std::runtime_error("sceNpScoreGetBoardInfo: the answer for a null board info depends on the title's SDK version");
    return Offline(reqId);
}

int APS5_VABI sceNpScoreGetFriendsRanking(int reqId, std::uint32_t boardId, int includeSelf, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, const NpScoreFriendRankingOption* option) {
    (void)boardId;
    (void)lastSortDate;
    (void)totalRecord;
    return GetFriendsRanking(reqId, includeSelf, rankArray, rankArraySize, commentArray, commentArraySize, infoArray, infoArraySize, arrayNum, option);
}

int APS5_VABI sceNpScoreGetFriendsRankingAsync(int reqId, std::uint32_t boardId, int includeSelf, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, const NpScoreFriendRankingOption* option) {
    (void)boardId;
    (void)lastSortDate;
    (void)totalRecord;
    return GetFriendsRanking(reqId, includeSelf, rankArray, rankArraySize, commentArray, commentArraySize, infoArray, infoArraySize, arrayNum, option);
}

int APS5_VABI sceNpScoreGetRankingByNpId(int reqId, std::uint32_t boardId, const void* npIdArray, std::uint64_t npIdArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option) {
    (void)boardId;
    (void)lastSortDate;
    (void)totalRecord;
    return GetRankingByNpId(reqId, npIdArray, npIdArraySize, rankArray, rankArraySize, commentArray, commentArraySize, infoArray, infoArraySize, arrayNum, option);
}

int APS5_VABI sceNpScoreGetRankingByNpIdAsync(int reqId, std::uint32_t boardId, const void* npIdArray, std::uint64_t npIdArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option) {
    (void)boardId;
    (void)lastSortDate;
    (void)totalRecord;
    return GetRankingByNpId(reqId, npIdArray, npIdArraySize, rankArray, rankArraySize, commentArray, commentArraySize, infoArray, infoArraySize, arrayNum, option);
}

int APS5_VABI sceNpScoreGetRankingByRange(int reqId, std::uint32_t boardId, std::uint32_t startSerialRank, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option) {
    (void)boardId;
    (void)lastSortDate;
    (void)totalRecord;
    return GetRankingByRange(reqId, startSerialRank, rankArray, rankArraySize, commentArray, commentArraySize, infoArray, infoArraySize, arrayNum, option);
}

int APS5_VABI sceNpScoreGetRankingByRangeAsync(int reqId, std::uint32_t boardId, std::uint32_t startSerialRank, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option) {
    (void)boardId;
    (void)lastSortDate;
    (void)totalRecord;
    return GetRankingByRange(reqId, startSerialRank, rankArray, rankArraySize, commentArray, commentArraySize, infoArray, infoArraySize, arrayNum, option);
}

int APS5_VABI sceNpScorePollAsync(int reqId, int* result) {
    std::lock_guard lock(scoreMutex);
    const auto request = requests.find(reqId);
    if (request == requests.end()) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    if (!request->second.result) return POLL_RUNNING;
    if (result) *result = *request->second.result;
    return POLL_FINISHED;
}

int APS5_VABI sceNpScoreRecordScore(int reqId, std::uint32_t boardId, std::int64_t score, const void* scoreComment, const NpScoreGameInfo* gameInfo, std::uint32_t* tmpRank, const void* compareDate, void* option) {
    (void)boardId;
    (void)score;
    (void)scoreComment;
    (void)tmpRank;
    (void)compareDate;
    return RecordScore(reqId, gameInfo, option);
}

int APS5_VABI sceNpScoreRecordScoreAsync(int reqId, std::uint32_t boardId, std::int64_t score, const void* scoreComment, const NpScoreGameInfo* gameInfo, std::uint32_t* tmpRank, const void* compareDate, void* option) {
    (void)boardId;
    (void)score;
    (void)scoreComment;
    (void)tmpRank;
    (void)compareDate;
    return RecordScore(reqId, gameInfo, option);
}

int APS5_VABI sceNpScoreCensorComment(int reqId, const char* comment, void* option) {
    return CheckComment(reqId, comment, option);
}

int APS5_VABI sceNpScoreCensorCommentAsync(int reqId, const char* comment, void* option) {
    return CheckComment(reqId, comment, option);
}

int APS5_VABI sceNpScoreChangeModeForOtherSaveDataOwners(int id, std::uint32_t mode) {
    if (mode > SAVE_DATA_OWNER_MODE_MAX) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(scoreMutex);
    return titleContexts.contains(id) || requests.contains(id) ? 0 : SCE_NP_COMMUNITY_ERROR_INVALID_ID;
}

int APS5_VABI sceNpScoreCreateNpTitleCtxA(std::uint32_t serviceLabel, int selfId) {
    (void)serviceLabel;
    (void)selfId;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpScoreCreateTitleCtx(const void* titleId, const void* titleSecret, const void* selfNpId) {
    std::fputs("sceNpScoreCreateTitleCtx can use only for sp-int, so you can't use this at product.\nPlease use sceNpScoreCreateNpTitleCtx().\nNP Service label is published at NP title's devnet thread.\n", stderr);
    if (!titleId || !titleSecret || !selfNpId) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    return AddTitleContext();
}

int APS5_VABI sceNpScoreGetBoardInfoAsync(int reqId, std::uint32_t boardId, void* boardInfo, void* option) {
    (void)boardId;
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (!boardInfo) throw std::runtime_error("sceNpScoreGetBoardInfoAsync: the answer for a null board info depends on the title's SDK version");
    return Offline(reqId);
}

int APS5_VABI sceNpScoreGetFriendsRankingA(int reqId, std::uint32_t boardId, int includeSelf, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, const NpScoreFriendRankingOption* option) {
    (void)boardId;
    (void)lastSortDate;
    (void)totalRecord;
    return GetFriendsRanking(reqId, includeSelf, rankArray, rankArraySize, commentArray, commentArraySize, infoArray, infoArraySize, arrayNum, option, RANK_DATA_A_SIZE);
}

int APS5_VABI sceNpScoreGetFriendsRankingAAsync(int reqId, std::uint32_t boardId, int includeSelf, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, const NpScoreFriendRankingOption* option) {
    (void)boardId;
    (void)lastSortDate;
    (void)totalRecord;
    return GetFriendsRanking(reqId, includeSelf, rankArray, rankArraySize, commentArray, commentArraySize, infoArray, infoArraySize, arrayNum, option, RANK_DATA_A_SIZE);
}

int APS5_VABI sceNpScoreGetFriendsRankingForCrossSave(int reqId, std::uint32_t boardId, int includeSelf, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, const NpScoreFriendRankingOption* option) {
    (void)boardId;
    (void)lastSortDate;
    (void)totalRecord;
    return GetFriendsRanking(reqId, includeSelf, rankArray, rankArraySize, commentArray, commentArraySize, infoArray, infoArraySize, arrayNum, option);
}

int APS5_VABI sceNpScoreGetFriendsRankingForCrossSaveAsync(int reqId, std::uint32_t boardId, int includeSelf, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, const NpScoreFriendRankingOption* option) {
    (void)boardId;
    (void)lastSortDate;
    (void)totalRecord;
    return GetFriendsRanking(reqId, includeSelf, rankArray, rankArraySize, commentArray, commentArraySize, infoArray, infoArraySize, arrayNum, option);
}

int APS5_VABI sceNpScoreGetGameData(int reqId, std::uint32_t boardId, const void* npId, std::uint64_t* totalSize, std::uint64_t recvSize, void* data, void* option) {
    (void)boardId;
    (void)totalSize;
    (void)recvSize;
    (void)data;
    return GetGameData(reqId, npId, option);
}

int APS5_VABI sceNpScoreGetGameDataAsync(int reqId, std::uint32_t boardId, const void* npId, std::uint64_t* totalSize, std::uint64_t recvSize, void* data, void* option) {
    (void)boardId;
    (void)totalSize;
    (void)recvSize;
    (void)data;
    return GetGameData(reqId, npId, option);
}

int APS5_VABI sceNpScoreGetGameDataByAccountId(int reqId, std::uint32_t boardId, std::uint64_t accountId, std::uint64_t* totalSize, std::uint64_t recvSize, void* data, void* option) {
    (void)boardId;
    (void)totalSize;
    (void)recvSize;
    (void)data;
    return GetGameDataByAccountId(reqId, accountId, option);
}

int APS5_VABI sceNpScoreGetGameDataByAccountIdAsync(int reqId, std::uint32_t boardId, std::uint64_t accountId, std::uint64_t* totalSize, std::uint64_t recvSize, void* data, void* option) {
    (void)boardId;
    (void)totalSize;
    (void)recvSize;
    (void)data;
    return GetGameDataByAccountId(reqId, accountId, option);
}

int APS5_VABI sceNpScoreGetRankingByAccountId(int reqId, std::uint32_t boardId, const std::uint64_t* accountIdArray, std::uint64_t accountIdArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option) {
    (void)boardId;
    (void)lastSortDate;
    (void)totalRecord;
    return GetRankingByAccountId(reqId, accountIdArray, accountIdArraySize, rankArray, rankArraySize, commentArray, commentArraySize, infoArray, infoArraySize, arrayNum, option, ACCOUNT_ID_SIZE);
}

int APS5_VABI sceNpScoreGetRankingByAccountIdAsync(int reqId, std::uint32_t boardId, const std::uint64_t* accountIdArray, std::uint64_t accountIdArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option) {
    (void)boardId;
    (void)lastSortDate;
    (void)totalRecord;
    return GetRankingByAccountId(reqId, accountIdArray, accountIdArraySize, rankArray, rankArraySize, commentArray, commentArraySize, infoArray, infoArraySize, arrayNum, option, ACCOUNT_ID_SIZE);
}

int APS5_VABI sceNpScoreGetRankingByAccountIdForCrossSave(int reqId, std::uint32_t boardId, const std::uint64_t* accountIdArray, std::uint64_t accountIdArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option) {
    (void)boardId;
    (void)lastSortDate;
    (void)totalRecord;
    return GetRankingByAccountId(reqId, accountIdArray, accountIdArraySize, rankArray, rankArraySize, commentArray, commentArraySize, infoArray, infoArraySize, arrayNum, option, ACCOUNT_ID_SIZE);
}

int APS5_VABI sceNpScoreGetRankingByAccountIdForCrossSaveAsync(int reqId, std::uint32_t boardId, const std::uint64_t* accountIdArray, std::uint64_t accountIdArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option) {
    (void)boardId;
    (void)lastSortDate;
    (void)totalRecord;
    return GetRankingByAccountId(reqId, accountIdArray, accountIdArraySize, rankArray, rankArraySize, commentArray, commentArraySize, infoArray, infoArraySize, arrayNum, option, ACCOUNT_ID_SIZE);
}

int APS5_VABI sceNpScoreGetRankingByAccountIdPcId(int reqId, std::uint32_t boardId, const void* idArray, std::uint64_t idArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option) {
    (void)boardId;
    (void)lastSortDate;
    (void)totalRecord;
    return GetRankingByAccountId(reqId, idArray, idArraySize, rankArray, rankArraySize, commentArray, commentArraySize, infoArray, infoArraySize, arrayNum, option, ACCOUNT_ID_PC_ID_SIZE);
}

int APS5_VABI sceNpScoreGetRankingByAccountIdPcIdAsync(int reqId, std::uint32_t boardId, const void* idArray, std::uint64_t idArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option) {
    (void)boardId;
    (void)lastSortDate;
    (void)totalRecord;
    return GetRankingByAccountId(reqId, idArray, idArraySize, rankArray, rankArraySize, commentArray, commentArraySize, infoArray, infoArraySize, arrayNum, option, ACCOUNT_ID_PC_ID_SIZE);
}

int APS5_VABI sceNpScoreGetRankingByAccountIdPcIdForCrossSave(int reqId, std::uint32_t boardId, const void* idArray, std::uint64_t idArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option) {
    (void)boardId;
    (void)lastSortDate;
    (void)totalRecord;
    return GetRankingByAccountId(reqId, idArray, idArraySize, rankArray, rankArraySize, commentArray, commentArraySize, infoArray, infoArraySize, arrayNum, option, ACCOUNT_ID_PC_ID_SIZE);
}

int APS5_VABI sceNpScoreGetRankingByAccountIdPcIdForCrossSaveAsync(int reqId, std::uint32_t boardId, const void* idArray, std::uint64_t idArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option) {
    (void)boardId;
    (void)lastSortDate;
    (void)totalRecord;
    return GetRankingByAccountId(reqId, idArray, idArraySize, rankArray, rankArraySize, commentArray, commentArraySize, infoArray, infoArraySize, arrayNum, option, ACCOUNT_ID_PC_ID_SIZE);
}

int APS5_VABI sceNpScoreGetRankingByNpIdPcId(int reqId, std::uint32_t boardId, const void* idArray, std::uint64_t idArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option) {
    (void)boardId;
    (void)lastSortDate;
    (void)totalRecord;
    return GetRankingByNpIdPcId(reqId, idArray, idArraySize, rankArray, rankArraySize, commentArray, commentArraySize, infoArray, infoArraySize, arrayNum, option);
}

int APS5_VABI sceNpScoreGetRankingByNpIdPcIdAsync(int reqId, std::uint32_t boardId, const void* idArray, std::uint64_t idArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option) {
    (void)boardId;
    (void)lastSortDate;
    (void)totalRecord;
    return GetRankingByNpIdPcId(reqId, idArray, idArraySize, rankArray, rankArraySize, commentArray, commentArraySize, infoArray, infoArraySize, arrayNum, option);
}

int APS5_VABI sceNpScoreGetRankingByRangeA(int reqId, std::uint32_t boardId, std::uint32_t startSerialRank, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option) {
    (void)boardId;
    (void)lastSortDate;
    (void)totalRecord;
    return GetRankingByRange(reqId, startSerialRank, rankArray, rankArraySize, commentArray, commentArraySize, infoArray, infoArraySize, arrayNum, option, RANK_DATA_A_SIZE);
}

int APS5_VABI sceNpScoreGetRankingByRangeAAsync(int reqId, std::uint32_t boardId, std::uint32_t startSerialRank, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option) {
    (void)boardId;
    (void)lastSortDate;
    (void)totalRecord;
    return GetRankingByRange(reqId, startSerialRank, rankArray, rankArraySize, commentArray, commentArraySize, infoArray, infoArraySize, arrayNum, option, RANK_DATA_A_SIZE);
}

int APS5_VABI sceNpScoreGetRankingByRangeForCrossSave(int reqId, std::uint32_t boardId, std::uint32_t startSerialRank, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option) {
    (void)boardId;
    (void)lastSortDate;
    (void)totalRecord;
    return GetRankingByRange(reqId, startSerialRank, rankArray, rankArraySize, commentArray, commentArraySize, infoArray, infoArraySize, arrayNum, option);
}

int APS5_VABI sceNpScoreGetRankingByRangeForCrossSaveAsync(int reqId, std::uint32_t boardId, std::uint32_t startSerialRank, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option) {
    (void)boardId;
    (void)lastSortDate;
    (void)totalRecord;
    return GetRankingByRange(reqId, startSerialRank, rankArray, rankArraySize, commentArray, commentArraySize, infoArray, infoArraySize, arrayNum, option);
}

int APS5_VABI sceNpScoreRecordGameData(int reqId, std::uint32_t boardId, std::int64_t score, std::uint64_t totalSize, std::uint64_t sendSize, const void* data, void* option) {
    (void)boardId;
    (void)score;
    (void)totalSize;
    (void)sendSize;
    return RecordGameData(reqId, data, option);
}

int APS5_VABI sceNpScoreRecordGameDataAsync(int reqId, std::uint32_t boardId, std::int64_t score, std::uint64_t totalSize, std::uint64_t sendSize, const void* data, void* option) {
    (void)boardId;
    (void)score;
    (void)totalSize;
    (void)sendSize;
    return RecordGameData(reqId, data, option);
}

int APS5_VABI sceNpScoreSanitizeComment(int reqId, const char* comment, char* sanitizedComment, void* option) {
    (void)sanitizedComment;
    return CheckComment(reqId, comment, option);
}

int APS5_VABI sceNpScoreSanitizeCommentAsync(int reqId, const char* comment, char* sanitizedComment, void* option) {
    (void)sanitizedComment;
    return CheckComment(reqId, comment, option);
}

int APS5_VABI sceNpScoreSetPlayerCharacterId(int ctxId, int pcId) {
    if (pcId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(scoreMutex);
    return titleContexts.contains(ctxId) || requests.contains(ctxId) ? 0 : SCE_NP_COMMUNITY_ERROR_INVALID_ID;
}

int APS5_VABI sceNpScoreSetThreadParam(int threadPriority, std::uint64_t cpuAffinityMask) {
    (void)threadPriority;
    (void)cpuAffinityMask;
    return 0;
}

int APS5_VABI sceNpScoreSetTimeout(int id, int resolveRetry, int resolveTimeout, int connTimeout, int sendTimeout, int recvTimeout) {
    if (resolveRetry < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (resolveTimeout != 0 && resolveTimeout < RESOLVE_TIMEOUT_MIN) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (connTimeout != 0 && connTimeout < TRANSFER_TIMEOUT_MIN) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (recvTimeout != 0 && recvTimeout < TRANSFER_TIMEOUT_MIN) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (sendTimeout != 0 && sendTimeout < TRANSFER_TIMEOUT_MIN) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(scoreMutex);
    return titleContexts.contains(id) || requests.contains(id) ? 0 : SCE_NP_COMMUNITY_ERROR_INVALID_ID;
}

int APS5_VABI sceNpScoreWaitAsync(int reqId, int* result) {
    std::lock_guard lock(scoreMutex);
    const auto request = requests.find(reqId);
    if (request == requests.end()) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    if (!request->second.result) NotImplemented_nid_no_patch("sceNpScoreWaitAsync (a request with no operation started)");
    if (result) *result = *request->second.result;
    return 0;
}

}
