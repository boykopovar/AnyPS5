#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceContentSearchInit(const ContentSearchInitParam* initParam);
int APS5_VABI sceContentSearchCloseMetadata(std::int32_t metadataId);
int APS5_VABI sceContentSearchGetMetadataFieldInfo(std::int32_t metadataId, const char* field, std::int32_t* metadataType, std::int32_t* size);
int APS5_VABI sceContentSearchGetMetadataValue(std::int32_t metadataId, const char* field, void* value);
int APS5_VABI sceContentSearchOpenMetadata(const char* filePath, std::int32_t* metadataId);
int APS5_VABI sceContentSearchSearchContent(const ContentSearchContentColumnSet* columnSet, std::uint32_t columnSetLength, const ContentSearchContentOrderByCondition* orderByConditions, std::uint32_t orderByConditionsLength, std::uint32_t offset, std::uint32_t limit, std::int64_t* numOfContent, void* infos, std::int64_t* lastUpdateId);
int APS5_VABI sceContentSearchGetContentLastUpdateId(std::int64_t* lastUpdateId);
int APS5_VABI sceContentSearchOpenMetadataByContentId(std::int64_t contentId, std::int32_t* metadataId);
int APS5_VABI sceContentSearchGetTotalContentSize(const ContentSearchContentColumnSet* columnSet, std::uint32_t columnSetLength, std::int64_t* size, std::int64_t* lastUpdateId);
int APS5_VABI sceContentSearchGetNumOfContent(const ContentSearchContentColumnSet* columnSet, std::uint32_t columnSetLength, std::int64_t* numOfContent, std::int64_t* lastUpdateId);
int APS5_VABI sceContentSearchTerm(void);
int APS5_VABI sceContentBinderEnd();
int APS5_VABI sceContentBinderGetNumOfDifference();
int APS5_VABI sceContentBinderInitialize();
int APS5_VABI sceContentBinderIsUpdated();
int APS5_VABI sceContentBinderReset();
int APS5_VABI sceContentBinderSearchDifference();
int APS5_VABI sceContentBinderStart();
int APS5_VABI sceContentBinderTerminate();
int APS5_VABI sceContentSearchGetApplicationLastUpdateId();
int APS5_VABI sceContentSearchGetMyApplicationIndex();
int APS5_VABI sceContentSearchGetNumOfApplication();
int APS5_VABI sceContentSearchSearchApplication();

}
