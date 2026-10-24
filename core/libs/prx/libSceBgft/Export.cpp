#include "prx/libSceBgft/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static_assert(sizeof(BgftStorageTaskParam) == 112);
static_assert(offsetof(BgftStorageTaskParam, contentId) == 8);
static_assert(offsetof(BgftStorageTaskParam, text32) == 32);
static_assert(offsetof(BgftStorageTaskParam, value56) == 56);
static_assert(offsetof(BgftStorageTaskParam, text64) == 64);
static_assert(offsetof(BgftStorageTaskParam, value96) == 96);
static_assert(offsetof(BgftStorageTaskParam, value104) == 104);

static_assert(sizeof(BgftInitParam) == 16);
static_assert(offsetof(BgftInitParam, heapSize) == 8);

extern "C" {

int APS5_VABI sceBgftServiceInit(const BgftInitParam* params) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceTerm(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceDownloadPauseTaskAll() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceDownloadResumeTaskAll() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceDownloadStartTaskAll() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceDownloadStopTaskAll() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceDownloadFindActivePatchTask(const char* contentId, int32_t* taskId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceDownloadFindActivePupTask(int32_t* taskId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceDownloadFindTaskByContentId(const char* contentId, int32_t subType, int32_t* taskId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceDownloadGetProgress(int32_t taskId, BgftProgress* progress) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceDownloadPauseTask(int32_t taskId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceDownloadResumeTask(int32_t taskId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceDownloadStartTask(int32_t taskId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceDownloadStopTask(int32_t taskId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadFindActiveGameAndGameAcTask(const char* contentId, int32_t* taskId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadFindActiveTask(const char* contentId, int32_t subType, int32_t* taskId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadGetGameAndGameAcProgress(const char* contentId, BgftProgress* progress) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadGetPatchProgress() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadGetPlayGoProgress(const char* contentId, BgftProgress* progress) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadGetProgress(int32_t taskId, BgftProgress* progress) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadGetTaskInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadPauseTask(int32_t taskId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadRegisterTaskByStorageEx(const BgftStorageTaskParam* params, int32_t* taskId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadResumeTask(int32_t taskId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadStopTask(int32_t taskId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadStartTask(int32_t taskId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadUnregisterTask(int32_t taskId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntInit(const BgftInitParam* params) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceDownloadQueryTask(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDebugDownloadCorruptPkg(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDebugDownloadRequest(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDebugPlayGoGetPlayGoStatusString(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDebugPlayGoIsPaused(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDebugPlayGoResume(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDebugPlayGoRevertToFullState(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDebugPlayGoRevertToInitialState(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDebugPlayGoRevertToSnapshot(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDebugPlayGoSnapshotByTitleId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDebugPlayGoSuspend(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDownloadDebugDeleteBgftEnvFile(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDownloadDebugDownloadBgftEnvFile(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDownloadDebugGetBgftEnvInfoString(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDownloadDebugGetStat(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDownloadGetExtUsbUserStorageSize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDownloadGetUserStorageSize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDownloadPauseTaskAll(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDownloadQueryTask(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDownloadRegisterTaskByStorage(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDownloadRestartTask(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDownloadResumeTaskAll(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDownloadStartTaskAll(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDownloadStopTaskAll(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDownloadUnregisterTaskAll(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntExtUsbCheckActiveDownloadTasks(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntGetTaskInfoValueInt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntGetTaskInfoValueString(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntGetTaskInfoValueStringIndex(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntPlayGoFindTaskId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntPlayGoGetDiscIniChunksInstallProgress(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntPlayGoIsPaused(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntPlayGoResume(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntTerm(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntUploadGetProgress(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntUploadQueryTask(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntUploadRegisterTask(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntUploadRestartTask(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntUploadStartTask(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntUploadStartTaskAll(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntUploadStopTask(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntUploadStopTaskAll(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntUploadUnregisterTask(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntUploadUnregisterTaskAll(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDebugPlayGoClearSetFreeZone() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDebugPlayGoIsSetFreeZone() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadCheckPatchUpdateState() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadClearStats() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadGetApplicationDataSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadGetPatchGoProgress() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadHideTask() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadSetResumeState() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadSetStartState() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntPlayGoCheckDiscIniChunksInstall() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntPlayGoStartDataDiscInstall() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntSetTaskVisible() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntUploadGetAvailableTaskCount() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntUploadGetTaskInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntUploadHideTask() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntUploadSetStartState() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntUploadSetUploadError() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
