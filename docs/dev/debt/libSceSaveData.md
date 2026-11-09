# libSceSaveData technical debt

### Functional

- [libSceSaveData](../../../core/libs/prx/libSceSaveData/CMakeLists.txt) is built from the libSceSaveData.native sources, so a title that imports save data from `libSceSaveData`, as PS4 titles do, gets the same functions. The separate implementation it had before kept save-data memory in `_sd/sce_sdmemory/<user>/memory.dat` and mount aliases in `_sd/_aliases`; neither is read, so memory saved through that library is not found

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceSaveData/InventoryExports.cpp) - 14 entries present in both version-labelled images are import-resolution declarations only. None is listed in the pinned public 3.20 catalogue for this library. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `0TAVrO-LgPI` |
| `Unknown` | `35cI-Z0nz68` |
| `Unknown` | `AuTE0gFxZCI` |
| `Unknown` | `IE+HJN8+Jdg` |
| `Unknown` | `IGpkv-UOup0` |
| `Unknown` | `NxxouhCA12c` |
| `Unknown` | `QaI8L6NzmJU` |
| `Unknown` | `VM8qEhWh-A4` |
| `Unknown` | `apvJPjG+6-Q` |
| `Unknown` | `cbD93i6sy9E` |
| `Unknown` | `fIu-ckf3n-o` |
| `Unknown` | `g1roz3Ux+3I` |
| `Unknown` | `q8KxzjSpPVw` |
| `Unknown` | `yBM2vzabhEA` |

### Unknown function info

- [libSceSaveData](../../../core/libs/prx/libSceSaveData/CMakeLists.txt) keeps PSN and cloud exports pending in libSceSaveData.native.md: `sceSaveDataBindPsnAccount`, `sceSaveDataBindPsnAccountForSystemBackup`, `sceSaveDataCheckCloudData`, `sceSaveDataCheckPremiumMember`, `sceSaveDataCheckSyncStatus`, `sceSaveDataCreateUploadData`, `sceSaveDataDeleteCloudData`, `sceSaveDataDownload`, `sceSaveDataGetAllSizeProspero`, `sceSaveDataGetAutoSyncInfo`, `sceSaveDataGetAutoUploadConditions`, `sceSaveDataGetAutoUploadRequestInfo`, `sceSaveDataGetAutoUploadSetting`, `sceSaveDataGetCloudInfo`, `sceSaveDataGetCloudQuotaInfo`, `sceSaveDataManualDownload`, `sceSaveDataManualUpload`, `sceSaveDataNotifySaveDataDownload`, `sceSaveDataSetAutoSyncSettings`, `sceSaveDataSetAutoUploadSetting`, `sceSaveDataSyncCloudList`, `sceSaveDataUpload`. shadPS4 0614ce3 has return-success templates for the PS4 names only; fpPS4 04cefd4, KytyPS5 569ec04 none. PS5 signatures and no-PSN host behaviour unknown.
- [libSceSaveData](../../../core/libs/prx/libSceSaveData/CMakeLists.txt) keeps the system and database exports listed as pending in libSceSaveData.native.md: `sceSaveDataChangeDatabase`, `sceSaveDataChangeInternal`, `sceSaveDataDeleteAllUser`, `sceSaveDataDeleteBackupData`, `sceSaveDataDeleteProspero`, `sceSaveDataDeleteTitle`, `sceSaveDataDeleteUser`, `sceSaveDataDirNameSearchInternal`, `sceSaveDataIsDeletingUsbDb`, `sceSaveDataManualDelete`, `sceSaveDataMount5`, `sceSaveDataMountInternal`, `sceSaveDataMountSys`, `sceSaveDataRebuildDatabase`, `sceSaveDataRestoreLoadSaveDataMemory`, `sceSaveDataShutdownStart`, `sceSaveDataSupportedFakeBrokenStatus`, `sceSaveDataUmountSys`. shadPS4 0614ce3 has return-success templates only for the PS4 names and none for the rest, and fpPS4 04cefd4 and KytyPS5 569ec04 carry none; RPS5E d540c46f wires `sceSaveDataMount5` to its mount3 handler, unmeasured. PS5 signatures and behaviour are unknown.
- [libSceSaveData](../../../core/libs/prx/libSceSaveData/CMakeLists.txt) keeps the query exports listed as pending in libSceSaveData.native.md: `sceSaveDataGetAllSize`, `sceSaveDataGetAppLaunchedUser`, `sceSaveDataGetBoundPsnAccountCount`, `sceSaveDataGetEventInfo`, `sceSaveDataGetSaveDataCount`, `sceSaveDataGetSaveDataRootDir`, `sceSaveDataGetSaveDataRootPath`, `sceSaveDataGetSaveDataRootUsbPath`, `sceSaveDataGetUpdatedDataCount`, `sceSaveDataInitializeForCdlg`, `sceSaveDataIsMounted`, `sceSaveDataSetEventInfo`, `sceSaveDataSetSaveDataLibraryUser`. shadPS4 0614ce3 has only return-success templates, and fpPS4 04cefd4 and KytyPS5 569ec04 none. RPS5E d540c46 answers `sceSaveDataGetEventInfo` and `sceSaveDataSetEventInfo` with plain success and writes through a second argument for `sceSaveDataGetSaveDataCount`, `sceSaveDataGetAllSize` and `sceSaveDataIsMounted`; the other eight have no RPS5E registration — shapes #14150 rejected as invented success; none is measured on a console.
- [libSceSaveData](../../../core/libs/prx/libSceSaveData/CMakeLists.txt) keeps the backup-check and dialog-variant exports listed as pending in libSceSaveData.native.md: `sceSaveDataCheckBackupDataForCdlg`, `sceSaveDataCheckBackupDataInternal`, `sceSaveDataCheckBackupDataLight`, `sceSaveDataCheckSaveDataBroken`, `sceSaveDataCheckSaveDataVersion`, `sceSaveDataCheckSaveDataVersionLatest`, `sceSaveDataRestoreBackupDataForCdlg`. shadPS4 0614ce3 has return-success templates only for the PS4 names and none for the rest, and fpPS4 04cefd4 and KytyPS5 569ec04 carry none; #12748 implements the PS4-shaped `sceSaveDataCheckBackupData` and `sceSaveDataRestoreBackupData` from shadPS4, which does not bind the ForCdlg, Internal or Light NIDs, so whether the PS5 variants share that behaviour, and their signatures, are unknown.
