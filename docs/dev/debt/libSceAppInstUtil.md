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
- [sceAppInstUtilAppPrepareOverwritePkg, sceAppInstUtilAppRecoverApp and sceAppInstUtilAppUnInstallTheme](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) - absent from the supplied system module and from the supplied decompiled callers; the old export inventory establishes names but not prototypes. The three parameter lists (`const char*` path or title id) are from the [OpenOrbis AppInstUtil.h](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/master/include/orbis/AppInstUtil.h) (PS4). The PS5 exports may take more; the bodies still throw through `NotImplemented_nid_no_patch` and nothing here has been measured on a console.
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
- [sceAppInstUtilAppIsInInstalling](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - throws, as its prototype is disputed: SharpProspero's [`AppInstUtil.cs`](https://github.com/SvenGDK/SharpProspero/blob/5dfdb4b0b166e83dc9ae275001afb695f12c7174/src/SharpProspero/Interop/AppInstUtil/AppInstUtil.cs#L79-L88) passes an `int*` that receives the answer, while its [`PackageInstaller.cs`](https://github.com/SvenGDK/SharpProspero/blob/5dfdb4b0b166e83dc9ae275001afb695f12c7174/src/SharpProspero/Platform/PackageInstaller.cs#L484-L498) passes a NUL-terminated id and reads the return value, as the PS4 headers declare `bool sceAppInstUtilAppIsInInstalling(const char* contentId)` ([flatz](https://github.com/flatz/ps4_stub_lib_maker_v2/blob/a8d044c12ea2dc3fa190ce1769eeb48b30d28ed4/include/appinst_util.h#L35)). Writing through the argument would overwrite a caller's id under the second form

### Functional

- [sceAppInstUtilInitialize, sceAppInstUtilTerminate, sceAppInstUtilAppExists](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - `sceAppInstUtilAppExists` writes 1 only for the running title's id, as another title cannot be launched (`sceSystemServiceLaunchApp` throws). The meanings follow [SharpProspero](https://github.com/SvenGDK/SharpProspero/blob/5dfdb4b0b166e83dc9ae275001afb695f12c7174/src/SharpProspero/Interop/AppInstUtil/AppInstUtil.cs#L18-L88). The results follow firmware 13.42 `libSceAppInstUtil.sprx` (read from disassembly, not measured on a console): initialization is counted and both calls always return 0, and `sceAppInstUtilAppExists` answers 0x80A30003 for a null pointer, then 0x80A31000 while the count is not positive. The console's service may still fail to connect after a successful initialization; that path isn't modelled. The non-zero value a console writes for an installed title is unmeasured
- [sceAppInstUtilAppExists](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - the console's answer for an unreadable title id or an unwritable `exists` pointer is unmeasured; AnyPS5 answers 0x80A30003 for both, after the initialization check. How far the console reads the title id is unknown. AnyPS5 reads up to the id's terminator, at most the running title's id length plus one byte. An unreadable byte before the terminator in that window answers 0x80A30003, even after a byte that already differs, where `strcmp` would stop
- [sceAppInstUtilAppIsInUpdating](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - writes false for every title id, as AnyPS5 never downloads or applies title updates. Its errors copy `sceAppInstUtilAppExists`: 0x80A30003 for a null pointer, then 0x80A31000 while the initialization count is not positive, then 0x80A30003 for a title id unreadable within the same window or an unwritable output. Its own firmware 13.42 routine has not been read, so the console's results for these cases are unverified
- [Exports answering only their service checks](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - the exports below throw after the two checks firmware 13.42's `libSceAppInstUtil.sprx` makes before it calls its install service: 0x80A30003 when an argument listed here is null, then 0x80A31000 while the reference count `sceAppInstUtilInitialize` raises is not above 0. What the service calls do, and the meaning of their arguments, is unknown. The checks were read from disassembly and run under a CPU emulator on x86-64, not measured on a console. Arguments count from 1; a declaration that had no parameters gets them only up to the last checked one, with neutral names, `const void*` for checked ones and the width the module moves for the others.

| Export | Null-checked arguments |
| --- | --- |
| `sceAppInstUtilAppInstallAll` | none |
| `sceAppInstUtilAppGetSize` | 1, 2 |
| `sceAppInstUtilAppInstallPkg` | 1 |
| `sceAppInstUtilAppUnInstallAddcont` | 1, 2 |
| `sceAppInstUtilAppUnInstallPat` | 1 |
| `sceAppInstUtilAppUnInstallTypes` | 1 |
| `sceAppInstUtilGetInstallProgressInfo` | 1, 2 |
| `sceAppInstUtilGetInstallProgress` | 1, 2 |
| `sceAppInstUtilGetPrimaryAppSlot` | 1, 2 |
| `sceAppInstUtilCheckLatestPatch2` | 2, 3 |
| `sceAppInstUtilInstallPatch2` | 1, 2, 3 |
| `sceAppInstUtilGetPatchInstallStatus2` | 1, 2, 3 |
| `sceAppInstUtilAppRequestMoveApps2` | 3 |
| `sceAppInstUtilAppGetAddcontListTotalSize` | 1 |
| `sceAppInstUtilAppGetAddcontInfo` | 1, 2, 3 |
| `sceAppInstUtilAppGetAddcontInfoByContentId` | 1, 2 |
| `sceAppInstUtilGetAddcontInstalledStatus` | 1, 2 |
| `sceAppInstUtilAppGetInsertedDiscTitleId` | 1 |
| `sceAppInstUtilAppUpdateStartTime` | 1, 2 |
| `sceAppInstUtilAppInstallSharePlayLink` | 1, 2 |
| `sceAppInstUtilAppInstallSharePlayLinkByUser` | 1, 2 |
| `sceAppInstUtilAppGetAddcontNum` | 1, 2 |
| `sceAppInstUtilAppDestroyMetadata` | 1 |
| `sceAppInstUtilCheckAppSystemVer` | 1, 2, 3 |
| `sceAppInstUtilAppConvertAppDownloadToDisc` | 1, 2 |
| `sceAppInstUtilGetMainAppTitleIdForSP` | 1, 2 |
| `sceAppInstUtilGetSubAppTitleIdForSP` | 1, 2 |
| `sceAppInstUtilGetMainAppTitleId` | 1, 2 |
| `sceAppInstUtilGetSubAppTitleId` | 1, 2 |
| `sceAppInstUtilAppGetAppInstallStatus` | 1, 2 |
| `sceAppInstUtilAppDestroyPkg` | 1 |
| `sceAppInstUtilJson` | 1, 2 |
| `sceAppInstUtilGetInsertedDiscRoleType` | 1 |
| `sceAppInstUtilRequestInstallDiscGame` | none |
| `sceAppInstUtilRequestDataDiscCopy` | 1 |
| `sceAppInstUtilCancelDataDiscCopy` | 1 |
| `sceAppInstUtilGetDataDiscCopyProgress` | 1, 2 |
| `sceAppInstUtilSetPrimaryAppSlot` | 1 |
| `sceAppInstUtilGetAppSlotInfoList` | 1, 2 |
| `sceAppInstUtilGetAppEmptySlot` | 1, 2 |
| `sceAppInstUtilAppInstallGetLinkState` | 1, 2 |
| `sceAppInstUtilAppUnInstallForMultiInstall` | 1 |
| `sceAppInstUtilAppInstallPs4CloudGame` | 1, 2 |
| `sceAppInstUtilAppGetCompilationDiscInfo` | 1 |
| `sceAppInstUtilAppInstallRequestCompilationDiscApp` | 1 |
| `sceAppInstUtilAppProhibitDownloadInstall` | none |
| `sceAppInstUtilAppAllowDownloadInstall` | none |
| `sceAppInstUtilGetAppMoveProgressInfo` | 1 |
| `sceAppInstUtilAppCancelMoveApps` | none |
| `sceAppInstUtilAppGetMoveErrorAppList` | 1, 3 |
| `sceAppInstUtilAppGetInsertedDiscMasterDataId` | 1 |
| `sceAppInstUtilGetInsertedDiscTotal` | 1 |
| `sceAppInstUtilAppSetRemasterInstallType` | 1 |
| `sceAppInstUtilGetInstallProgressInfoForMultiDisc` | 1, 2 |
| `sceAppInstUtilMountBdEmu` | 1, 2 |
| `sceAppInstUtilUnmountBdEmu` | none |
| `sceAppInstUtilGetInstallStatus` | 1, 2 |
| `sceAppInstUtilResumeInstall` | 1 |
| `sceAppInstUtilPauseInstall` | 1 |
| `sceAppInstUtilRegisterDownload` | 1, 2 |
| `sceAppInstUtilStartInstall` | 1 |
| `sceAppInstUtilInstallPatch` | 1, 2, 3 |
| `sceAppInstUtilGetPatchInstallStatus` | 1, 2 |
| `sceAppInstUtilResumePatchInstall` | 1 |
| `sceAppInstUtilPausePatchInstall` | 1 |
| `sceAppInstUtilCancelInstallPatch` | 1 |
| `sceAppInstUtilCancelInstallAddcont` | 1 |
| `sceAppInstUtilCancelInstall` | 1 |
| `sceAppInstUtilCancelForMultiInstall` | 1 |
| `sceAppInstUtilIsRestoreState` | 1, 2 |
| `sceAppInstUtilGetRestoreState` | 1, 2 |
| `sceAppInstUtilGetAddons` | 1, 5 |
| `sceAppInstUtilGetWorkspaceStatus` | 1, 2, 3 |
| `sceAppInstUtilDeleteWorkspace` | 1 |
| `sceAppInstUtilGetParentalControlLevel` | 1, 3 |
| `sceAppInstUtilCheckLatestPatch` | 2, 3 |
| `sceAppInstUtilCancelCheckPatch` | none |
| `sceAppInstUtilAppInstallPsNowIcon` | 1, 2 |
| `sceAppInstUtilAppInstallContentPush` | 1, 2 |
| `sceAppInstUtilSwitchInstallLocation` | none |
| `sceAppInstUtilGetConfigList` | 1, 2 |
| `sceAppInstUtilSwitchConfig` | 1, 2 |
| `sceAppInstUtilRestoreDataSource` | 1 |
| `sceAppInstUtilGetNeededStorageSize` | 1, 2 |
| `sceAppInstUtilAppGetStorageDestType` | 1, 2 |
| `libSceAppInstUtilUnknown_2WQuFX8yYXg_nid_no_patch` | 1, 2, 3 |
| `libSceAppInstUtilUnknown_4vqRxwpIE1M_nid_no_patch` | 1, 2 |
| `libSceAppInstUtilUnknown_9QIC1kZpSG4_nid_no_patch` | none |
| `libSceAppInstUtilUnknown_K73u2VX0JFo_nid_no_patch` | 1, 2, 4 |
| `libSceAppInstUtilUnknown_N6H7LMfZD5Q_nid_no_patch` | none |
| `libSceAppInstUtilUnknown_QMjuGVPtjUY_nid_no_patch` | 1 |
| `libSceAppInstUtilUnknown_R_minus_OUApMZ1d8_nid_no_patch` | 1, 2, 3 |
| `libSceAppInstUtilUnknown_T9h6l8bFKGE_nid_no_patch` | none |
| `libSceAppInstUtilUnknown_XWjOLtdgxa4_nid_no_patch` | none |
| `libSceAppInstUtilUnknown_Y_plus_ln_minus_TkY_plus_hA_nid_no_patch` | 1 |
| `libSceAppInstUtilUnknown_Zv_plus_26GDSKcQ_nid_no_patch` | 1, 3, 4 |
| `libSceAppInstUtilUnknown_ecad2GAw55Y_nid_no_patch` | none |
| `libSceAppInstUtilUnknown_gFlBHjtrMDI_nid_no_patch` | none |
| `libSceAppInstUtilUnknown_j80u95nZ_plus_3A_nid_no_patch` | none |
| `libSceAppInstUtilUnknown_qn0aziKw_minus_kE_nid_no_patch` | 1 |
| `libSceAppInstUtilUnknown_yYVV_minus_f0zoWY_nid_no_patch` | 1 |
