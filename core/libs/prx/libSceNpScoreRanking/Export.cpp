#include "prx/libSceNpScoreRanking/include/Api.hpp"
#include <cstddef>
#include <cstdint>
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
static constexpr std::uint64_t MAX_RANGE_PER_REQUEST = 100;
static constexpr std::uint64_t MAX_NPID_PER_REQUEST = 101;
static constexpr std::uint64_t GAME_INFO_MAX_SIZE = 189;
static constexpr std::uint64_t NP_ID_SIZE = 36;
static constexpr std::uint64_t RANK_DATA_SIZE = 0x80;
static constexpr std::uint64_t PLAYER_RANK_DATA_SIZE = 0x88;
static constexpr std::uint64_t COMMENT_SIZE = 64;
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

int Offline(int reqId) {
    std::lock_guard lock(scoreMutex);
    const auto request = requests.find(reqId);
    if (request == requests.end()) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    if (request->second.result == SCE_NP_COMMUNITY_ERROR_ABORTED) return SCE_NP_COMMUNITY_ERROR_ABORTED;
    if (!request->second.result) request->second.result = SCE_NP_ERROR_SIGNED_OUT;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int RecordScore(int reqId, const NpScoreGameInfo* gameInfo, void* option) {
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (gameInfo && gameInfo->infoSize > GAME_INFO_MAX_SIZE) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return Offline(reqId);
}

int GetRankingByRange(int reqId, std::uint32_t startSerialRank, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* option) {
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (arrayNum > MAX_RANGE_PER_REQUEST) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (startSerialRank == 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (!rankArray || arrayNum == 0) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (rankArraySize != arrayNum * RANK_DATA_SIZE) return SCE_NP_COMMUNITY_ERROR_INVALID_ALIGNMENT;
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

int GetFriendsRanking(int reqId, int includeSelf, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, const NpScoreFriendRankingOption* option) {
    if (!rankArray) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (rankArraySize != arrayNum * RANK_DATA_SIZE) return SCE_NP_COMMUNITY_ERROR_INVALID_ALIGNMENT;
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
    std::lock_guard lock(scoreMutex);
    if (titleContexts.size() >= MAX_OBJECTS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_OBJECTS;
    const int titleCtxId = nextTitleContext++;
    titleContexts.insert(titleCtxId);
    return titleCtxId;
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

int APS5_VABI sceNpScoreCensorComment(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreCensorCommentAsync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreChangeModeForOtherSaveDataOwners(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreCreateNpTitleCtxA(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreCreateTitleCtx(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreGetBoardInfoAsync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreGetFriendsRankingA(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreGetFriendsRankingAAsync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreGetFriendsRankingForCrossSave(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreGetFriendsRankingForCrossSaveAsync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreGetGameData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreGetGameDataAsync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreGetGameDataByAccountId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreGetGameDataByAccountIdAsync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreGetRankingByAccountId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreGetRankingByAccountIdAsync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreGetRankingByAccountIdForCrossSave(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreGetRankingByAccountIdForCrossSaveAsync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreGetRankingByAccountIdPcId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreGetRankingByAccountIdPcIdAsync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreGetRankingByAccountIdPcIdForCrossSave(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreGetRankingByAccountIdPcIdForCrossSaveAsync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreGetRankingByNpIdPcId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreGetRankingByNpIdPcIdAsync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreGetRankingByRangeA(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreGetRankingByRangeAAsync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreGetRankingByRangeForCrossSave(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreGetRankingByRangeForCrossSaveAsync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreRecordGameData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreRecordGameDataAsync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreSanitizeComment(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreSanitizeCommentAsync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreSetPlayerCharacterId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreSetThreadParam(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreSetTimeout(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpScoreWaitAsync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
