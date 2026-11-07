#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceFsInternalForVsh/include/InventoryExports.hpp"
#include "prx/libSceFsInternalForVsh/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceBdSchedCancelBackgroundCopyRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBdSchedCancelPrioritizedBackgroundCopyRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBdSchedConfigure() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBdSchedConfigure2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBdSchedDeconfigure() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBdSchedGetBackgroundCopyRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBdSchedGetExtentMap() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBdSchedGetPrioritizedBackgroundCopyRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBdSchedGetState() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBdSchedSetBackgroundCopyRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBdSchedSetDiscIdentity() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBdSchedSetPrioritizedBackgroundCopyRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBdSchedStartGame() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBdSchedStartRipping() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBdSchedStopScheduling() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsBfsCheckCleanFlag() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsBindApplication() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsBindPackages() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsCheckFormat() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsCheckSingleUser() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsCreateExfatPartition() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsCreatePfsSaveDataImage() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsCreatePfsTrophyDataImage() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsCreatePprPfsTrophyDataImage() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsDeviceAlignedPread() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsDeviceAlignedPwrite() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsDevpfsA53IocGetBuildMetadataStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsDevpfsCancelWaitEvent() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsDevpfsCreateForVSH() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsDevpfsDeleteForVSH() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsDevpfsDeregisterEvent() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsDevpfsEventComplete() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsDevpfsEventResponseComplete() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsDevpfsFetchMetaEntry() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsDevpfsFsck() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsDevpfsInitFetchMetaEntryCtx() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsDevpfsMountForFSMP() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsDevpfsRegisterEvent() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsDevpfsWaitEvent() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsDrfsRegisterSocket() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsDrfsResumeSocket() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsErriCtl() {
    return static_cast<int>(0x8002002d);
}

int APS5_VABI sceFsErriGetState() {
    return static_cast<int>(0x8002002d);
}

