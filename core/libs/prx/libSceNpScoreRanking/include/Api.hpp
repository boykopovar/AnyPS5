#pragma once

#include "SceTypes.hpp"

struct NpScoreFriendRankingOption {
    std::uint64_t size;
    const std::uint32_t* startSerialRank;
    const std::uint32_t* hits;
};

struct NpScoreGameInfo {
    std::uint64_t infoSize;
    std::uint8_t data[189];
    std::uint8_t pad[3];
};

extern "C" {

int APS5_VABI sceNpScoreAbortRequest(int reqId);
int APS5_VABI sceNpScoreCreateNpTitleCtx(std::uint32_t serviceLabel, const void* selfNpId);
int APS5_VABI sceNpScoreCreateRequest(int titleCtxId);
int APS5_VABI sceNpScoreDeleteNpTitleCtx(int titleCtxId);
int APS5_VABI sceNpScoreDeleteRequest(int reqId);
int APS5_VABI sceNpScoreGetBoardInfo(int reqId, std::uint32_t boardId, void* boardInfo, void* option);
int APS5_VABI sceNpScoreGetFriendsRanking(int reqId, std::uint32_t boardId, int includeSelf, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, const NpScoreFriendRankingOption* option);
int APS5_VABI sceNpScoreGetFriendsRankingAsync(int reqId, std::uint32_t boardId, int includeSelf, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, const NpScoreFriendRankingOption* option);
int APS5_VABI sceNpScoreGetRankingByNpId(int reqId, std::uint32_t boardId, const void* npIdArray, std::uint64_t npIdArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option);
int APS5_VABI sceNpScoreGetRankingByNpIdAsync(int reqId, std::uint32_t boardId, const void* npIdArray, std::uint64_t npIdArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option);
int APS5_VABI sceNpScoreGetRankingByRange(int reqId, std::uint32_t boardId, std::uint32_t startSerialRank, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option);
int APS5_VABI sceNpScoreGetRankingByRangeAsync(int reqId, std::uint32_t boardId, std::uint32_t startSerialRank, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option);
int APS5_VABI sceNpScorePollAsync(int reqId, int* result);
int APS5_VABI sceNpScoreRecordScore(int reqId, std::uint32_t boardId, std::int64_t score, const void* scoreComment, const NpScoreGameInfo* gameInfo, std::uint32_t* tmpRank, const void* compareDate, void* option);
int APS5_VABI sceNpScoreRecordScoreAsync(int reqId, std::uint32_t boardId, std::int64_t score, const void* scoreComment, const NpScoreGameInfo* gameInfo, std::uint32_t* tmpRank, const void* compareDate, void* option);

int APS5_VABI sceNpScoreCensorComment(int reqId, const char* comment, void* option);
int APS5_VABI sceNpScoreCensorCommentAsync(int reqId, const char* comment, void* option);
int APS5_VABI sceNpScoreChangeModeForOtherSaveDataOwners(int id, std::uint32_t mode);
int APS5_VABI sceNpScoreCreateNpTitleCtxA(std::uint32_t serviceLabel, int selfId);
int APS5_VABI sceNpScoreCreateTitleCtx(const void* titleId, const void* titleSecret, const void* selfNpId);
int APS5_VABI sceNpScoreGetBoardInfoAsync(int reqId, std::uint32_t boardId, void* boardInfo, void* option);
int APS5_VABI sceNpScoreGetFriendsRankingA(int reqId, std::uint32_t boardId, int includeSelf, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, const NpScoreFriendRankingOption* option);
int APS5_VABI sceNpScoreGetFriendsRankingAAsync(int reqId, std::uint32_t boardId, int includeSelf, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, const NpScoreFriendRankingOption* option);
int APS5_VABI sceNpScoreGetFriendsRankingForCrossSave(int reqId, std::uint32_t boardId, int includeSelf, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, const NpScoreFriendRankingOption* option);
int APS5_VABI sceNpScoreGetFriendsRankingForCrossSaveAsync(int reqId, std::uint32_t boardId, int includeSelf, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, const NpScoreFriendRankingOption* option);
int APS5_VABI sceNpScoreGetGameData(int reqId, std::uint32_t boardId, const void* npId, std::uint64_t* totalSize, std::uint64_t recvSize, void* data, void* option);
int APS5_VABI sceNpScoreGetGameDataAsync(int reqId, std::uint32_t boardId, const void* npId, std::uint64_t* totalSize, std::uint64_t recvSize, void* data, void* option);
int APS5_VABI sceNpScoreGetGameDataByAccountId(int reqId, std::uint32_t boardId, std::uint64_t accountId, std::uint64_t* totalSize, std::uint64_t recvSize, void* data, void* option);
int APS5_VABI sceNpScoreGetGameDataByAccountIdAsync(int reqId, std::uint32_t boardId, std::uint64_t accountId, std::uint64_t* totalSize, std::uint64_t recvSize, void* data, void* option);
int APS5_VABI sceNpScoreGetRankingByAccountId(int reqId, std::uint32_t boardId, const std::uint64_t* accountIdArray, std::uint64_t accountIdArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option);
int APS5_VABI sceNpScoreGetRankingByAccountIdAsync(int reqId, std::uint32_t boardId, const std::uint64_t* accountIdArray, std::uint64_t accountIdArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option);
int APS5_VABI sceNpScoreGetRankingByAccountIdForCrossSave(int reqId, std::uint32_t boardId, const std::uint64_t* accountIdArray, std::uint64_t accountIdArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option);
int APS5_VABI sceNpScoreGetRankingByAccountIdForCrossSaveAsync(int reqId, std::uint32_t boardId, const std::uint64_t* accountIdArray, std::uint64_t accountIdArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option);
int APS5_VABI sceNpScoreGetRankingByAccountIdPcId(int reqId, std::uint32_t boardId, const void* idArray, std::uint64_t idArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option);
int APS5_VABI sceNpScoreGetRankingByAccountIdPcIdAsync(int reqId, std::uint32_t boardId, const void* idArray, std::uint64_t idArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option);
int APS5_VABI sceNpScoreGetRankingByAccountIdPcIdForCrossSave(int reqId, std::uint32_t boardId, const void* idArray, std::uint64_t idArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option);
int APS5_VABI sceNpScoreGetRankingByAccountIdPcIdForCrossSaveAsync(int reqId, std::uint32_t boardId, const void* idArray, std::uint64_t idArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option);
int APS5_VABI sceNpScoreGetRankingByNpIdPcId(int reqId, std::uint32_t boardId, const void* idArray, std::uint64_t idArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option);
int APS5_VABI sceNpScoreGetRankingByNpIdPcIdAsync(int reqId, std::uint32_t boardId, const void* idArray, std::uint64_t idArraySize, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option);
int APS5_VABI sceNpScoreGetRankingByRangeA(int reqId, std::uint32_t boardId, std::uint32_t startSerialRank, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option);
int APS5_VABI sceNpScoreGetRankingByRangeAAsync(int reqId, std::uint32_t boardId, std::uint32_t startSerialRank, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option);
int APS5_VABI sceNpScoreGetRankingByRangeForCrossSave(int reqId, std::uint32_t boardId, std::uint32_t startSerialRank, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option);
int APS5_VABI sceNpScoreGetRankingByRangeForCrossSaveAsync(int reqId, std::uint32_t boardId, std::uint32_t startSerialRank, void* rankArray, std::uint64_t rankArraySize, void* commentArray, std::uint64_t commentArraySize, void* infoArray, std::uint64_t infoArraySize, std::uint64_t arrayNum, void* lastSortDate, void* totalRecord, void* option);
int APS5_VABI sceNpScoreRecordGameData(int reqId, std::uint32_t boardId, std::int64_t score, std::uint64_t totalSize, std::uint64_t sendSize, const void* data, void* option);
int APS5_VABI sceNpScoreRecordGameDataAsync(int reqId, std::uint32_t boardId, std::int64_t score, std::uint64_t totalSize, std::uint64_t sendSize, const void* data, void* option);
int APS5_VABI sceNpScoreSanitizeComment(int reqId, const char* comment, char* sanitizedComment, void* option);
int APS5_VABI sceNpScoreSanitizeCommentAsync(int reqId, const char* comment, char* sanitizedComment, void* option);
int APS5_VABI sceNpScoreSetPlayerCharacterId(int ctxId, int pcId);
int APS5_VABI sceNpScoreSetThreadParam(int threadPriority, std::uint64_t cpuAffinityMask);
int APS5_VABI sceNpScoreSetTimeout(int id, int resolveRetry, int resolveTimeout, int connTimeout, int sendTimeout, int recvTimeout);
int APS5_VABI sceNpScoreWaitAsync(int reqId, int* result);
}

