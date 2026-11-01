# libSceAppInstUtil technical debt

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) - 26 entries present in both version-labelled images are import-resolution declarations only. None is listed in the pinned public 3.20 catalogue for this library. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `2WQuFX8yYXg` |
| `Unknown` | `4vqRxwpIE1M` |
| `Unknown` | `9QIC1kZpSG4` |
| `Unknown` | `K73u2VX0JFo` |
| `Unknown` | `LsUZe38RhCs` |
| `Unknown` | `N6H7LMfZD5Q` |
| `Unknown` | `NXqdUFpinmo` |
| `Unknown` | `QMjuGVPtjUY` |
| `Unknown` | `R-OUApMZ1d8` |
| `Unknown` | `T9h6l8bFKGE` |
| `Unknown` | `X5Q7QxodFcg` |
| `Unknown` | `XWjOLtdgxa4` |
| `Unknown` | `Y+ln-TkY+hA` |
| `Unknown` | `Zv+26GDSKcQ` |
| `Unknown` | `ecad2GAw55Y` |
| `Unknown` | `gFlBHjtrMDI` |
| `Unknown` | `gP27iU+aZFw` |
| `Unknown` | `grC16befiso` |
| `Unknown` | `j80u95nZ+3A` |
| `Unknown` | `j9qjbjMifhQ` |
| `Unknown` | `pbYNtR2enrY` |
| `Unknown` | `qn0aziKw-kE` |
| `Unknown` | `qxjCWxg8ki4` |
| `Unknown` | `vZ1SWJ0y4mo` |
| `Unknown` | `xV7MhF+9AQk` |
| `Unknown` | `yYVV-f0zoWY` |

### Unknown function info

- [sceAppInstUtilAppInstallPkg](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) - the package path is followed by an opaque 64-bit value sent to the install service; its meaning and pointer-versus-integer type remain unresolved.
- [sceAppInstUtilGetInstallProgressInfo, sceAppInstUtilGetInstallProgress and sceAppInstUtilGetPrimaryAppSlot](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) - take a title-id string and an output pointer. The IPC replies contain 48, 4 and 4 bytes respectively; callers and reply producers are needed to establish the field types.
- [sceAppInstUtilAppPrepareOverwritePkg, sceAppInstUtilAppRecoverApp and sceAppInstUtilAppUnInstallTheme](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) - absent from the supplied system module and from the supplied decompiled callers; the old export inventory establishes names but not prototypes.
- [sceAppInstUtilCheckLatestPatch2, sceAppInstUtilAppGetIrremovableAppList2, sceAppInstUtilGetAddons2, sceAppInstUtilInstallPatch2, sceAppInstUtilAppUnInstall3, sceAppInstUtilGetPatchInstallStatus2, sceAppInstUtilAppRequestMoveApps2, sceAppInstUtilGetContentSize3](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) throw on every call. Their names and NIDs are confirmed defined function exports in the pinned [PS5 13.60 inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libsceappinstutil-783ade3c34.jsonl); signatures and behavior remain unknown. Parameterless `int` declarations are import placeholders, not verified prototypes or working implementations.
- [sceAppInstUtilAppGetAppInstallStatusByAppInfo](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppUnInstall2](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppUnInstallForRemote](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppUnInstallForRemote2](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppGetAddcontListTotalSize](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppGetAddcontInfo](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppGetAddcontInfoByContentId](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilGetAddcontInstalledStatus](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppGetInsertedDiscTitleId](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppUpdateStartTime](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppInstallSharePlayLink](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppInstallSharePlayLinkByUser](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppGetAddcontNum](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppCancelUnInstall](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppDestroyMetadata](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilCheckAppSystemVer](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppConvertAppDownloadToDisc](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilGetMainAppTitleIdForSP](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilGetSubAppTitleIdForSP](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilGetMainAppTitleId](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilGetSubAppTitleId](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppGetAppInstallStatus](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppDestroyPkg](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilJson](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilGetInsertedDiscRoleType](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilRequestInstallDiscGame](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilRequestDataDiscCopy](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilCancelDataDiscCopy](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilGetDataDiscCopyProgress](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilSetPrimaryAppSlot](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilGetAppSlotInfoList](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilGetAppEmptySlot](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppInstallGetLinkState](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppUnInstallForMultiInstall](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppInstallPs4CloudGame](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppStoreCompilationDiscInfo](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppGetCompilationDiscInfo](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppGetStoreCompilationDiscInfo](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppGetInsertedDiscTitleIdList](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppInstallRequestCompilationDiscApp](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppGetNoRelatedAddcontTitleList](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppGetIrremovableAppList](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppRequestMoveApps](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppProhibitDownloadInstall](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppAllowDownloadInstall](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilGetAppMoveProgressInfo](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppCancelMoveApps](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppGetMoveErrorAppList](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppGetAllAppSize](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppCancelGetAllAppSize](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppGetInsertedDiscMasterDataId](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppGetAppOtherSize](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppCancelGetAppOtherSize](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilGetInsertedDiscTotal](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppSetRemasterInstallType](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilGetInstallProgressInfoForMultiDisc](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilMountBdEmu](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilUnmountBdEmu](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilGetInstallStatus](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilResumeInstall](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilPauseInstall](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilRegisterDownload](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilInstall](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilStartInstall](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilInstallPatch](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilGetPatchInstallStatus](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilResumePatchInstall](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilPausePatchInstall](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilCancelInstallPatch](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilCancelInstallAddcont](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilCancelInstall](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilCancelForMultiInstall](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilInstallByPackage](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilIsRestoreState](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilGetRestoreState](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilGetAddons](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilGetInstallOptions](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilGetContentSize](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilCancelGetInformation](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilGetWorkspaceStatus](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilDeleteWorkspace](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilGetParentalControlLevel](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilCheckLatestPatch](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilCancelCheckPatch](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppInstallPsNowIcon](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilGetAddcontInfoList2](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilGetAddcontInfoList3](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilClearAddcontInfoCache](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilGetContentSize2](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppInstallContentPush](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilGetAvailableStorage](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilSwitchInstallLocation](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilGetConfigList](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilSwitchConfig](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilRestoreDataSource](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilGetNeededStorageSize](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppGetStorageDestType](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilInstallAddons](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature
- [sceAppInstUtilAppConvertAppDiscToDownload, sceAppInstUtilAppGetCustomThemeInfo, sceAppInstUtilAppGetInstallAddcontFromDiscStatus and sceAppInstUtilAppInstallCloudGame](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - The list is the intersection of the export table of the console's libSceAppInstUtil (function symbols only), the pinned aerolib names and the functions not yet defined anywhere in the repo. Names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAppInstUtil.c) and the [verified NID name database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv), but no credible prototypes were found. They are declared without parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified.
- [sceAppInstUtilAppInstallMediaPlayer, sceAppInstUtilAppInstallRequestAddcontFromDisc and sceAppInstUtilAppSetSavedataUploadStatus](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - The list is the intersection of the export table of the console's libSceAppInstUtil (function symbols only), the pinned aerolib names and the functions not yet defined anywhere in the repo. Names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAppInstUtil.c) and the [verified NID name database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv), but no credible prototypes were found. They are declared without parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified.