int APS5_VABI sceFsExfatMkfsExternalHdd() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsExfatfsMkfs() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsExtUSBSchedDestroy() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsExtUSBSchedInsert() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsExtUSBSchedInsertForHDD() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsExternalStorageDeconfigure() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsExternalStorageGetRawDevice() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsExternalStorageInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsExternalStorageVerifyMetadata() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsGetAttribute() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsGetClusterStatistics() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsGetDeviceSectorsize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsGetFsMetadata() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsGetFsMetadataWithProgress() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsGetSmrInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsGschedCtl() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsISSchedConfigCurrentThread() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsISSchedConfigDescriptor() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsISSchedConfigThread() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsISSchedMapStat() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsISSchedResultToPriority() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsISSchedResultToSlot() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsISSchedStatCurrentThread() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsISSchedStatDescriptor() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsISSchedStatThread() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsISSchedUnmapStat() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsInitCreatePfsSaveDataOpt() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsInitCreatePfsTrophyDataOpt() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsInitMountDownloadDataOpt() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsInitMountGamePkgOpt() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsInitMountLwfsCompatOpt() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsInitMountLwfsOpt() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsInitMountPprPkgOpt() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsInitMountSaveDataOpt() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsInitMountTrophyDataOpt() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsInitUmountDownloadDataOpt() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsInitUmountGamePkgOpt() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsInitUmountLwfsCompatOpt() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsInitUmountLwfsOpt() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsInitUmountPprPkgOpt() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsInitUmountSaveDataOpt() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsInitUmountTrophyDataOpt() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsInitWorkspaceFileMetadataParam() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsLvdAttach() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsLvdAttach2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsLvdAttachPartiallyCryptedDownloadData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsLvdAttachSingleDefaultImage() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsLvdAttachSingleImage() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsLvdDetach() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsLvdDisableLayer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsLvdEnableLayer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsLvdSetDiscIdentity() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsLvdWaitResumed() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsMlfsMount() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsMlfsMountWithFlags() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsMlfsUmount() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsMountDownloadData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsMountGamePkg() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsMountLwfs() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsMountLwfsCompat() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsMountPartiallyCryptedDownloadData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsMountPprPkg() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsMountSaveData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsMountTrophyData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsNsfsAddOverlayToNamespace() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsNsfsMountWithFlags() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsNsfsSetGlobalExclude() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsNsfsUmount() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsPartitionTrim() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsReserveCluster() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsSetAttribute() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsSetClusterCacheSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsSetFileReservedTotal() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsSetSystemReserveSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsSetWorkspaceFileHash() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsStatWorkspace() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsSysStorageGetStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsTransactionCancel() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsTransactionSaveDataInitMountOpt() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsTransactionSaveDataMount() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsTransactionStart() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsTrophyImageError() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsAllocateAdditionalContents() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsAllocateAligned() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsAllocateDownloadData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsAllocateGameImage() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsAllocatePatchImage() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsAllocateSaveData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsAllocateTrophyData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsCheckCleanFlag() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsCheckFixedCylinderGroupSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsCollectTelemetry() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsCollectTelemetry2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsDirectWrite() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsEstimateZoneCapacityForUser() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsExtendFile() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsFsck() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsFsckWithMM() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsGetExtentMap() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsGrowLwfs() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsGrowfs() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsHideBlocksFromUser() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsInitalizeSparseFile() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsLargewriteFsck() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsLargewriteFsckWithMM() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsMkLwfs() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsMkfs() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsMkfsForExternalHDD() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsMkfsForSystem() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsMkfsWithFixedCylinderGroupSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsReallocateSparseFile() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsRecryptBlocks() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsRecryptGetState() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsRecryptIsUnfinished() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsRecryptSyncMetadata() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsSeekHoleInFile() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUfsStopOperation() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUmountDownloadData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUmountGamePkg() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUmountLwfs() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUmountLwfsCompat() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUmountSaveData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUmountTrophyData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUmountWorkspace() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUnbindApplication() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsUnbindPackages() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsWsCreate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsWsDelete() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsWsFetchFileMetadata() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsWsFsck() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsWsGetStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsWsMount() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsWsUmount() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {

int APS5_VABI sceFsBfsAllocateAdditionalContents() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsBfsAllocateGameImage() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsBfsAllocatePatchImage() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsBfsAllocateSaveData() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsBfsAllocateTrophyData() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsBfsApplyPatch() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsBfsCancelPatch() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsBfsCommitPatch() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsBfsContainerBind() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsBfsContainerCreate() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsBfsContainerQuery() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsBfsContainerUnbind() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsBfsContainerUpdate() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsBfsExtendFile() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsBfsFormat() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsBfsFsck() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsBfsGcGetStatus() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsBfsGcMove() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsBfsGetAvailableSize() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsBfsGetBindStatus() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsBfsGetContainers() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsBfsGetFileSize() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsBfsGetMapping() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsBfsGetSCFStatus() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsBfsGetStatus() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsBfsM2Format() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsBfsMount() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsBfsTrim() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsBfsUnprotect() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsCreatePprPfsSaveDataImage() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsCreateWorkspace() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsDeleteWorkspace() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsDevpfsAllocateFile() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsDevpfsAllocateSparseFile() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsDevpfsMountForVSH() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsDevpfsSystemPowerEvent() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsDevpfsTermFetchMetaEntryCtx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsDevpfsUmountForFSMP() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsDevpfsUmountForVSH() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsDevpfsWriteChunk() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsDrfsDestroyContext() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsDrfsDisconnect() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsDrfsInitContext() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsDrfsMount() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsDrfsSuspendSocket() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsDrfsUmount() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsDrfsUnregisterSocket() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsDrfsWaitEvent() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsFetchWorkspaceFileMetadata() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsInitCreatePprPfsSaveDataOpt() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsInitCreateWorkspaceOpt() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsInitDeleteWorkspaceOpt() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsInitMountWorkspaceOpt() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsInitUmountWorkspaceOpt() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsIsActiveBankSystemEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsIsActiveBankSystem() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsM2StorageCheckCleanFlag() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsM2StorageDeconfigure() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsM2StorageFsckWithMM() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsM2StorageGetM2UniqueId() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsM2StorageInitialize() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsM2StorageMount() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsM2StorageUnmount() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsM2StorageVerifyMetadata() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsMountPprSaveData() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsMountWorkspace() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsNewfsDefault() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsNsfsMount() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsPartitionSwitchBank() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsPathMappingResolve() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsPlaygoDisable() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsPlaygoEnable() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsPlaygoInitializeParams() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsPlaygoSetParams() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsTransactionCommit() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsTransactionPprNoCowSaveDataMount() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsTransactionPprSaveDataMount() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsTransactionSaveDataReadParam() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsTransactionSaveDataUmount() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsTransactionSaveDataWriteParam() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsUfsCoerceCryptMode() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsUfsMkfsProsperoApplicationTemporaryData() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsUfsMkfsProsperoDownloadData() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsUmountPprPkg() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsUpdatePackageMetadataApp0() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsWorkspaceCoreDump() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsWsWriteFile() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFsWsWriteFileExtdata() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceFsInternalForVshUnknown__plus_gVdjPejLwc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+gVdjPejLwc", libSceFsInternalForVshUnknown__plus_gVdjPejLwc_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown__minus_HW6neCER_plus_A_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-HW6neCER+A", libSceFsInternalForVshUnknown__minus_HW6neCER_plus_A_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_0JhonchgIa8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0JhonchgIa8", libSceFsInternalForVshUnknown_0JhonchgIa8_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_0XlyVxRNeIw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0XlyVxRNeIw", libSceFsInternalForVshUnknown_0XlyVxRNeIw_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_1lLh9mukbz0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("1lLh9mukbz0", libSceFsInternalForVshUnknown_1lLh9mukbz0_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_2UgTmJt2dsc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("2UgTmJt2dsc", libSceFsInternalForVshUnknown_2UgTmJt2dsc_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_3TFUof4RLWM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("3TFUof4RLWM", libSceFsInternalForVshUnknown_3TFUof4RLWM_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_41PqBeun5cU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("41PqBeun5cU", libSceFsInternalForVshUnknown_41PqBeun5cU_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_4uqQOvVpISA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("4uqQOvVpISA", libSceFsInternalForVshUnknown_4uqQOvVpISA_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_68F1C7m2pO4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("68F1C7m2pO4", libSceFsInternalForVshUnknown_68F1C7m2pO4_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_6Bku_plus_Im1Pew_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("6Bku+Im1Pew", libSceFsInternalForVshUnknown_6Bku_plus_Im1Pew_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_89zs8_plus_EWQYQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("89zs8+EWQYQ", libSceFsInternalForVshUnknown_89zs8_plus_EWQYQ_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_9X3ydcDhbhw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("9X3ydcDhbhw", libSceFsInternalForVshUnknown_9X3ydcDhbhw_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_AnMWzsLhsYk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("AnMWzsLhsYk", libSceFsInternalForVshUnknown_AnMWzsLhsYk_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_CFlYeKz_plus_HYs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("CFlYeKz+HYs", libSceFsInternalForVshUnknown_CFlYeKz_plus_HYs_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_DEHwNbsX2BA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("DEHwNbsX2BA", libSceFsInternalForVshUnknown_DEHwNbsX2BA_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_DNjr3qYr_minus_lY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("DNjr3qYr-lY", libSceFsInternalForVshUnknown_DNjr3qYr_minus_lY_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_DSmBrz8enws_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("DSmBrz8enws", libSceFsInternalForVshUnknown_DSmBrz8enws_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_Elqj7jkJd5s_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Elqj7jkJd5s", libSceFsInternalForVshUnknown_Elqj7jkJd5s_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_GNeCtmqY_minus_58_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("GNeCtmqY-58", libSceFsInternalForVshUnknown_GNeCtmqY_minus_58_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_GsHxp1IXkfA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("GsHxp1IXkfA", libSceFsInternalForVshUnknown_GsHxp1IXkfA_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_KesAmkrIb9Y_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("KesAmkrIb9Y", libSceFsInternalForVshUnknown_KesAmkrIb9Y_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_KrZEB_plus_TsZ5k_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("KrZEB+TsZ5k", libSceFsInternalForVshUnknown_KrZEB_plus_TsZ5k_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_LGPQXeAAFdc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("LGPQXeAAFdc", libSceFsInternalForVshUnknown_LGPQXeAAFdc_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_LrpsS5hGlyY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("LrpsS5hGlyY", libSceFsInternalForVshUnknown_LrpsS5hGlyY_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_M9342PuS_plus_y0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("M9342PuS+y0", libSceFsInternalForVshUnknown_M9342PuS_plus_y0_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_MQs7RB_minus_KfYM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("MQs7RB-KfYM", libSceFsInternalForVshUnknown_MQs7RB_minus_KfYM_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_N7SnQKgSGA4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("N7SnQKgSGA4", libSceFsInternalForVshUnknown_N7SnQKgSGA4_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_NHH7572Mh30_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("NHH7572Mh30", libSceFsInternalForVshUnknown_NHH7572Mh30_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_NUfgeY29b9E_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("NUfgeY29b9E", libSceFsInternalForVshUnknown_NUfgeY29b9E_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_NdL2fJF8e78_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("NdL2fJF8e78", libSceFsInternalForVshUnknown_NdL2fJF8e78_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_NrSqjlYMetU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("NrSqjlYMetU", libSceFsInternalForVshUnknown_NrSqjlYMetU_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_O6akvoAE6k0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("O6akvoAE6k0", libSceFsInternalForVshUnknown_O6akvoAE6k0_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_P8F9U9uTPqA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("P8F9U9uTPqA", libSceFsInternalForVshUnknown_P8F9U9uTPqA_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_Rwk7z377rco_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Rwk7z377rco", libSceFsInternalForVshUnknown_Rwk7z377rco_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_SyNglOzKCV8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("SyNglOzKCV8", libSceFsInternalForVshUnknown_SyNglOzKCV8_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_T3LiDkpiS7s_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("T3LiDkpiS7s", libSceFsInternalForVshUnknown_T3LiDkpiS7s_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_UD35isX9168_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("UD35isX9168", libSceFsInternalForVshUnknown_UD35isX9168_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_W8CmA_plus_wZ9_plus_w_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("W8CmA+wZ9+w", libSceFsInternalForVshUnknown_W8CmA_plus_wZ9_plus_w_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_WwEWgYYuAmo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("WwEWgYYuAmo", libSceFsInternalForVshUnknown_WwEWgYYuAmo_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_XwPXO_plus_lxKy0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("XwPXO+lxKy0", libSceFsInternalForVshUnknown_XwPXO_plus_lxKy0_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_Y_minus_AXyBKGn8I_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Y-AXyBKGn8I", libSceFsInternalForVshUnknown_Y_minus_AXyBKGn8I_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_Z9o0nKENoCQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Z9o0nKENoCQ", libSceFsInternalForVshUnknown_Z9o0nKENoCQ_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_ZcrLUvsmmrU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ZcrLUvsmmrU", libSceFsInternalForVshUnknown_ZcrLUvsmmrU_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_a5ex0HGaufY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("a5ex0HGaufY", libSceFsInternalForVshUnknown_a5ex0HGaufY_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_b5kZJiwZPT8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("b5kZJiwZPT8", libSceFsInternalForVshUnknown_b5kZJiwZPT8_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_bGBEeJ2B6XE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("bGBEeJ2B6XE", libSceFsInternalForVshUnknown_bGBEeJ2B6XE_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_cDZDkhZeK4o_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("cDZDkhZeK4o", libSceFsInternalForVshUnknown_cDZDkhZeK4o_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_cUyUz68FKBc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("cUyUz68FKBc", libSceFsInternalForVshUnknown_cUyUz68FKBc_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_czWRa7s1ogE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("czWRa7s1ogE", libSceFsInternalForVshUnknown_czWRa7s1ogE_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_dKQBYtuMOWA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("dKQBYtuMOWA", libSceFsInternalForVshUnknown_dKQBYtuMOWA_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_eV9ctY_plus_OiOo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("eV9ctY+OiOo", libSceFsInternalForVshUnknown_eV9ctY_plus_OiOo_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_g3Lf8TiVEBM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("g3Lf8TiVEBM", libSceFsInternalForVshUnknown_g3Lf8TiVEBM_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_gcOITai35ls_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("gcOITai35ls", libSceFsInternalForVshUnknown_gcOITai35ls_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_gerE9e47NoU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("gerE9e47NoU", libSceFsInternalForVshUnknown_gerE9e47NoU_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_gtzIEP_plus_e2jo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("gtzIEP+e2jo", libSceFsInternalForVshUnknown_gtzIEP_plus_e2jo_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_hfb5Y906dYA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("hfb5Y906dYA", libSceFsInternalForVshUnknown_hfb5Y906dYA_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_hhPrQak7oFg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("hhPrQak7oFg", libSceFsInternalForVshUnknown_hhPrQak7oFg_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_iJQ7aolqS1A_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("iJQ7aolqS1A", libSceFsInternalForVshUnknown_iJQ7aolqS1A_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_juY2ugdopus_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("juY2ugdopus", libSceFsInternalForVshUnknown_juY2ugdopus_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_k_minus_WaWvM74jI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("k-WaWvM74jI", libSceFsInternalForVshUnknown_k_minus_WaWvM74jI_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_lZ_plus_oSeAyI8Q_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("lZ+oSeAyI8Q", libSceFsInternalForVshUnknown_lZ_plus_oSeAyI8Q_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_lxyJjALNmp4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("lxyJjALNmp4", libSceFsInternalForVshUnknown_lxyJjALNmp4_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_mvNIgoNVeWA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("mvNIgoNVeWA", libSceFsInternalForVshUnknown_mvNIgoNVeWA_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_nIMoeyl3SRY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("nIMoeyl3SRY", libSceFsInternalForVshUnknown_nIMoeyl3SRY_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_noVGZAYgjI8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("noVGZAYgjI8", libSceFsInternalForVshUnknown_noVGZAYgjI8_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_oP09BdVEs7o_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("oP09BdVEs7o", libSceFsInternalForVshUnknown_oP09BdVEs7o_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_pt_plus_umHHWgvM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("pt+umHHWgvM", libSceFsInternalForVshUnknown_pt_plus_umHHWgvM_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_rOoqs69ugy8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("rOoqs69ugy8", libSceFsInternalForVshUnknown_rOoqs69ugy8_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_rakMRjDRGS4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("rakMRjDRGS4", libSceFsInternalForVshUnknown_rakMRjDRGS4_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_s7X_minus_JxrF650_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("s7X-JxrF650", libSceFsInternalForVshUnknown_s7X_minus_JxrF650_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_v3jcGxw1mR8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("v3jcGxw1mR8", libSceFsInternalForVshUnknown_v3jcGxw1mR8_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_v9uqOhwlhjs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("v9uqOhwlhjs", libSceFsInternalForVshUnknown_v9uqOhwlhjs_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_yadgQseaDKA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("yadgQseaDKA", libSceFsInternalForVshUnknown_yadgQseaDKA_nid_no_patch);

int APS5_VABI libSceFsInternalForVshUnknown_ydRWehw26Kk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ydRWehw26Kk", libSceFsInternalForVshUnknown_ydRWehw26Kk_nid_no_patch);

}
