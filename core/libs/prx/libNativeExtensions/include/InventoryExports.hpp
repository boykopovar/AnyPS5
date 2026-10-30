#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI AsyncStorageClearNative();
int APS5_VABI AsyncStorageCloseNative();
int APS5_VABI AsyncStorageGetAllKeysNative();
int APS5_VABI AsyncStorageGetItemNative();
int APS5_VABI AsyncStorageOpenNative();
int APS5_VABI AsyncStorageRemoveItemNative();
int APS5_VABI AsyncStorageSetItemNative();
int APS5_VABI BgsStorageClose();
int APS5_VABI BgsStorageFreeKeyValueResult();
int APS5_VABI BgsStorageFreeListResult();
int APS5_VABI BgsStorageGetContainers();
int APS5_VABI BgsStorageGetItems();
int APS5_VABI BgsStorageGetItemsBySearch();
int APS5_VABI BgsStorageOpen();
int APS5_VABI CurlGetDefaultUA();
int APS5_VABI CurlMultiInitialize();
int APS5_VABI CurlMultiQueueRequest();
int APS5_VABI DecryptRnpsBundle();
int APS5_VABI HttpCacheWrapperFree();
int APS5_VABI HttpCacheWrapperRetrieve();
int APS5_VABI HttpCacheWrapperRevalidate();
int APS5_VABI NetCtlCheckCallbackNative();
int APS5_VABI NetCtlGetDeviceTypeNative();
int APS5_VABI NetCtlGetStateNative();
int APS5_VABI NetCtlInitNative();
int APS5_VABI NetCtlRegisterCallbackNative();
int APS5_VABI NetCtlTermNative();
int APS5_VABI NetCtlUnregisterCallbackNative();
int APS5_VABI RnpsGetOpenPsIdForSystem();
int APS5_VABI RnpsGetTicks();
int APS5_VABI ThreadUtils_SetSchedulePolicy();

}
