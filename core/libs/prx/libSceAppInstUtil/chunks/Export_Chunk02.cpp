extern "C" {
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceAppInstUtil/include/InventoryExports.hpp"
#include "prx/libSceAppInstUtil/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/AppMetadata/include/AppMetadata.hpp"

static std::mutex g_serviceMutex;
static int g_serviceReferences = 0;
static constexpr int ErrorNullArgument = static_cast<int>(0x80A30003);
static constexpr int ErrorNotInitialized = static_cast<int>(0x80A31000);

int APS5_VABI sceAppInstUtilGetAppEmptySlot(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppInstallGetLinkState(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppUnInstallForMultiInstall(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppInstallPs4CloudGame(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppStoreCompilationDiscInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppGetCompilationDiscInfo(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppGetStoreCompilationDiscInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppGetInsertedDiscTitleIdList(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppInstallRequestCompilationDiscApp(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppGetNoRelatedAddcontTitleList(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppGetIrremovableAppList(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppRequestMoveApps(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppProhibitDownloadInstall(void) {
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppAllowDownloadInstall(void) {
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilGetAppMoveProgressInfo(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppCancelMoveApps(void) {
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppGetMoveErrorAppList(const void* argument1, std::uint64_t argument2, const void* argument3) {
    if (argument1 == nullptr || argument3 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppGetAllAppSize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppCancelGetAllAppSize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppGetInsertedDiscMasterDataId(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppGetAppOtherSize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppCancelGetAppOtherSize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilGetInsertedDiscTotal(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppSetRemasterInstallType(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilGetInstallProgressInfoForMultiDisc(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilMountBdEmu(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilUnmountBdEmu(void) {
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilGetInstallStatus(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilResumeInstall(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilPauseInstall(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilRegisterDownload(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilInstall(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilStartInstall(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilInstallPatch(const void* argument1, const void* argument2, const void* argument3) {
    if (argument1 == nullptr || argument2 == nullptr || argument3 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilGetPatchInstallStatus(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilResumePatchInstall(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilPausePatchInstall(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilCancelInstallPatch(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilCancelInstallAddcont(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilCancelInstall(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilCancelForMultiInstall(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilInstallByPackage(const void* metaInfo, void* pkgInfo, const void* playGoInfo) {
    if (metaInfo == nullptr || pkgInfo == nullptr || playGoInfo == nullptr) return ErrorNullArgument;
    {
        std::lock_guard lock(g_serviceMutex);
        if (g_serviceReferences <= 0) return ErrorNotInitialized;
    }
    const auto uri = ReadMetaField(metaInfo, 0x00, 8, __func__);
    if (uri == 0) return ErrorNullArgument;
    struct Limit {
        std::size_t offset;
        std::size_t length;
    };
    constexpr Limit limits[] = {{0x00, 0x3fff}, {0x08, 0x3fff}, {0x10, 0x3}, {0x18, 0x30}, {0x20, 0x80}, {0x28, 0x3fff}};
    for (const Limit& limit : limits) {
        const auto text = limit.offset == 0 ? uri : ReadMetaField(metaInfo, limit.offset, 8, __func__);
        if (!MetaStringFits(text, limit.length, __func__)) return ErrorNullArgument;
    }
    ReadMetaField(metaInfo, 0x34, 1, __func__);
    ReadMetaField(metaInfo, 0x30, 4, __func__);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAppInstUtilIsRestoreState(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilGetRestoreState(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilGetAddons(const void* argument1, std::uint32_t argument2, std::uint32_t argument3, std::uint32_t argument4, const void* argument5) {
    if (argument1 == nullptr || argument5 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilGetInstallOptions(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilGetContentSize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilCancelGetInformation(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilGetWorkspaceStatus(const void* argument1, const void* argument2, const void* argument3) {
    if (argument1 == nullptr || argument2 == nullptr || argument3 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilDeleteWorkspace(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilGetParentalControlLevel(const void* argument1, std::uint32_t argument2, const void* argument3) {
    if (argument1 == nullptr || argument3 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilCheckLatestPatch(std::uint32_t argument1, const void* argument2, const void* argument3) {
    if (argument2 == nullptr || argument3 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilCancelCheckPatch(void) {
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppInstallPsNowIcon(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilGetAddcontInfoList2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilGetAddcontInfoList3(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilClearAddcontInfoCache(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilGetContentSize2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppInstallContentPush(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilGetAvailableStorage(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilSwitchInstallLocation(void) {
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilGetConfigList(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilSwitchConfig(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilRestoreDataSource(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
