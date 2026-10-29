#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSaveDataInitialize3(const void* init);
int APS5_VABI sceSaveDataTerminate(void);
int APS5_VABI sceSaveDataBackup(const SaveDataBackup* backup);
int APS5_VABI sceSaveDataGetEventResult(const void* eventParam, SaveDataEvent* event);
int APS5_VABI sceSaveDataGetSaveDataMemory2(SaveDataMemoryGet2* getParam);
int APS5_VABI sceSaveDataSetSaveDataMemory2(const SaveDataMemorySet2* setParam);
int APS5_VABI sceSaveDataSetupSaveDataMemory2(const SaveDataMemorySetup2* setupParam, SaveDataMemorySetupResult* result);
int APS5_VABI sceSaveDataSyncSaveDataMemory(const void* syncParam);
int APS5_VABI sceSaveDataTransferringMount(const SaveDataTransferringMount* mount, SaveDataMountResult* mountResult);
int APS5_VABI sceSaveDataAbort();
int APS5_VABI sceSaveDataBindPsnAccount(void);
int APS5_VABI sceSaveDataBindPsnAccountForSystemBackup(void);
int APS5_VABI sceSaveDataChangeDatabase(void);
int APS5_VABI sceSaveDataChangeInternal(void);
int APS5_VABI sceSaveDataCheckBackupData(void);
int APS5_VABI sceSaveDataCheckBackupDataForCdlg(void);
int APS5_VABI sceSaveDataCheckBackupDataInternal(void);
int APS5_VABI sceSaveDataCheckBackupDataLight(void);
int APS5_VABI sceSaveDataCheckCloudData(void);
int APS5_VABI sceSaveDataCheckSaveDataBroken(void);
int APS5_VABI sceSaveDataCheckSaveDataVersion(void);
int APS5_VABI sceSaveDataCheckSaveDataVersionLatest(void);
int APS5_VABI sceSaveDataClearProgress();
int APS5_VABI sceSaveDataCreateUploadData(void);
int APS5_VABI sceSaveDataDebugGetThreadId(void);
int APS5_VABI sceSaveDataDeleteAllUser(void);
int APS5_VABI sceSaveDataDeleteBackupData(void);
int APS5_VABI sceSaveDataDeleteCloudData(void);
int APS5_VABI sceSaveDataDeleteProspero(void);
int APS5_VABI sceSaveDataDeleteTitle(void);
int APS5_VABI sceSaveDataDeleteUser(void);
int APS5_VABI sceSaveDataDirNameSearchInternal(void);
int APS5_VABI sceSaveDataDownload(void);
int APS5_VABI sceSaveDataGetAllSize();
int APS5_VABI sceSaveDataGetAppLaunchedUser(void);
int APS5_VABI sceSaveDataGetAutoUploadConditions(void);
int APS5_VABI sceSaveDataGetAutoUploadRequestInfo(void);
int APS5_VABI sceSaveDataGetAutoUploadSetting(void);
int APS5_VABI sceSaveDataGetBoundPsnAccountCount(void);
int APS5_VABI sceSaveDataGetClientThreadPriority(void);
int APS5_VABI sceSaveDataGetCloudQuotaInfo(void);
int APS5_VABI sceSaveDataGetEventInfo();
int APS5_VABI sceSaveDataGetMountedSaveDataCount();
int APS5_VABI sceSaveDataGetProgress();
int APS5_VABI sceSaveDataGetSaveDataCount();
int APS5_VABI sceSaveDataGetSaveDataRootDir(void);
int APS5_VABI sceSaveDataGetSaveDataRootPath(void);
int APS5_VABI sceSaveDataGetSaveDataRootUsbPath(void);
int APS5_VABI sceSaveDataGetSavePoint(void);
int APS5_VABI sceSaveDataGetUpdatedDataCount(void);
int APS5_VABI sceSaveDataInitialize2();
int APS5_VABI sceSaveDataInitializeForCdlg(void);
int APS5_VABI sceSaveDataIsDeletingUsbDb(void);
int APS5_VABI sceSaveDataIsMounted();
int APS5_VABI sceSaveDataManualDelete(void);
int APS5_VABI sceSaveDataManualDownload(void);
int APS5_VABI sceSaveDataManualUpload(void);
int APS5_VABI sceSaveDataMount5();
int APS5_VABI sceSaveDataMountInternal(void);
int APS5_VABI sceSaveDataMountSys(void);
int APS5_VABI sceSaveDataRebuildDatabase(void);
int APS5_VABI sceSaveDataRegisterEventCallback();
int APS5_VABI sceSaveDataRestoreBackupData(void);
int APS5_VABI sceSaveDataRestoreBackupDataForCdlg(void);
int APS5_VABI sceSaveDataRestoreLoadSaveDataMemory(void);
int APS5_VABI sceSaveDataSetAutoUploadSetting(void);
int APS5_VABI sceSaveDataSetEventInfo();
int APS5_VABI sceSaveDataSetSaveDataLibraryUser(void);
int APS5_VABI sceSaveDataShutdownStart(void);
int APS5_VABI sceSaveDataSupportedFakeBrokenStatus(void);
int APS5_VABI sceSaveDataSyncCloudList(void);
int APS5_VABI sceSaveDataUmountSys(void);
int APS5_VABI sceSaveDataUmountWithBackup(void);
int APS5_VABI sceSaveDataUnregisterEventCallback();
int APS5_VABI sceSaveDataUpload(void);

}
