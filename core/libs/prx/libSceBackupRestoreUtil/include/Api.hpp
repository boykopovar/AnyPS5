#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceBackupRestoreUtilBackupData();
int APS5_VABI sceBackupRestoreUtilCancelBackupData();
int APS5_VABI sceBackupRestoreUtilCancelGetRequiredSize();
int APS5_VABI sceBackupRestoreUtilCancelGetTitles();
int APS5_VABI sceBackupRestoreUtilCancelRestoreData();
int APS5_VABI sceBackupRestoreUtilDeleteBackupData();
int APS5_VABI sceBackupRestoreUtilGetBackupDataInfo();
int APS5_VABI sceBackupRestoreUtilGetBackupParam();
int APS5_VABI sceBackupRestoreUtilGetBackupProgress();
int APS5_VABI sceBackupRestoreUtilGetRequiredSize();
int APS5_VABI sceBackupRestoreUtilGetRestoreProgress();
int APS5_VABI sceBackupRestoreUtilGetTitles();
int APS5_VABI sceBackupRestoreUtilInitialize();
int APS5_VABI sceBackupRestoreUtilPrepareBackup();
int APS5_VABI sceBackupRestoreUtilRestoreData();
int APS5_VABI sceBackupRestoreUtilTerminate();
int APS5_VABI sceBackupRestoreUtilCalcRequiredSize();
int APS5_VABI sceBackupRestoreUtilCancelCalcRequiredSize();
int APS5_VABI sceBackupRestoreUtilGetBackupTelemetryInfo();
int APS5_VABI sceBackupRestoreUtilGetRestoreFailedTitles();
int APS5_VABI sceBackupRestoreUtilGetRestoreTelemetryInfo();

}
