#pragma once

#include "SceTypes.hpp"

struct SaveDataMountPs4 {
    std::int32_t user_id;
    std::int32_t pad;
    const void* title_id;
    const SceSaveDataDirName* dir_name;
    const void* fingerprint;
    std::uint64_t blocks;
    std::uint32_t mount_mode;
    std::uint8_t reserved[32];
};

extern "C" {

int APS5_VABI sceSaveDataCommit(const SaveDataCommitParam* param);
int APS5_VABI sceSaveDataCreateTransactionResource(uint32_t size);
int APS5_VABI sceSaveDataDelete(const SaveDataDelete* del);
int APS5_VABI sceSaveDataDeleteTransactionResource(int32_t resource);
int APS5_VABI sceSaveDataDirNameSearch(const SaveDataDirNameSearchCond* cond, SaveDataDirNameSearchResult* result);
int APS5_VABI sceSaveDataGetMountInfo(const SaveDataMountPoint* mountPoint, SaveDataMountInfo* info);
int APS5_VABI sceSaveDataGetParam(const SaveDataMountPoint* mountPoint, uint32_t paramType, void* paramBuf, size_t paramBufSize, size_t* gotSize);
int APS5_VABI sceSaveDataLoadIcon(const SaveDataMountPoint* mountPoint, SaveDataIcon* icon);
int APS5_VABI sceSaveDataMount3(const SaveDataMount3* mount, SaveDataMountResult* mountResult);
int APS5_VABI sceSaveDataPrepare(const SaveDataMountPoint* mountPoint, const SaveDataPrepareParam* param);
int APS5_VABI sceSaveDataSaveIcon(const SaveDataMountPoint* mountPoint, const SaveDataIcon* icon);
int APS5_VABI sceSaveDataSaveIconByPath(const SaveDataMountPoint* mountPoint, const char* path);
int APS5_VABI sceSaveDataSetParam(const SaveDataMountPoint* mountPoint, uint32_t paramType, const void* paramBuf, size_t paramBufSize);
int APS5_VABI sceSaveDataUmount2(uint32_t mode, const SaveDataMountPoint* mountPoint);
int APS5_VABI sceSaveDataTransferringMountPs4(const SaveDataTransferringMount* mount, SaveDataMountResult* mountResult);
int APS5_VABI sceSaveDataDirNameSearchPs4(const SaveDataDirNameSearchCond* cond, SaveDataDirNameSearchResult* result);
int APS5_VABI sceSaveDataConvert(const SaveDataConvert* param);
int APS5_VABI sceSaveDataGetConvertProgress(float* progress);
int APS5_VABI sceSaveDataCancel(std::uint32_t cancelArgument);
int APS5_VABI sceSaveDataDirNameSearch2(const SaveDataDirNameSearchCond* cond, SaveDataDirNameSearchResult* result);
int APS5_VABI sceSaveDataPrepareForTransferring(const SaveDataMountPoint* mountPoint, const SaveDataPrepareParam* param);
int APS5_VABI sceSaveDataCheckIpmiIfSize();
int APS5_VABI sceSaveDataDebug();
int APS5_VABI sceSaveDataDebugCheckBackupData();
int APS5_VABI sceSaveDataDebugCleanMount();
int APS5_VABI sceSaveDataDebugCompiledSdkVersion();
int APS5_VABI sceSaveDataDebugCreateSaveDataRoot();
int APS5_VABI sceSaveDataDebugProspero();
int APS5_VABI sceSaveDataDebugRemoveSaveDataRoot();
int APS5_VABI sceSaveDataDebugTarget();
int APS5_VABI sceSaveDataInitialize(const void* init);
int APS5_VABI sceSaveDataMount(const SaveDataMountPs4* mount, SaveDataMountResult* mountResult);
int APS5_VABI sceSaveDataUmount(const SaveDataMountPoint* mountPoint);
int APS5_VABI sceSaveDataSetupSaveDataMemory(std::int32_t userId, std::size_t memorySize, const SaveDataParam* param);
int APS5_VABI sceSaveDataGetSaveDataMemory(std::int32_t userId, void* buf, std::size_t bufSize, std::int64_t offset);
int APS5_VABI sceSaveDataSetSaveDataMemory(std::int32_t userId, void* buf, std::size_t bufSize, std::int64_t offset);

}
