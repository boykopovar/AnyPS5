#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI sceNpBandwidthTestAbort(int contextId);
int APS5_VABI sceNpBandwidthTestGetStatus(int contextId, int* status);
int APS5_VABI sceNpBandwidthTestInitStartDownload(const void* param);
int APS5_VABI sceNpBandwidthTestInitStartUpload(const void* param);
int APS5_VABI sceNpBandwidthTestShutdown(int contextId, void* result);
int APS5_VABI sceNpBandwidthTestInitStart();
int APS5_VABI sceNpLookupCreateRequest(int titleCtxId);
int APS5_VABI sceNpLookupCreateTitleCtx(const void* selfNpId);
int APS5_VABI sceNpLookupDeleteRequest(int reqId);
int APS5_VABI sceNpLookupDeleteTitleCtx(int titleCtxId);
int APS5_VABI sceNpLookupNpId(int reqId, const NpOnlineId* onlineId, void* npId, void* option);
int APS5_VABI sceNpWordFilterAbortRequest(int reqId);
int APS5_VABI sceNpWordFilterCensorComment(int reqId, const char* comment, void* option);
int APS5_VABI sceNpWordFilterCreateAsyncRequest(int titleCtxId, const std::uint64_t* param);
int APS5_VABI sceNpWordFilterCreateRequest(int titleCtxId);
int APS5_VABI sceNpWordFilterCreateTitleCtx(const void* selfNpId);
int APS5_VABI sceNpWordFilterDeleteRequest(int reqId);
int APS5_VABI sceNpWordFilterDeleteTitleCtx(int titleCtxId);
int APS5_VABI sceNpWordFilterPollAsync(int reqId, int* result);
int APS5_VABI sceNpLookupAbortRequest(int reqId);
int APS5_VABI sceNpLookupCreateAsyncRequest(int titleCtxId, const std::uint64_t* param);
int APS5_VABI sceNpLookupPollAsync(int reqId, int* result);
int APS5_VABI sceNpLookupSetTimeout(int id, int resolveRetry, std::uint32_t resolveTimeout, std::uint32_t connTimeout, std::uint32_t sendTimeout, std::uint32_t recvTimeout);
int APS5_VABI sceNpLookupWaitAsync(int reqId, int* result);
int APS5_VABI sceNpWordFilterSetTimeout(int id, int resolveRetry, std::uint32_t resolveTimeout, std::uint32_t connTimeout, std::uint32_t sendTimeout, std::uint32_t recvTimeout);
int APS5_VABI sceNpWordFilterWaitAsync(int reqId, int* result);
int APS5_VABI sceNpWordFilterSanitizeComment(int reqId, const char* comment, char* sanitizedComment, void* option);

