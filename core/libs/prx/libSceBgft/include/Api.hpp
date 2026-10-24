#pragma once

#include "SceTypes.hpp"

struct BgftInitParam {
    void* heap;
    uint64_t heapSize;
};

struct BgftProgress;

struct BgftStorageTaskParam {
    std::uint32_t value0;
    std::uint32_t value4;
    const char* contentId;
    const char* text16;
    std::byte unused24[8];
    const char* text32;
    const char* text40;
    const char* text48;
    std::uint32_t value56;
    std::byte padding60[4];
    const char* text64;
    const char* text72;
    const char* text80;
    const char* text88;
    std::uint64_t value96;
    std::uint32_t value104;
};

extern "C" {

int APS5_VABI sceBgftServiceInit(const BgftInitParam* params);
int APS5_VABI sceBgftServiceTerm(void);
int APS5_VABI sceBgftServiceDownloadPauseTaskAll();
int APS5_VABI sceBgftServiceDownloadResumeTaskAll();
int APS5_VABI sceBgftServiceDownloadStartTaskAll();
int APS5_VABI sceBgftServiceDownloadStopTaskAll();
int APS5_VABI sceBgftServiceDownloadFindActivePatchTask(const char* contentId, int32_t* taskId);
int APS5_VABI sceBgftServiceDownloadFindActivePupTask(int32_t* taskId);
int APS5_VABI sceBgftServiceDownloadFindTaskByContentId(const char* contentId, int32_t subType, int32_t* taskId);
int APS5_VABI sceBgftServiceDownloadGetProgress(int32_t taskId, BgftProgress* progress);
int APS5_VABI sceBgftServiceDownloadPauseTask(int32_t taskId);
int APS5_VABI sceBgftServiceDownloadResumeTask(int32_t taskId);
int APS5_VABI sceBgftServiceDownloadStartTask(int32_t taskId);
int APS5_VABI sceBgftServiceDownloadStopTask(int32_t taskId);
int APS5_VABI sceBgftServiceIntDownloadFindActiveGameAndGameAcTask(const char* contentId, int32_t* taskId);
int APS5_VABI sceBgftServiceIntDownloadFindActiveTask(const char* contentId, int32_t subType, int32_t* taskId);
int APS5_VABI sceBgftServiceIntDownloadGetGameAndGameAcProgress(const char* contentId, BgftProgress* progress);
int APS5_VABI sceBgftServiceIntDownloadGetPatchProgress();
int APS5_VABI sceBgftServiceIntDownloadGetPlayGoProgress(const char* contentId, BgftProgress* progress);
int APS5_VABI sceBgftServiceIntDownloadGetProgress(int32_t taskId, BgftProgress* progress);
int APS5_VABI sceBgftServiceIntDownloadGetTaskInfo();
int APS5_VABI sceBgftServiceIntDownloadPauseTask(int32_t taskId);
int APS5_VABI sceBgftServiceIntDownloadRegisterTaskByStorageEx(const BgftStorageTaskParam* params, int32_t* taskId);
int APS5_VABI sceBgftServiceIntDownloadResumeTask(int32_t taskId);
int APS5_VABI sceBgftServiceIntDownloadStopTask(int32_t taskId);
int APS5_VABI sceBgftServiceIntDownloadStartTask(int32_t taskId);
int APS5_VABI sceBgftServiceIntDownloadUnregisterTask(int32_t taskId);
int APS5_VABI sceBgftServiceIntInit(const BgftInitParam* params);
int APS5_VABI sceBgftServiceDownloadQueryTask(void);
int APS5_VABI sceBgftServiceIntDebugDownloadCorruptPkg(void);
int APS5_VABI sceBgftServiceIntDebugDownloadRequest(void);
int APS5_VABI sceBgftServiceIntDebugPlayGoGetPlayGoStatusString(void);
int APS5_VABI sceBgftServiceIntDebugPlayGoIsPaused(void);
int APS5_VABI sceBgftServiceIntDebugPlayGoResume(void);
int APS5_VABI sceBgftServiceIntDebugPlayGoRevertToFullState(void);
int APS5_VABI sceBgftServiceIntDebugPlayGoRevertToInitialState(void);
int APS5_VABI sceBgftServiceIntDebugPlayGoRevertToSnapshot(void);
int APS5_VABI sceBgftServiceIntDebugPlayGoSnapshotByTitleId(void);
int APS5_VABI sceBgftServiceIntDebugPlayGoSuspend(void);
int APS5_VABI sceBgftServiceIntDownloadDebugDeleteBgftEnvFile(void);
int APS5_VABI sceBgftServiceIntDownloadDebugDownloadBgftEnvFile(void);
int APS5_VABI sceBgftServiceIntDownloadDebugGetBgftEnvInfoString(void);
int APS5_VABI sceBgftServiceIntDownloadDebugGetStat(void);
int APS5_VABI sceBgftServiceIntDownloadGetExtUsbUserStorageSize(void);
int APS5_VABI sceBgftServiceIntDownloadGetUserStorageSize(void);
int APS5_VABI sceBgftServiceIntDownloadPauseTaskAll(void);
int APS5_VABI sceBgftServiceIntDownloadQueryTask(void);
int APS5_VABI sceBgftServiceIntDownloadRegisterTaskByStorage(void);
int APS5_VABI sceBgftServiceIntDownloadRestartTask(void);
int APS5_VABI sceBgftServiceIntDownloadResumeTaskAll(void);
int APS5_VABI sceBgftServiceIntDownloadStartTaskAll(void);
int APS5_VABI sceBgftServiceIntDownloadStopTaskAll(void);
int APS5_VABI sceBgftServiceIntDownloadUnregisterTaskAll(void);
int APS5_VABI sceBgftServiceIntExtUsbCheckActiveDownloadTasks(void);
int APS5_VABI sceBgftServiceIntGetTaskInfoValueInt(void);
int APS5_VABI sceBgftServiceIntGetTaskInfoValueString(void);
int APS5_VABI sceBgftServiceIntGetTaskInfoValueStringIndex(void);
int APS5_VABI sceBgftServiceIntPlayGoFindTaskId(void);
int APS5_VABI sceBgftServiceIntPlayGoGetDiscIniChunksInstallProgress(void);
int APS5_VABI sceBgftServiceIntPlayGoIsPaused(void);
int APS5_VABI sceBgftServiceIntPlayGoResume(void);
int APS5_VABI sceBgftServiceIntTerm(void);
int APS5_VABI sceBgftServiceIntUploadGetProgress(void);
int APS5_VABI sceBgftServiceIntUploadQueryTask(void);
int APS5_VABI sceBgftServiceIntUploadRegisterTask(void);
int APS5_VABI sceBgftServiceIntUploadRestartTask(void);
int APS5_VABI sceBgftServiceIntUploadStartTask(void);
int APS5_VABI sceBgftServiceIntUploadStartTaskAll(void);
int APS5_VABI sceBgftServiceIntUploadStopTask(void);
int APS5_VABI sceBgftServiceIntUploadStopTaskAll(void);
int APS5_VABI sceBgftServiceIntUploadUnregisterTask(void);
int APS5_VABI sceBgftServiceIntUploadUnregisterTaskAll(void);

int APS5_VABI sceBgftServiceIntDebugPlayGoClearSetFreeZone();
int APS5_VABI sceBgftServiceIntDebugPlayGoIsSetFreeZone();
int APS5_VABI sceBgftServiceIntDownloadCheckPatchUpdateState();
int APS5_VABI sceBgftServiceIntDownloadClearStats();
int APS5_VABI sceBgftServiceIntDownloadGetApplicationDataSize();
int APS5_VABI sceBgftServiceIntDownloadGetPatchGoProgress();
int APS5_VABI sceBgftServiceIntDownloadHideTask();
int APS5_VABI sceBgftServiceIntDownloadSetResumeState();
int APS5_VABI sceBgftServiceIntDownloadSetStartState();
int APS5_VABI sceBgftServiceIntPlayGoCheckDiscIniChunksInstall();
int APS5_VABI sceBgftServiceIntPlayGoStartDataDiscInstall();
int APS5_VABI sceBgftServiceIntSetTaskVisible();
int APS5_VABI sceBgftServiceIntUploadGetAvailableTaskCount();
int APS5_VABI sceBgftServiceIntUploadGetTaskInfo();
int APS5_VABI sceBgftServiceIntUploadHideTask();
int APS5_VABI sceBgftServiceIntUploadSetStartState();
int APS5_VABI sceBgftServiceIntUploadSetUploadError();

}
