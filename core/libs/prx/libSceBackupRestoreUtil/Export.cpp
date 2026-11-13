#include "prx/libSceBackupRestoreUtil/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_BACKUP_RESTORE_UTIL_ERROR_NOT_INITIALIZED = static_cast<int>(0x81300000);

}

extern "C" {

int APS5_VABI sceBackupRestoreUtilBackupData() {
    return SCE_BACKUP_RESTORE_UTIL_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceBackupRestoreUtilCancelBackupData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBackupRestoreUtilCancelGetRequiredSize() {
    return SCE_BACKUP_RESTORE_UTIL_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceBackupRestoreUtilCancelGetTitles() {
    return SCE_BACKUP_RESTORE_UTIL_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceBackupRestoreUtilCancelRestoreData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBackupRestoreUtilDeleteBackupData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBackupRestoreUtilGetBackupDataInfo() {
    return SCE_BACKUP_RESTORE_UTIL_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceBackupRestoreUtilGetBackupParam() {
    return SCE_BACKUP_RESTORE_UTIL_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceBackupRestoreUtilGetBackupProgress() {
    return SCE_BACKUP_RESTORE_UTIL_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceBackupRestoreUtilGetRequiredSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBackupRestoreUtilGetRestoreProgress() {
    return SCE_BACKUP_RESTORE_UTIL_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceBackupRestoreUtilGetTitles() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBackupRestoreUtilInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBackupRestoreUtilPrepareBackup() {
    return SCE_BACKUP_RESTORE_UTIL_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceBackupRestoreUtilRestoreData() {
    return SCE_BACKUP_RESTORE_UTIL_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceBackupRestoreUtilTerminate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBackupRestoreUtilCalcRequiredSize() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceBackupRestoreUtilCancelCalcRequiredSize() {
    return SCE_BACKUP_RESTORE_UTIL_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceBackupRestoreUtilGetBackupTelemetryInfo() {
    return SCE_BACKUP_RESTORE_UTIL_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceBackupRestoreUtilGetRestoreFailedTitles() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceBackupRestoreUtilGetRestoreTelemetryInfo() {
    return SCE_BACKUP_RESTORE_UTIL_ERROR_NOT_INITIALIZED;
}

}
