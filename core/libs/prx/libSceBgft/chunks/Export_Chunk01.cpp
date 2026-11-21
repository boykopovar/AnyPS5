extern "C" {
#include "prx/libSceBgft/include/LegacyExports.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceBgft/include/InventoryExports.hpp"
#include "prx/libSceBgft/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <string>
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

static_assert(offsetof(BgftStorageTaskParam, text16) == 16);
static_assert(offsetof(BgftStorageTaskParam, text40) == 40);

static constexpr int SCE_BGFT_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80990001);
static constexpr int SCE_BGFT_ERROR_NOT_INITIALIZED = static_cast<int>(0x80990002);
static constexpr int SCE_BGFT_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80990004);
static constexpr int SCE_BGFT_ERROR_NOT_SUPPORTED = static_cast<int>(0x80990006);

static bool g_initialized = false;

static int Initialize(const BgftInitParam* params, const char* name) {
    if (g_initialized) return SCE_BGFT_ERROR_ALREADY_INITIALIZED;
    if (params == nullptr) return SCE_BGFT_ERROR_INVALID_ARGUMENT;
    BgftInitParam localParam{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&localParam, params, sizeof(localParam));
    errno = savedErrno;
    if (!copied) return SCE_BGFT_ERROR_INVALID_ARGUMENT;
    if (localParam.heapSize == 0) NotImplemented_nid_no_patch(name);
    g_initialized = true;
    return 0;
}

static int Terminate() {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    g_initialized = false;
    return 0;
}


int APS5_VABI sceBgftServiceInit(const BgftInitParam* params) {
    return Initialize(params, __func__);
}
int APS5_VABI sceBgftServiceTerm(void) {
    return Terminate();
}
int APS5_VABI sceBgftServiceDownloadPauseTaskAll() {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceDownloadResumeTaskAll() {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceDownloadStartTaskAll() {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceDownloadStopTaskAll() {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceDownloadFindActivePatchTask(const char* contentId, int32_t* taskId) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceDownloadFindActivePupTask(int32_t* taskId) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceDownloadFindTaskByContentId(const char* contentId, int32_t subType, int32_t* taskId) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceDownloadGetProgress(int32_t taskId, BgftProgress* progress) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceDownloadPauseTask(int32_t taskId) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceDownloadResumeTask(int32_t taskId) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceDownloadStartTask(int32_t taskId) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceDownloadStopTask(int32_t taskId) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadFindActiveGameAndGameAcTask(const char* contentId, int32_t* taskId) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadFindActiveTask(const char* contentId, int32_t subType, int32_t* taskId) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadGetGameAndGameAcProgress(const char* contentId, BgftProgress* progress) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadGetPatchProgress(const char* contentId, BgftProgress* progress) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadGetPlayGoProgress(const char* contentId, BgftProgress* progress) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadGetProgress(int32_t taskId, BgftProgress* progress) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadGetTaskInfo(int32_t taskId, BgftTaskInfo* taskInfo) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadPauseTask(int32_t taskId) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
static bool validGuestString(const char* text, std::size_t limit) {
    std::string value;
    const int savedErrno = errno;
    const int result = CopyGuestString_nid_no_patch(text, limit, value);
    errno = savedErrno;
    return result == 0;
}

int APS5_VABI sceBgftServiceIntDownloadRegisterTaskByStorageEx(const BgftStorageTaskParam* params, int32_t* taskId) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    if (params == nullptr || taskId == nullptr) return SCE_BGFT_ERROR_INVALID_ARGUMENT;
    BgftStorageTaskParam localParams{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&localParams, params, sizeof(localParams));
    errno = savedErrno;
    if (!copied) return SCE_BGFT_ERROR_INVALID_ARGUMENT;
    if (localParams.contentId == nullptr || !validGuestString(localParams.contentId, 0x31)) return SCE_BGFT_ERROR_INVALID_ARGUMENT;
    if (localParams.text16 != nullptr && !validGuestString(localParams.text16, 0x800)) return SCE_BGFT_ERROR_INVALID_ARGUMENT;
    if (localParams.text32 != nullptr && !validGuestString(localParams.text32, 0x259)) return SCE_BGFT_ERROR_INVALID_ARGUMENT;
    if (localParams.text40 != nullptr && !validGuestString(localParams.text40, 0x800)) return SCE_BGFT_ERROR_INVALID_ARGUMENT;
    return SCE_BGFT_ERROR_NOT_SUPPORTED;
}
int APS5_VABI sceBgftServiceIntDownloadResumeTask(int32_t taskId) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadStopTask(int32_t taskId) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadStartTask(int32_t taskId) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntDownloadUnregisterTask(int32_t taskId) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceBgftServiceIntInit(const BgftInitParam* params) {
    return Initialize(params, __func__);
}
int APS5_VABI sceBgftServiceDownloadQueryTask(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDebugDownloadCorruptPkg(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDebugDownloadRequest(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDebugPlayGoGetPlayGoStatusString(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDebugPlayGoIsPaused(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDebugPlayGoResume(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDebugPlayGoRevertToFullState(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDebugPlayGoRevertToInitialState(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDebugPlayGoRevertToSnapshot(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDebugPlayGoSnapshotByTitleId(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDebugPlayGoSuspend(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDownloadDebugDeleteBgftEnvFile(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDownloadDebugDownloadBgftEnvFile(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDownloadDebugGetBgftEnvInfoString(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDownloadDebugGetStat(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDownloadGetExtUsbUserStorageSize(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDownloadGetUserStorageSize(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDownloadPauseTaskAll(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDownloadQueryTask(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDownloadRegisterTaskByStorage(const BgftStorageTaskParam* params, int32_t* taskId) {
    if (params == nullptr) return SCE_BGFT_ERROR_INVALID_ARGUMENT;
    return sceBgftServiceIntDownloadRegisterTaskByStorageEx(params, taskId);
}

int APS5_VABI sceBgftServiceIntDownloadRestartTask(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDownloadResumeTaskAll(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDownloadStartTaskAll(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDownloadStopTaskAll(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntDownloadUnregisterTaskAll(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntExtUsbCheckActiveDownloadTasks(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntGetTaskInfoValueInt(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntGetTaskInfoValueString(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntGetTaskInfoValueStringIndex(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntPlayGoFindTaskId(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntPlayGoGetDiscIniChunksInstallProgress(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntPlayGoIsPaused(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntPlayGoResume(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntTerm(void) {
    return Terminate();
}

int APS5_VABI sceBgftServiceIntUploadGetProgress(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntUploadQueryTask(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntUploadRegisterTask(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntUploadRestartTask(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
