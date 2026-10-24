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

}
