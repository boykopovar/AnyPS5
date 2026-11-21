extern "C" {
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceFsInternalForVsh/include/InventoryExports.hpp"
#include "prx/libSceFsInternalForVsh/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

int InitWorkspaceOpt(void* opt) {
    const std::uint32_t zero = 0;
    const int savedErrno = errno;
    const bool copied = opt != nullptr && GuestCopyOut_nid_no_patch(opt, &zero, sizeof(zero));
    errno = savedErrno;
    return copied ? 0 : static_cast<int>(0x80020016u);
}

struct OptStore {
    std::uint32_t offset;
    std::uint32_t size;
    std::uint64_t value;
};

int InitOpt(void* opt, const OptStore* stores, std::size_t count) {
    if (opt == nullptr) return static_cast<int>(0x80020016u);
    const int savedErrno = errno;
    bool copied = true;
    for (std::size_t s = 0; copied && s < count; ++s) {
        std::uint8_t bytes[32] = {};
        for (std::uint32_t i = 0; i < stores[s].size && i < 8; ++i) {
            bytes[i] = static_cast<std::uint8_t>(stores[s].value >> (8 * i));
        }
        copied = GuestCopyOut_nid_no_patch(static_cast<std::uint8_t*>(opt) + stores[s].offset, bytes, stores[s].size);
    }
    errno = savedErrno;
    return copied ? 0 : static_cast<int>(0x80020016u);
}

}


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

int APS5_VABI sceFsInitCreatePfsSaveDataOpt(void* opt) {
    static constexpr OptStore stores[] = {{0x0, 4, 0x8000}, {0x4, 2, 0x0}};
    return InitOpt(opt, stores, sizeof(stores) / sizeof(stores[0]));
}

int APS5_VABI sceFsInitCreatePfsTrophyDataOpt(void* opt) {
    static constexpr OptStore stores[] = {{0x0, 4, 0x8000}, {0x4, 2, 0x0}};
    return InitOpt(opt, stores, sizeof(stores) / sizeof(stores[0]));
}

int APS5_VABI sceFsInitMountDownloadDataOpt() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFsInitMountGamePkgOpt(void* opt) {
    static constexpr OptStore stores[] = {{0x44, 4, 0x0}, {0x48, 8, 0x0}, {0x0, 32, 0x0}, {0x20, 4, 0x0}, {0x28, 16, 0x0}, {0x31, 16, 0x0}};
    return InitOpt(opt, stores, sizeof(stores) / sizeof(stores[0]));
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

int APS5_VABI sceFsInitUmountDownloadDataOpt(void* opt) {
    return InitOpt(opt, nullptr, 0);
}

int APS5_VABI sceFsInitUmountGamePkgOpt(void* opt) {
    static constexpr OptStore stores[] = {{0x0, 4, 0x1}};
    return InitOpt(opt, stores, sizeof(stores) / sizeof(stores[0]));
}

int APS5_VABI sceFsInitUmountLwfsCompatOpt(void* opt) {
    return InitOpt(opt, nullptr, 0);
}

int APS5_VABI sceFsInitUmountLwfsOpt(void* opt) {
    return InitOpt(opt, nullptr, 0);
}

int APS5_VABI sceFsInitUmountPprPkgOpt(void* opt) {
    static constexpr OptStore stores[] = {{0x0, 8, 0x0}, {0x8, 8, 0xffffffff00000000}, {0x10, 4, 0xffffffff}, {0x18, 8, 0x0}};
    return InitOpt(opt, stores, sizeof(stores) / sizeof(stores[0]));
}

int APS5_VABI sceFsInitUmountSaveDataOpt(void* opt) {
    return InitOpt(opt, nullptr, 0);
}

int APS5_VABI sceFsInitUmountTrophyDataOpt(void* opt) {
    return InitOpt(opt, nullptr, 0);
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

}
