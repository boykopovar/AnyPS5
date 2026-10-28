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

int APS5_VABI sceNpScoreCensorComment();
int APS5_VABI sceNpScoreCensorCommentAsync();
int APS5_VABI sceNpScoreChangeModeForOtherSaveDataOwners();
int APS5_VABI sceNpScoreCreateNpTitleCtxA();
int APS5_VABI sceNpScoreCreateTitleCtx();
int APS5_VABI sceNpScoreGetBoardInfoAsync();
int APS5_VABI sceNpScoreGetFriendsRankingA();
int APS5_VABI sceNpScoreGetFriendsRankingAAsync();
int APS5_VABI sceNpScoreGetFriendsRankingForCrossSave();
int APS5_VABI sceNpScoreGetFriendsRankingForCrossSaveAsync();
int APS5_VABI sceNpScoreGetGameData();
int APS5_VABI sceNpScoreGetGameDataAsync();
int APS5_VABI sceNpScoreGetGameDataByAccountId();
int APS5_VABI sceNpScoreGetGameDataByAccountIdAsync();
int APS5_VABI sceNpScoreGetRankingByAccountId();
int APS5_VABI sceNpScoreGetRankingByAccountIdAsync();
int APS5_VABI sceNpScoreGetRankingByAccountIdForCrossSave();
int APS5_VABI sceNpScoreGetRankingByAccountIdForCrossSaveAsync();
int APS5_VABI sceNpScoreGetRankingByAccountIdPcId();
int APS5_VABI sceNpScoreGetRankingByAccountIdPcIdAsync();
int APS5_VABI sceNpScoreGetRankingByAccountIdPcIdForCrossSave();
int APS5_VABI sceNpScoreGetRankingByAccountIdPcIdForCrossSaveAsync();
int APS5_VABI sceNpScoreGetRankingByNpIdPcId();
int APS5_VABI sceNpScoreGetRankingByNpIdPcIdAsync();
int APS5_VABI sceNpScoreGetRankingByRangeA();
int APS5_VABI sceNpScoreGetRankingByRangeAAsync();
int APS5_VABI sceNpScoreGetRankingByRangeForCrossSave();
int APS5_VABI sceNpScoreGetRankingByRangeForCrossSaveAsync();
int APS5_VABI sceNpScoreRecordGameData();
int APS5_VABI sceNpScoreRecordGameDataAsync();
int APS5_VABI sceNpScoreSanitizeComment();
int APS5_VABI sceNpScoreSanitizeCommentAsync();
int APS5_VABI sceNpScoreSetPlayerCharacterId();
int APS5_VABI sceNpScoreSetThreadParam();
int APS5_VABI sceNpScoreSetTimeout();
int APS5_VABI sceNpScoreWaitAsync();
}

