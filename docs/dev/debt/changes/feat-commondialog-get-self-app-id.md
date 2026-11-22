# libSceCommonDialog

## add Functional

- [`sce::CommonDialogUtil::getSelfAppId`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp) (libSceCommonDialog) - returns the running big-app id through `sceLncUtilGetAppIdOfRunningBigApp`. Firmware 13.42 instead queries the still-unimplemented `sceLncUtilGetAppStatus` and returns -1 when that query fails, so the failure path and results outside the running title are unverified. Closes when: `sceLncUtilGetAppStatus` is implemented and the failure path is compared with the console.
