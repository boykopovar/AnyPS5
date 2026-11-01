#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpEntitlementAccessGetAddcontEntitlementInfo(uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel, NpEntitlementAccessAddcontEntitlementInfo* info);
int APS5_VABI sceNpEntitlementAccessGetAddcontEntitlementInfoList(uint32_t serviceLabel, NpEntitlementAccessAddcontEntitlementInfo* list, uint32_t listNum, uint32_t* hitNum);
int APS5_VABI sceNpEntitlementAccessGetSkuFlag(uint32_t* skuFlag);
int APS5_VABI sceNpEntitlementAccessGetEntitlementKey( uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel, NpEntitlementAccessEntitlementKey* key);
int APS5_VABI sceNpEntitlementAccessInitialize(const NpEntitlementAccessInitParam* initParam, NpEntitlementAccessBootParam* bootParam);
int APS5_VABI sceNpEntitlementAccessAbortRequest(std::int64_t requestId);
int APS5_VABI sceNpEntitlementAccessDeleteRequest(std::int64_t requestId);
int APS5_VABI sceNpEntitlementAccessGenerateTransactionId(NpEntitlementAccessTransactionId* transactionId);
int APS5_VABI sceNpEntitlementAccessPollConsumeEntitlement(std::int64_t requestId, std::int32_t* result, std::int32_t* countResult);
int APS5_VABI sceNpEntitlementAccessRequestConsumeUnifiedEntitlement(std::int32_t userId, std::uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel, const NpEntitlementAccessTransactionId* transactionId, std::int32_t count, std::int64_t* requestId);
int APS5_VABI sceNpEntitlementAccessRequestConsumeServiceEntitlement(std::int32_t userId, std::uint32_t serviceLabel, const NpServiceEntitlementLabel* entitlementLabel, const NpEntitlementAccessTransactionId* transactionId, std::int32_t count, std::int64_t* requestId);
int APS5_VABI sceNpEntitlementAccessPollServiceEntitlementInfo(std::int64_t requestId, std::int32_t* result, NpEntitlementAccessServiceInfo* info);
int APS5_VABI sceNpEntitlementAccessPollUnifiedEntitlementInfo(std::int64_t requestId, std::int32_t* result, NpEntitlementAccessUnifiedInfo* info);
int APS5_VABI sceNpEntitlementAccessRequestServiceEntitlementInfo(std::int32_t userId, std::uint32_t serviceLabel, const NpServiceEntitlementLabel* entitlementLabel, std::int64_t* requestId);
int APS5_VABI sceNpEntitlementAccessRequestUnifiedEntitlementInfo(std::int32_t userId, std::uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel, std::int64_t* requestId);
int APS5_VABI sceNpEntitlementAccessGetPftFlag(uint32_t* flag);
int APS5_VABI sceNpEntitlementAccessGetGameTrialsFlag(uint32_t* flag);
int APS5_VABI sceNpEntitlementAccessPollServiceEntitlementInfoList(std::int64_t requestId, std::int32_t* result, NpEntitlementAccessServiceInfo* infos, std::uint32_t capacity, std::int32_t* firstResult, std::int32_t* secondResult, std::int32_t* thirdResult);
int APS5_VABI sceNpEntitlementAccessPollUnifiedEntitlementInfoList(std::int64_t requestId, std::int32_t* result, NpEntitlementAccessUnifiedInfo* infos, std::uint32_t capacity, std::int32_t* firstResult, std::int32_t* secondResult, std::int32_t* thirdResult);
int APS5_VABI sceNpEntitlementAccessRequestServiceEntitlementInfoList(std::int32_t userId, std::uint32_t serviceLabel, const NpServiceEntitlementLabel* entitlementLabels, std::uint32_t labelCount, const NpEntitlementAccessListParameter* parameter, std::int64_t* requestId);
int APS5_VABI sceNpEntitlementAccessRequestUnifiedEntitlementInfoList(std::int32_t userId, std::uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabels, std::uint32_t labelCount, const NpEntitlementAccessListParameter* parameter, std::int64_t* requestId);
int APS5_VABI sceNpEntitlementAccessGetAddcontEntitlementInfoIndividual(std::int32_t userId, uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel, NpEntitlementAccessAddcontEntitlementInfo* info);
int APS5_VABI sceNpEntitlementAccessPollConsumableEntitlementInfo();
int APS5_VABI sceNpEntitlementAccessRequestConsumableEntitlementInfo();
int APS5_VABI sceNpEntitlementAccessRequestConsumeEntitlement();

}
