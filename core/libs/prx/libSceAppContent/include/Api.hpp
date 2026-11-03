#pragma once

#include "SceTypes.hpp"

struct AppContentAddcontInfo {
    NpUnifiedEntitlementLabel entitlement_label;
    uint32_t status;
};

struct AppContentEntitlementKey {
    char data[16];
};

extern "C" {

int APS5_VABI sceAppContentAddcontMount(uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel, AppContentMountPoint* mountPoint);
int APS5_VABI sceAppContentAddcontUnmount(const AppContentMountPoint* mountPoint);
int APS5_VABI sceAppContentGetAddcontInfo(uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel, AppContentAddcontInfo* info);
int APS5_VABI sceAppContentGetAddcontInfoList(uint32_t serviceLabel, AppContentAddcontInfo* list, uint32_t listNum, uint32_t* hitNum);
int APS5_VABI sceAppContentAppParamGetInt(uint32_t paramId, int32_t* value);
int APS5_VABI sceAppContentDownloadDataFormat(const AppContentMountPoint* mountPoint);
int APS5_VABI sceAppContentDownloadDataGetAvailableSpaceKb(const AppContentMountPoint* mountPoint, size_t* availableSpaceKb);
int APS5_VABI sceAppContentInitialize(const AppContentInitParam* initParam, AppContentBootParam* bootParam);
int APS5_VABI sceAppContentTemporaryDataFormat(const AppContentMountPoint* mountPoint);
int APS5_VABI sceAppContentTemporaryDataGetAvailableSpaceKb(const AppContentMountPoint* mountPoint, size_t* availableSpaceKb);
int APS5_VABI sceAppContentTemporaryDataMount2(uint32_t option, AppContentMountPoint* mountPoint);
int APS5_VABI sceAppContentTemporaryDataUnmount(const AppContentMountPoint* mountPoint);
int APS5_VABI sceAppContentAddcontEnqueueDownload(uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel);
int APS5_VABI sceAppContentAddcontDelete(uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel);
int APS5_VABI sceAppContentGetAddcontDownloadProgress(uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel, void* progress);
int APS5_VABI sceAppContentAddcontEnqueueDownloadByEntitlemetId(const char* entitlementId);
int APS5_VABI sceAppContentAddcontEnqueueDownloadSp(uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel);
int APS5_VABI sceAppContentAddcontMountByEntitlemetId(const char* entitlementId, AppContentMountPoint* mountPoint);
int APS5_VABI sceAppContentAddcontShrink(uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel);
int APS5_VABI sceAppContentCheckBundleLicenseOnDisc(uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel);
int APS5_VABI sceAppContentAppParamGetString(void);
int APS5_VABI sceAppContentDownload0Expand(void);
int APS5_VABI sceAppContentDownload0Shrink(void);
int APS5_VABI sceAppContentDownload1Expand(void);
int APS5_VABI sceAppContentDownload1Shrink(void);
int APS5_VABI sceAppContentGetAddcontInfoByEntitlementId(const char* entitlementId, AppContentAddcontInfo* info);
int APS5_VABI sceAppContentGetAddcontInfoListByIroTag(uint32_t iroTag, AppContentAddcontInfo* list, uint32_t listNum, uint32_t* hitNum);
int APS5_VABI sceAppContentGetDownloadedStoreCountry(void);
int APS5_VABI sceAppContentGetEntitlementKey(uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel, AppContentEntitlementKey* key);
int APS5_VABI sceAppContentGetGameTrialsFlag(uint32_t* gameTrialsFlag);
int APS5_VABI sceAppContentGetPftFlag(uint32_t* pftFlag);
int APS5_VABI sceAppContentGetPlayableStatus(uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel, uint32_t* playableStatus);
int APS5_VABI sceAppContentGetRegion(void);
int APS5_VABI sceAppContentRequestPatchInstall(void);
int APS5_VABI sceAppContentTemporaryDataMount(AppContentMountPoint* mountPoint);

}
