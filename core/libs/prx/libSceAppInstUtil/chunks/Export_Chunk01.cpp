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

static bool ServiceInitialized() {
    std::lock_guard lock(g_serviceMutex);
    return g_serviceReferences > 0;
}
static constexpr int GuestEnomem = 12;
static constexpr int GuestEfault = 14;

[[noreturn]] static void FaultAt(const char* function, const char* what) {
    throw std::logic_error(std::string(function) + ": " + what + " faults in firmware 13.60");
}

static std::uint64_t ReadMetaField(const void* metaInfo, std::size_t offset, std::size_t size, const char* function) {
    std::uint64_t value = 0;
    if (!GuestCopyIn_nid_no_patch(&value, static_cast<const std::uint8_t*>(metaInfo) + offset, size)) FaultAt(function, "reading MetaInfo where it is not readable");
    return value;
}

static bool MetaStringFits(std::uint64_t text, std::size_t limit, const char* function) {
    if (text == 0) return true;
    std::string ignored;
    const int error = CopyGuestString_nid_no_patch(reinterpret_cast<const char*>(text), limit, ignored);
    if (error == GuestEfault) FaultAt(function, "reading a MetaInfo string where it is not readable");
    if (error != 0 && error != 63) throw std::bad_alloc();
    return error == 0;
}


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

}
