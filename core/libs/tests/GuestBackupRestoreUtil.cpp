#include "prx/libSceBackupRestoreUtil/libSceBackupRestoreUtil.hpp"

#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

} // namespace

int main() {
    constexpr int notInitialized = static_cast<int>(0x81300000u);
    for (int call = 0; call < 2; ++call) {
        Require(sceBackupRestoreUtilGetBackupParam() == notInitialized);
        Require(sceBackupRestoreUtilBackupData() == notInitialized);
        Require(sceBackupRestoreUtilGetBackupProgress() == notInitialized);
        Require(sceBackupRestoreUtilRestoreData() == notInitialized);
        Require(sceBackupRestoreUtilGetRestoreProgress() == notInitialized);
        Require(sceBackupRestoreUtilGetBackupDataInfo() == notInitialized);
        Require(sceBackupRestoreUtilCancelGetRequiredSize() == notInitialized);
        Require(sceBackupRestoreUtilCancelGetTitles() == notInitialized);
        Require(sceBackupRestoreUtilPrepareBackup() == notInitialized);
        Require(sceBackupRestoreUtilGetBackupTelemetryInfo() == notInitialized);
        Require(sceBackupRestoreUtilGetRestoreTelemetryInfo() == notInitialized);
        Require(sceBackupRestoreUtilCancelCalcRequiredSize() == notInitialized);
        Require(sceBackupRestoreUtilCalcRequiredSize() == notInitialized);
        Require(sceBackupRestoreUtilCancelBackupData() == notInitialized);
        Require(sceBackupRestoreUtilCancelRestoreData() == notInitialized);
        Require(sceBackupRestoreUtilDeleteBackupData() == notInitialized);
        Require(sceBackupRestoreUtilGetRequiredSize() == notInitialized);
        Require(sceBackupRestoreUtilGetRestoreFailedTitles() == notInitialized);
        Require(sceBackupRestoreUtilGetTitles() == notInitialized);
    }
    return 0;
}
