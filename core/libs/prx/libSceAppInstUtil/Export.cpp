#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceAppInstUtil/include/InventoryExports.hpp"
#include "prx/libSceAppInstUtil/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <new>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/AppMetadata/include/AppMetadata.hpp"

static std::mutex g_serviceMutex;
static int g_serviceReferences = 0;
static constexpr int ErrorNullArgument = static_cast<int>(0x80A30003);
static constexpr int ErrorNotInitialized = static_cast<int>(0x80A31000);

static bool ServiceInitialized() {
    std::lock_guard lock(g_serviceMutex);
    return g_serviceReferences > 0;
}
static constexpr int GuestEnomem = 12;
static constexpr int GuestEfault = 14;

extern "C" {

int APS5_VABI sceAppInstUtilInitialize(void) {
    std::lock_guard lock(g_serviceMutex);
    ++g_serviceReferences;
    return 0;
}
int APS5_VABI sceAppInstUtilTerminate(void) {
    std::lock_guard lock(g_serviceMutex);
    --g_serviceReferences;
    return 0;
}
int APS5_VABI sceAppInstUtilAppInstallAll(void* reserved) {
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceAppInstUtilAppInstallTitleDir(const char* titleId, const char* titleDir, void* reserved) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceAppInstUtilAppIsInInstalling(int32_t* installing) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceAppInstUtilAppIsInUpdating(const char* titleId, bool* updating) {
    const int savedErrno = errno;
    if (titleId == nullptr || updating == nullptr) return ErrorNullArgument;
    std::lock_guard lock(g_serviceMutex);
    if (g_serviceReferences <= 0) return ErrorNotInitialized;
    std::string requested;
    const int copyError = CopyGuestString_nid_no_patch(titleId, std::strlen(GetAppTitleId_nid_postfix().value) + 1, requested);
    errno = savedErrno;
    if (copyError == GuestEfault) return ErrorNullArgument;
    if (copyError == GuestEnomem) throw std::bad_alloc();
    const bool localUpdating = false;
    const bool copied = GuestCopyOut_nid_no_patch(updating, &localUpdating, sizeof(localUpdating));
    errno = savedErrno;
    if (!copied) return ErrorNullArgument;
    return 0;
}
int APS5_VABI sceAppInstUtilAppExists(const char* titleId, int32_t* exists) {
    const int savedErrno = errno;
    if (titleId == nullptr || exists == nullptr) return ErrorNullArgument;
    std::lock_guard lock(g_serviceMutex);
    if (g_serviceReferences <= 0) return ErrorNotInitialized;
    const AppTitleId running = GetAppTitleId_nid_postfix();
    std::string requested;
    const int copyError = CopyGuestString_nid_no_patch(titleId, std::strlen(running.value) + 1, requested);
    errno = savedErrno;
    if (copyError == GuestEfault) return ErrorNullArgument;
    if (copyError == GuestEnomem) throw std::bad_alloc();
    const int32_t localExists = copyError == 0 && requested == running.value ? 1 : 0;
    const bool copied = GuestCopyOut_nid_no_patch(exists, &localExists, sizeof(localExists));
    errno = savedErrno;
    if (!copied) return ErrorNullArgument;
    return 0;
}
int APS5_VABI sceAppInstUtilAppGetSize(const char* titleId, std::uint64_t* size) {
    if (titleId == nullptr || size == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceAppInstUtilAppInstallPkg(const char* packagePath, std::uint64_t serviceArgument) {
    if (packagePath == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceAppInstUtilAppPrepareOverwritePkg(const char* pkgPath) {
    (void)pkgPath;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceAppInstUtilAppRecoverApp(const char* titleId) {
    (void)titleId;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceAppInstUtilAppUnInstall(const char* titleId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceAppInstUtilAppUnInstallAddcont(const char* titleId, const char* contentLabel) {
    if (titleId == nullptr || contentLabel == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceAppInstUtilAppUnInstallPat(const char* titleId) {
    if (titleId == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceAppInstUtilAppUnInstallTheme(const char* contentId) {
    (void)contentId;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceAppInstUtilAppUnInstallTypes(const char* titleId, std::uint32_t types) {
    if (titleId == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceAppInstUtilGetInstallProgressInfo(const char* titleId, AppInstallProgressInfo* info) {
    if (titleId == nullptr || info == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceAppInstUtilGetInstallProgress(const char* titleId, void* progress) {
    if (titleId == nullptr || progress == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceAppInstUtilGetPrimaryAppSlot(const char* titleId, void* slot) {
    if (titleId == nullptr || slot == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilCheckLatestPatch2(std::uint32_t argument1, const void* argument2, const void* argument3) {
    if (argument2 == nullptr || argument3 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppGetIrremovableAppList2() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAppInstUtilGetAddons2() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAppInstUtilInstallPatch2(const void* argument1, const void* argument2, const void* argument3) {
    if (argument1 == nullptr || argument2 == nullptr || argument3 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppUnInstall3() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAppInstUtilGetPatchInstallStatus2(const void* argument1, const void* argument2, const void* argument3) {
    if (argument1 == nullptr || argument2 == nullptr || argument3 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppRequestMoveApps2(std::uint32_t argument1, std::uint32_t argument2, const void* argument3) {
    if (argument3 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilGetContentSize3() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAppInstUtilAppGetAppInstallStatusByAppInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppUnInstall2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppUnInstallForRemote(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppUnInstallForRemote2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppGetAddcontListTotalSize(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppGetAddcontInfo(const void* argument1, const void* argument2, const void* argument3) {
    if (argument1 == nullptr || argument2 == nullptr || argument3 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppGetAddcontInfoByContentId(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilGetAddcontInstalledStatus(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppGetInsertedDiscTitleId(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppUpdateStartTime(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppInstallSharePlayLink(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppInstallSharePlayLinkByUser(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppGetAddcontNum(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppCancelUnInstall(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppDestroyMetadata(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilCheckAppSystemVer(const void* argument1, const void* argument2, const void* argument3) {
    if (argument1 == nullptr || argument2 == nullptr || argument3 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppConvertAppDownloadToDisc(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilGetMainAppTitleIdForSP(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilGetSubAppTitleIdForSP(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilGetMainAppTitleId(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilGetSubAppTitleId(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppGetAppInstallStatus(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppDestroyPkg(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilJson(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilGetInsertedDiscRoleType(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilRequestInstallDiscGame(void) {
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilRequestDataDiscCopy(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilCancelDataDiscCopy(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilGetDataDiscCopyProgress(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilSetPrimaryAppSlot(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilGetAppSlotInfoList(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

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

int APS5_VABI sceAppInstUtilInstallByPackage(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
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

int APS5_VABI sceAppInstUtilGetNeededStorageSize(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppGetStorageDestType(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilInstallAddons(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppConvertAppDiscToDownload(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppGetCustomThemeInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppGetInstallAddcontFromDiscStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppInstallCloudGame(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppInstallMediaPlayer(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppInstallRequestAddcontFromDisc(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppSetSavedataUploadStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {





int APS5_VABI libSceAppInstUtilUnknown_2WQuFX8yYXg_nid_no_patch(const void* argument1, const void* argument2, const void* argument3) {
    if (argument1 == nullptr || argument2 == nullptr || argument3 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("2WQuFX8yYXg", libSceAppInstUtilUnknown_2WQuFX8yYXg_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_4vqRxwpIE1M_nid_no_patch(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("4vqRxwpIE1M", libSceAppInstUtilUnknown_4vqRxwpIE1M_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_9QIC1kZpSG4_nid_no_patch() {
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("9QIC1kZpSG4", libSceAppInstUtilUnknown_9QIC1kZpSG4_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_K73u2VX0JFo_nid_no_patch(const void* argument1, const void* argument2, std::uint64_t argument3, const void* argument4) {
    if (argument1 == nullptr || argument2 == nullptr || argument4 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("K73u2VX0JFo", libSceAppInstUtilUnknown_K73u2VX0JFo_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_LsUZe38RhCs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("LsUZe38RhCs", libSceAppInstUtilUnknown_LsUZe38RhCs_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_N6H7LMfZD5Q_nid_no_patch() {
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("N6H7LMfZD5Q", libSceAppInstUtilUnknown_N6H7LMfZD5Q_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_NXqdUFpinmo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("NXqdUFpinmo", libSceAppInstUtilUnknown_NXqdUFpinmo_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_QMjuGVPtjUY_nid_no_patch(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("QMjuGVPtjUY", libSceAppInstUtilUnknown_QMjuGVPtjUY_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_R_minus_OUApMZ1d8_nid_no_patch(const void* argument1, const void* argument2, const void* argument3) {
    if (argument1 == nullptr || argument2 == nullptr || argument3 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("R-OUApMZ1d8", libSceAppInstUtilUnknown_R_minus_OUApMZ1d8_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_T9h6l8bFKGE_nid_no_patch() {
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("T9h6l8bFKGE", libSceAppInstUtilUnknown_T9h6l8bFKGE_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_X5Q7QxodFcg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("X5Q7QxodFcg", libSceAppInstUtilUnknown_X5Q7QxodFcg_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_XWjOLtdgxa4_nid_no_patch() {
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("XWjOLtdgxa4", libSceAppInstUtilUnknown_XWjOLtdgxa4_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_Y_plus_ln_minus_TkY_plus_hA_nid_no_patch(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("Y+ln-TkY+hA", libSceAppInstUtilUnknown_Y_plus_ln_minus_TkY_plus_hA_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_Zv_plus_26GDSKcQ_nid_no_patch(const void* argument1, std::uint64_t argument2, const void* argument3, const void* argument4) {
    if (argument1 == nullptr || argument3 == nullptr || argument4 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("Zv+26GDSKcQ", libSceAppInstUtilUnknown_Zv_plus_26GDSKcQ_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_ecad2GAw55Y_nid_no_patch() {
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("ecad2GAw55Y", libSceAppInstUtilUnknown_ecad2GAw55Y_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_gFlBHjtrMDI_nid_no_patch() {
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("gFlBHjtrMDI", libSceAppInstUtilUnknown_gFlBHjtrMDI_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_gP27iU_plus_aZFw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("gP27iU+aZFw", libSceAppInstUtilUnknown_gP27iU_plus_aZFw_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_grC16befiso_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("grC16befiso", libSceAppInstUtilUnknown_grC16befiso_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_j80u95nZ_plus_3A_nid_no_patch() {
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("j80u95nZ+3A", libSceAppInstUtilUnknown_j80u95nZ_plus_3A_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_j9qjbjMifhQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("j9qjbjMifhQ", libSceAppInstUtilUnknown_j9qjbjMifhQ_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_pbYNtR2enrY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("pbYNtR2enrY", libSceAppInstUtilUnknown_pbYNtR2enrY_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_qn0aziKw_minus_kE_nid_no_patch(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("qn0aziKw-kE", libSceAppInstUtilUnknown_qn0aziKw_minus_kE_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_qxjCWxg8ki4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("qxjCWxg8ki4", libSceAppInstUtilUnknown_qxjCWxg8ki4_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_vZ1SWJ0y4mo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("vZ1SWJ0y4mo", libSceAppInstUtilUnknown_vZ1SWJ0y4mo_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_xV7MhF_plus_9AQk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("xV7MhF+9AQk", libSceAppInstUtilUnknown_xV7MhF_plus_9AQk_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_yYVV_minus_f0zoWY_nid_no_patch(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("yYVV-f0zoWY", libSceAppInstUtilUnknown_yYVV_minus_f0zoWY_nid_no_patch);

}
