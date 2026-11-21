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

int APS5_VABI sceBgftServiceIntUploadStartTask(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntUploadStartTaskAll(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntUploadStopTask(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntUploadStopTaskAll(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntUploadUnregisterTask(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBgftServiceIntUploadUnregisterTaskAll(void) {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
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

extern "C" {

int APS5_VABI sceBgftServiceIntDownloadClearAllInstalled() {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceBgftServiceIntDownloadClearInstalled() {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceBgftServiceIntDownloadPauseGroupTasks() {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceBgftServiceIntDownloadResumeGroupTasks() {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceBgftServiceIntDownloadStartGroupTasks() {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceBgftServiceIntDownloadUnregisterCompleteTask() {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceBgftServiceIntUploadUnregisterCompleteTask() {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceBgftUnknown_3cvL6QTobNg_nid_no_patch() {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("3cvL6QTobNg", libSceBgftUnknown_3cvL6QTobNg_nid_no_patch);

int APS5_VABI libSceBgftUnknown_4JQKNtsVDpg_nid_no_patch() {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("4JQKNtsVDpg", libSceBgftUnknown_4JQKNtsVDpg_nid_no_patch);

int APS5_VABI libSceBgftUnknown_85axuUkg9eE_nid_no_patch() {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("85axuUkg9eE", libSceBgftUnknown_85axuUkg9eE_nid_no_patch);

int APS5_VABI libSceBgftUnknown_fJnnIc5sHOU_nid_no_patch() {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("fJnnIc5sHOU", libSceBgftUnknown_fJnnIc5sHOU_nid_no_patch);

int APS5_VABI libSceBgftUnknown_tBOVSvi9EjY_nid_no_patch() {
    if (!g_initialized) return SCE_BGFT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("tBOVSvi9EjY", libSceBgftUnknown_tBOVSvi9EjY_nid_no_patch);

}

extern "C" {

int APS5_VABI bgftLegacyUnknown_fuesi13BbWE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("fuesi13BbWE", bgftLegacyUnknown_fuesi13BbWE_nid_no_patch);

int APS5_VABI bgftLegacyUnknown_tmiwomV8oXU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("tmiwomV8oXU", bgftLegacyUnknown_tmiwomV8oXU_nid_no_patch);

}