int APS5_VABI sceNpAppInfoIntAbortRequest(void);
int APS5_VABI sceNpAppInfoIntCheckAvailability(std::uint32_t argument1, const void* argument2, std::uint32_t argument3, void* argument4, void* argument5);
int APS5_VABI sceNpAppInfoIntCheckAvailabilityA(std::uint32_t argument1, std::uint32_t argument2, std::uint32_t argument3, void* argument4, void* argument5);
int APS5_VABI sceNpAppInfoIntCheckAvailabilityAll(std::uint32_t argument1, const void* argument2, std::uint32_t argument3, void* argument4, std::uint64_t argument5, void* argument6);
int APS5_VABI sceNpAppInfoIntCheckAvailabilityAllA(std::uint32_t argument1, std::uint32_t argument2, std::uint32_t argument3, void* argument4, std::uint64_t argument5, void* argument6);
int APS5_VABI sceNpAppInfoIntCheckServiceAvailability(std::uint32_t argument1, const void* argument2, std::uint32_t argument3, std::uint32_t argument4, void* argument5, void* argument6);
int APS5_VABI sceNpAppInfoIntCheckServiceAvailabilityA(std::uint32_t argument1, std::uint32_t argument2, std::uint32_t argument3, std::uint32_t argument4, void* argument5, void* argument6);
int APS5_VABI sceNpAppInfoIntCheckServiceAvailabilityAll(std::uint32_t argument1, const void* argument2, std::uint32_t argument3, std::uint32_t argument4, void* argument5, std::uint64_t argument6, void* argument7);
int APS5_VABI sceNpAppInfoIntCheckServiceAvailabilityAllA(std::uint32_t argument1, std::uint32_t argument2, std::uint32_t argument3, std::uint32_t argument4, void* argument5, std::uint64_t argument6, void* argument7);
int APS5_VABI sceNpAppInfoIntCreateRequest(std::int32_t* requestId);
int APS5_VABI sceNpAppInfoIntDestroyRequest(void);
void APS5_VABI sceNpAppInfoIntFinalize(void);
int APS5_VABI sceNpAppInfoIntInitialize(std::int32_t userId);
int APS5_VABI sceNpAppLaunchLink2IntAbortRequest(std::int32_t requestId);
int APS5_VABI sceNpAppLaunchLink2IntCreateRequest(std::int32_t userId, const void* param, std::int32_t* requestId);
int APS5_VABI sceNpAppLaunchLink2IntDestroyRequest(std::int32_t requestId);
void APS5_VABI sceNpAppLaunchLink2IntFinalize(void);
int APS5_VABI sceNpAppLaunchLink2IntGetCompatibleTitleIdList(std::int32_t requestId, void* titleIdList, std::int32_t* titleIdNum);
int APS5_VABI sceNpAppLaunchLink2IntGetCompatibleTitleIdNum(std::int32_t requestId, std::int32_t* titleIdNum);
int APS5_VABI sceNpAppLaunchLink2IntInitialize(std::int32_t userId);
int APS5_VABI sceNpAppLaunchLinkIntAbortRequest(std::int32_t requestId);
int APS5_VABI sceNpAppLaunchLinkIntCreateRequest(std::int32_t userId, const void* param, std::int32_t* requestId);
int APS5_VABI sceNpAppLaunchLinkIntDestroyRequest(std::int32_t requestId);
void APS5_VABI sceNpAppLaunchLinkIntFinalize(void);
int APS5_VABI sceNpAppLaunchLinkIntGetCompatibleTitleIdList(std::int32_t requestId, void* titleIdList, std::int32_t* titleIdNum);
int APS5_VABI sceNpAppLaunchLinkIntGetCompatibleTitleIdNum(std::int32_t requestId, std::int32_t* titleIdNum);
int APS5_VABI sceNpAppLaunchLinkIntInitialize(std::int32_t userId);
int APS5_VABI sceNpBandwidthTestShutdownWithDetailedInfo();
int APS5_VABI sceNpLookupCreateTitleCtxA(int userId);
int APS5_VABI sceNpLookupNetAbortRequest(std::int32_t requestId);
int APS5_VABI sceNpLookupNetCensorComment(std::int32_t requestId, const void* npId, const char* comment, const void* option);
int APS5_VABI sceNpLookupNetConvertJidToNpId();
int APS5_VABI sceNpLookupNetConvertNpIdToJid();
int APS5_VABI sceNpLookupNetCreateRequest(std::int32_t titleCtxId);
int APS5_VABI sceNpLookupNetCreateTitleCtx(const void* selfNpId, const void* npId);
int APS5_VABI sceNpLookupNetDeleteRequest(std::int32_t requestId);
int APS5_VABI sceNpLookupNetDeleteTitleCtx(std::int32_t titleCtxId);
int APS5_VABI sceNpLookupNetInit(void);
int APS5_VABI sceNpLookupNetInitWithFunctionPointer(const void* param, const void* functionPointer);
int APS5_VABI sceNpLookupNetInitWithMemoryPool(const void* param, const void* pool);
int APS5_VABI sceNpLookupNetIsInit(std::int32_t* out);
int APS5_VABI sceNpLookupNetNpId(std::int32_t requestId, const void* onlineId, void* npId, const void* option);
int APS5_VABI sceNpLookupNetSanitizeComment(std::int32_t requestId, const void* npId, const char* comment, char* sanitizedComment, const void* option);
int APS5_VABI sceNpLookupNetSetTimeout(std::int32_t id, std::int32_t resolveRetry, std::int32_t resolveTimeout, std::int32_t connectTimeout, std::int32_t sendTimeout, std::int32_t receiveTimeout);
int APS5_VABI sceNpLookupNetTerm(void);
int APS5_VABI sceNpServiceChecker2IntAbortRequest(int requestId);
int APS5_VABI sceNpServiceChecker2IntCreateRequest(int* requestId);
int APS5_VABI sceNpServiceChecker2IntDestroyRequest(int requestId);
int APS5_VABI sceNpServiceChecker2IntFinalize();
int APS5_VABI sceNpServiceChecker2IntGetServiceAvailability(int requestId, const void* npId, void* result);
int APS5_VABI sceNpServiceChecker2IntGetServiceAvailabilityA(int requestId, int userId, void* result);
int APS5_VABI sceNpServiceChecker2IntInitialize();
int APS5_VABI sceNpServiceChecker2IntIsSetServiceType(const std::uint32_t* serviceTypes, std::uint32_t serviceType);
int APS5_VABI sceNpServiceCheckerIntAbortRequest(std::int32_t requestId);
int APS5_VABI sceNpServiceCheckerIntCreateRequest(const void* npId, std::int32_t* requestId);
int APS5_VABI sceNpServiceCheckerIntDestroyRequest(std::int32_t requestId);
void APS5_VABI sceNpServiceCheckerIntFinalize(void);
int APS5_VABI sceNpServiceCheckerIntGetAvailability(std::int32_t requestId, const void* npId, void* result);
int APS5_VABI sceNpServiceCheckerIntGetAvailabilityList(std::int32_t requestId, const void* const* npIds, std::size_t count, void* results);
int APS5_VABI sceNpServiceCheckerIntInitialize(std::int32_t userId);
int APS5_VABI sceNpServiceCheckerIntIsCached(std::int32_t requestId, std::uint8_t* cached);
int APS5_VABI sceNpTitleMetadataIntAbortRequest();
int APS5_VABI sceNpTitleMetadataIntCreateRequest();
int APS5_VABI sceNpTitleMetadataIntDeleteRequest();
int APS5_VABI sceNpTitleMetadataIntGetInfo(std::uint32_t argument1, void* argument2, void* argument3);
int APS5_VABI sceNpTitleMetadataIntGetNpTitleId(const char* titleId, std::size_t titleIdLength, char* npTitleId);
int APS5_VABI sceNpUtilityInit();
int APS5_VABI sceNpUtilityTerm();
int APS5_VABI sceNpWordFilterCreateTitleCtxA(int userId);

}
