#include "prx/libSceAppInstUtil/include/Api.hpp"
#include "prx/libSceAppInstUtil/include/InventoryExports.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "AppInstUtil: %s\n", message);
        std::abort();
    }
}

char* GuardedPageEnd() {
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::size_t pageSize = info.dwPageSize;
    auto* memory = static_cast<char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(memory != nullptr, "the guarded page was not allocated");
    DWORD previous;
    Require(VirtualProtect(memory + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0, "the guard page was not protected");
#else
    const long nativePageSize = sysconf(_SC_PAGESIZE);
    Require(nativePageSize > 0, "the page size is unknown");
    const std::size_t pageSize = static_cast<std::size_t>(nativePageSize);
    void* allocation = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(allocation != MAP_FAILED, "the guarded page was not mapped");
    auto* memory = static_cast<char*>(allocation);
    Require(mprotect(memory + pageSize, pageSize, PROT_NONE) == 0, "the guard page was not protected");
#endif
    return memory + pageSize;
}

char* EndingAt(char* end, const char* bytes, std::size_t count) {
    return static_cast<char*>(std::memcpy(end - count, bytes, count));
}

void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = 99;
}

}

void CheckServiceChecks() {
    constexpr int nullArgument = static_cast<int>(0x80A30003u);
    constexpr int notInitialized = static_cast<int>(0x80A31000u);
    alignas(16) unsigned char block[256]{};
    Require(sceAppInstUtilAppInstallAll(reinterpret_cast<void*>(block)) == notInitialized, "sceAppInstUtilAppInstallAll answered before initialization");
    Require(sceAppInstUtilAppInstallAll(nullptr) == notInitialized, "sceAppInstUtilAppInstallAll must ignore a null argument 1");
    Require(sceAppInstUtilAppGetSize(reinterpret_cast<const char*>(block), reinterpret_cast<std::uint64_t*>(block)) == notInitialized, "sceAppInstUtilAppGetSize answered before initialization");
    Require(sceAppInstUtilAppGetSize(nullptr, reinterpret_cast<std::uint64_t*>(block)) == nullArgument, "sceAppInstUtilAppGetSize must refuse a null argument 1");
    Require(sceAppInstUtilAppGetSize(reinterpret_cast<const char*>(block), nullptr) == nullArgument, "sceAppInstUtilAppGetSize must refuse a null argument 2");
    Require(sceAppInstUtilAppInstallPkg(reinterpret_cast<const char*>(block), static_cast<std::uint64_t>(2)) == notInitialized, "sceAppInstUtilAppInstallPkg answered before initialization");
    Require(sceAppInstUtilAppInstallPkg(nullptr, static_cast<std::uint64_t>(2)) == nullArgument, "sceAppInstUtilAppInstallPkg must refuse a null argument 1");
    Require(sceAppInstUtilAppUnInstallAddcont(reinterpret_cast<const char*>(block), reinterpret_cast<const char*>(block)) == notInitialized, "sceAppInstUtilAppUnInstallAddcont answered before initialization");
    Require(sceAppInstUtilAppUnInstallAddcont(nullptr, reinterpret_cast<const char*>(block)) == nullArgument, "sceAppInstUtilAppUnInstallAddcont must refuse a null argument 1");
    Require(sceAppInstUtilAppUnInstallAddcont(reinterpret_cast<const char*>(block), nullptr) == nullArgument, "sceAppInstUtilAppUnInstallAddcont must refuse a null argument 2");
    Require(sceAppInstUtilAppUnInstallPat(reinterpret_cast<const char*>(block)) == notInitialized, "sceAppInstUtilAppUnInstallPat answered before initialization");
    Require(sceAppInstUtilAppUnInstallPat(nullptr) == nullArgument, "sceAppInstUtilAppUnInstallPat must refuse a null argument 1");
    Require(sceAppInstUtilAppUnInstallTypes(reinterpret_cast<const char*>(block), static_cast<std::uint32_t>(2)) == notInitialized, "sceAppInstUtilAppUnInstallTypes answered before initialization");
    Require(sceAppInstUtilAppUnInstallTypes(nullptr, static_cast<std::uint32_t>(2)) == nullArgument, "sceAppInstUtilAppUnInstallTypes must refuse a null argument 1");
    Require(sceAppInstUtilGetInstallProgressInfo(reinterpret_cast<const char*>(block), reinterpret_cast<AppInstallProgressInfo*>(block)) == notInitialized, "sceAppInstUtilGetInstallProgressInfo answered before initialization");
    Require(sceAppInstUtilGetInstallProgressInfo(nullptr, reinterpret_cast<AppInstallProgressInfo*>(block)) == nullArgument, "sceAppInstUtilGetInstallProgressInfo must refuse a null argument 1");
    Require(sceAppInstUtilGetInstallProgressInfo(reinterpret_cast<const char*>(block), nullptr) == nullArgument, "sceAppInstUtilGetInstallProgressInfo must refuse a null argument 2");
    Require(sceAppInstUtilGetInstallProgress(reinterpret_cast<const char*>(block), reinterpret_cast<void*>(block)) == notInitialized, "sceAppInstUtilGetInstallProgress answered before initialization");
    Require(sceAppInstUtilGetInstallProgress(nullptr, reinterpret_cast<void*>(block)) == nullArgument, "sceAppInstUtilGetInstallProgress must refuse a null argument 1");
    Require(sceAppInstUtilGetInstallProgress(reinterpret_cast<const char*>(block), nullptr) == nullArgument, "sceAppInstUtilGetInstallProgress must refuse a null argument 2");
    Require(sceAppInstUtilGetPrimaryAppSlot(reinterpret_cast<const char*>(block), reinterpret_cast<void*>(block)) == notInitialized, "sceAppInstUtilGetPrimaryAppSlot answered before initialization");
    Require(sceAppInstUtilGetPrimaryAppSlot(nullptr, reinterpret_cast<void*>(block)) == nullArgument, "sceAppInstUtilGetPrimaryAppSlot must refuse a null argument 1");
    Require(sceAppInstUtilGetPrimaryAppSlot(reinterpret_cast<const char*>(block), nullptr) == nullArgument, "sceAppInstUtilGetPrimaryAppSlot must refuse a null argument 2");
    Require(sceAppInstUtilCheckLatestPatch2(static_cast<std::uint32_t>(1), reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilCheckLatestPatch2 answered before initialization");
    Require(sceAppInstUtilCheckLatestPatch2(static_cast<std::uint32_t>(1), nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilCheckLatestPatch2 must refuse a null argument 2");
    Require(sceAppInstUtilCheckLatestPatch2(static_cast<std::uint32_t>(1), reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilCheckLatestPatch2 must refuse a null argument 3");
    Require(sceAppInstUtilInstallPatch2(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilInstallPatch2 answered before initialization");
    Require(sceAppInstUtilInstallPatch2(nullptr, reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilInstallPatch2 must refuse a null argument 1");
    Require(sceAppInstUtilInstallPatch2(reinterpret_cast<const void*>(block), nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilInstallPatch2 must refuse a null argument 2");
    Require(sceAppInstUtilInstallPatch2(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilInstallPatch2 must refuse a null argument 3");
    Require(sceAppInstUtilGetPatchInstallStatus2(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilGetPatchInstallStatus2 answered before initialization");
    Require(sceAppInstUtilGetPatchInstallStatus2(nullptr, reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilGetPatchInstallStatus2 must refuse a null argument 1");
    Require(sceAppInstUtilGetPatchInstallStatus2(reinterpret_cast<const void*>(block), nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilGetPatchInstallStatus2 must refuse a null argument 2");
    Require(sceAppInstUtilGetPatchInstallStatus2(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilGetPatchInstallStatus2 must refuse a null argument 3");
    Require(sceAppInstUtilAppRequestMoveApps2(static_cast<std::uint32_t>(1), static_cast<std::uint32_t>(2), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilAppRequestMoveApps2 answered before initialization");
    Require(sceAppInstUtilAppRequestMoveApps2(static_cast<std::uint32_t>(1), static_cast<std::uint32_t>(2), nullptr) == nullArgument, "sceAppInstUtilAppRequestMoveApps2 must refuse a null argument 3");
    Require(sceAppInstUtilAppGetAddcontListTotalSize(reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilAppGetAddcontListTotalSize answered before initialization");
    Require(sceAppInstUtilAppGetAddcontListTotalSize(nullptr) == nullArgument, "sceAppInstUtilAppGetAddcontListTotalSize must refuse a null argument 1");
    Require(sceAppInstUtilAppGetAddcontInfo(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilAppGetAddcontInfo answered before initialization");
    Require(sceAppInstUtilAppGetAddcontInfo(nullptr, reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilAppGetAddcontInfo must refuse a null argument 1");
    Require(sceAppInstUtilAppGetAddcontInfo(reinterpret_cast<const void*>(block), nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilAppGetAddcontInfo must refuse a null argument 2");
    Require(sceAppInstUtilAppGetAddcontInfo(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilAppGetAddcontInfo must refuse a null argument 3");
    Require(sceAppInstUtilAppGetAddcontInfoByContentId(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilAppGetAddcontInfoByContentId answered before initialization");
    Require(sceAppInstUtilAppGetAddcontInfoByContentId(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilAppGetAddcontInfoByContentId must refuse a null argument 1");
    Require(sceAppInstUtilAppGetAddcontInfoByContentId(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilAppGetAddcontInfoByContentId must refuse a null argument 2");
    Require(sceAppInstUtilGetAddcontInstalledStatus(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilGetAddcontInstalledStatus answered before initialization");
    Require(sceAppInstUtilGetAddcontInstalledStatus(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilGetAddcontInstalledStatus must refuse a null argument 1");
    Require(sceAppInstUtilGetAddcontInstalledStatus(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilGetAddcontInstalledStatus must refuse a null argument 2");
    Require(sceAppInstUtilAppGetInsertedDiscTitleId(reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilAppGetInsertedDiscTitleId answered before initialization");
    Require(sceAppInstUtilAppGetInsertedDiscTitleId(nullptr) == nullArgument, "sceAppInstUtilAppGetInsertedDiscTitleId must refuse a null argument 1");
    Require(sceAppInstUtilAppUpdateStartTime(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilAppUpdateStartTime answered before initialization");
    Require(sceAppInstUtilAppUpdateStartTime(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilAppUpdateStartTime must refuse a null argument 1");
    Require(sceAppInstUtilAppUpdateStartTime(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilAppUpdateStartTime must refuse a null argument 2");
    Require(sceAppInstUtilAppInstallSharePlayLink(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilAppInstallSharePlayLink answered before initialization");
    Require(sceAppInstUtilAppInstallSharePlayLink(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilAppInstallSharePlayLink must refuse a null argument 1");
    Require(sceAppInstUtilAppInstallSharePlayLink(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilAppInstallSharePlayLink must refuse a null argument 2");
    Require(sceAppInstUtilAppInstallSharePlayLinkByUser(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilAppInstallSharePlayLinkByUser answered before initialization");
    Require(sceAppInstUtilAppInstallSharePlayLinkByUser(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilAppInstallSharePlayLinkByUser must refuse a null argument 1");
    Require(sceAppInstUtilAppInstallSharePlayLinkByUser(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilAppInstallSharePlayLinkByUser must refuse a null argument 2");
    Require(sceAppInstUtilAppGetAddcontNum(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilAppGetAddcontNum answered before initialization");
    Require(sceAppInstUtilAppGetAddcontNum(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilAppGetAddcontNum must refuse a null argument 1");
    Require(sceAppInstUtilAppGetAddcontNum(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilAppGetAddcontNum must refuse a null argument 2");
    Require(sceAppInstUtilAppDestroyMetadata(reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilAppDestroyMetadata answered before initialization");
    Require(sceAppInstUtilAppDestroyMetadata(nullptr) == nullArgument, "sceAppInstUtilAppDestroyMetadata must refuse a null argument 1");
    Require(sceAppInstUtilCheckAppSystemVer(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilCheckAppSystemVer answered before initialization");
    Require(sceAppInstUtilCheckAppSystemVer(nullptr, reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilCheckAppSystemVer must refuse a null argument 1");
    Require(sceAppInstUtilCheckAppSystemVer(reinterpret_cast<const void*>(block), nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilCheckAppSystemVer must refuse a null argument 2");
    Require(sceAppInstUtilCheckAppSystemVer(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilCheckAppSystemVer must refuse a null argument 3");
    Require(sceAppInstUtilAppConvertAppDownloadToDisc(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilAppConvertAppDownloadToDisc answered before initialization");
    Require(sceAppInstUtilAppConvertAppDownloadToDisc(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilAppConvertAppDownloadToDisc must refuse a null argument 1");
    Require(sceAppInstUtilAppConvertAppDownloadToDisc(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilAppConvertAppDownloadToDisc must refuse a null argument 2");
    Require(sceAppInstUtilGetMainAppTitleIdForSP(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilGetMainAppTitleIdForSP answered before initialization");
    Require(sceAppInstUtilGetMainAppTitleIdForSP(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilGetMainAppTitleIdForSP must refuse a null argument 1");
    Require(sceAppInstUtilGetMainAppTitleIdForSP(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilGetMainAppTitleIdForSP must refuse a null argument 2");
    Require(sceAppInstUtilGetSubAppTitleIdForSP(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilGetSubAppTitleIdForSP answered before initialization");
    Require(sceAppInstUtilGetSubAppTitleIdForSP(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilGetSubAppTitleIdForSP must refuse a null argument 1");
    Require(sceAppInstUtilGetSubAppTitleIdForSP(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilGetSubAppTitleIdForSP must refuse a null argument 2");
    Require(sceAppInstUtilGetMainAppTitleId(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilGetMainAppTitleId answered before initialization");
    Require(sceAppInstUtilGetMainAppTitleId(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilGetMainAppTitleId must refuse a null argument 1");
    Require(sceAppInstUtilGetMainAppTitleId(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilGetMainAppTitleId must refuse a null argument 2");
    Require(sceAppInstUtilGetSubAppTitleId(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilGetSubAppTitleId answered before initialization");
    Require(sceAppInstUtilGetSubAppTitleId(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilGetSubAppTitleId must refuse a null argument 1");
    Require(sceAppInstUtilGetSubAppTitleId(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilGetSubAppTitleId must refuse a null argument 2");
    Require(sceAppInstUtilAppGetAppInstallStatus(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilAppGetAppInstallStatus answered before initialization");
    Require(sceAppInstUtilAppGetAppInstallStatus(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilAppGetAppInstallStatus must refuse a null argument 1");
    Require(sceAppInstUtilAppGetAppInstallStatus(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilAppGetAppInstallStatus must refuse a null argument 2");
    Require(sceAppInstUtilAppDestroyPkg(reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilAppDestroyPkg answered before initialization");
    Require(sceAppInstUtilAppDestroyPkg(nullptr) == nullArgument, "sceAppInstUtilAppDestroyPkg must refuse a null argument 1");
    Require(sceAppInstUtilJson(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilJson answered before initialization");
    Require(sceAppInstUtilJson(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilJson must refuse a null argument 1");
    Require(sceAppInstUtilJson(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilJson must refuse a null argument 2");
    Require(sceAppInstUtilGetInsertedDiscRoleType(reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilGetInsertedDiscRoleType answered before initialization");
    Require(sceAppInstUtilGetInsertedDiscRoleType(nullptr) == nullArgument, "sceAppInstUtilGetInsertedDiscRoleType must refuse a null argument 1");
    Require(sceAppInstUtilRequestInstallDiscGame() == notInitialized, "sceAppInstUtilRequestInstallDiscGame answered before initialization");
    Require(sceAppInstUtilRequestDataDiscCopy(reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilRequestDataDiscCopy answered before initialization");
    Require(sceAppInstUtilRequestDataDiscCopy(nullptr) == nullArgument, "sceAppInstUtilRequestDataDiscCopy must refuse a null argument 1");
    Require(sceAppInstUtilCancelDataDiscCopy(reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilCancelDataDiscCopy answered before initialization");
    Require(sceAppInstUtilCancelDataDiscCopy(nullptr) == nullArgument, "sceAppInstUtilCancelDataDiscCopy must refuse a null argument 1");
    Require(sceAppInstUtilGetDataDiscCopyProgress(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilGetDataDiscCopyProgress answered before initialization");
    Require(sceAppInstUtilGetDataDiscCopyProgress(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilGetDataDiscCopyProgress must refuse a null argument 1");
    Require(sceAppInstUtilGetDataDiscCopyProgress(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilGetDataDiscCopyProgress must refuse a null argument 2");
    Require(sceAppInstUtilSetPrimaryAppSlot(reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilSetPrimaryAppSlot answered before initialization");
    Require(sceAppInstUtilSetPrimaryAppSlot(nullptr) == nullArgument, "sceAppInstUtilSetPrimaryAppSlot must refuse a null argument 1");
    Require(sceAppInstUtilGetAppSlotInfoList(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilGetAppSlotInfoList answered before initialization");
    Require(sceAppInstUtilGetAppSlotInfoList(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilGetAppSlotInfoList must refuse a null argument 1");
    Require(sceAppInstUtilGetAppSlotInfoList(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilGetAppSlotInfoList must refuse a null argument 2");
    Require(sceAppInstUtilGetAppEmptySlot(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilGetAppEmptySlot answered before initialization");
    Require(sceAppInstUtilGetAppEmptySlot(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilGetAppEmptySlot must refuse a null argument 1");
    Require(sceAppInstUtilGetAppEmptySlot(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilGetAppEmptySlot must refuse a null argument 2");
    Require(sceAppInstUtilAppInstallGetLinkState(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilAppInstallGetLinkState answered before initialization");
    Require(sceAppInstUtilAppInstallGetLinkState(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilAppInstallGetLinkState must refuse a null argument 1");
    Require(sceAppInstUtilAppInstallGetLinkState(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilAppInstallGetLinkState must refuse a null argument 2");
    Require(sceAppInstUtilAppUnInstallForMultiInstall(reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilAppUnInstallForMultiInstall answered before initialization");
    Require(sceAppInstUtilAppUnInstallForMultiInstall(nullptr) == nullArgument, "sceAppInstUtilAppUnInstallForMultiInstall must refuse a null argument 1");
    Require(sceAppInstUtilAppInstallPs4CloudGame(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilAppInstallPs4CloudGame answered before initialization");
    Require(sceAppInstUtilAppInstallPs4CloudGame(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilAppInstallPs4CloudGame must refuse a null argument 1");
    Require(sceAppInstUtilAppInstallPs4CloudGame(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilAppInstallPs4CloudGame must refuse a null argument 2");
    Require(sceAppInstUtilAppGetCompilationDiscInfo(reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilAppGetCompilationDiscInfo answered before initialization");
    Require(sceAppInstUtilAppGetCompilationDiscInfo(nullptr) == nullArgument, "sceAppInstUtilAppGetCompilationDiscInfo must refuse a null argument 1");
    Require(sceAppInstUtilAppInstallRequestCompilationDiscApp(reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilAppInstallRequestCompilationDiscApp answered before initialization");
    Require(sceAppInstUtilAppInstallRequestCompilationDiscApp(nullptr) == nullArgument, "sceAppInstUtilAppInstallRequestCompilationDiscApp must refuse a null argument 1");
    Require(sceAppInstUtilAppProhibitDownloadInstall() == notInitialized, "sceAppInstUtilAppProhibitDownloadInstall answered before initialization");
    Require(sceAppInstUtilAppAllowDownloadInstall() == notInitialized, "sceAppInstUtilAppAllowDownloadInstall answered before initialization");
    Require(sceAppInstUtilGetAppMoveProgressInfo(reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilGetAppMoveProgressInfo answered before initialization");
    Require(sceAppInstUtilGetAppMoveProgressInfo(nullptr) == nullArgument, "sceAppInstUtilGetAppMoveProgressInfo must refuse a null argument 1");
    Require(sceAppInstUtilAppCancelMoveApps() == notInitialized, "sceAppInstUtilAppCancelMoveApps answered before initialization");
    Require(sceAppInstUtilAppGetMoveErrorAppList(reinterpret_cast<const void*>(block), static_cast<std::uint64_t>(2), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilAppGetMoveErrorAppList answered before initialization");
    Require(sceAppInstUtilAppGetMoveErrorAppList(nullptr, static_cast<std::uint64_t>(2), reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilAppGetMoveErrorAppList must refuse a null argument 1");
    Require(sceAppInstUtilAppGetMoveErrorAppList(reinterpret_cast<const void*>(block), static_cast<std::uint64_t>(2), nullptr) == nullArgument, "sceAppInstUtilAppGetMoveErrorAppList must refuse a null argument 3");
    Require(sceAppInstUtilAppGetInsertedDiscMasterDataId(reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilAppGetInsertedDiscMasterDataId answered before initialization");
    Require(sceAppInstUtilAppGetInsertedDiscMasterDataId(nullptr) == nullArgument, "sceAppInstUtilAppGetInsertedDiscMasterDataId must refuse a null argument 1");
    Require(sceAppInstUtilGetInsertedDiscTotal(reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilGetInsertedDiscTotal answered before initialization");
    Require(sceAppInstUtilGetInsertedDiscTotal(nullptr) == nullArgument, "sceAppInstUtilGetInsertedDiscTotal must refuse a null argument 1");
    Require(sceAppInstUtilAppSetRemasterInstallType(reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilAppSetRemasterInstallType answered before initialization");
    Require(sceAppInstUtilAppSetRemasterInstallType(nullptr) == nullArgument, "sceAppInstUtilAppSetRemasterInstallType must refuse a null argument 1");
    Require(sceAppInstUtilGetInstallProgressInfoForMultiDisc(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilGetInstallProgressInfoForMultiDisc answered before initialization");
    Require(sceAppInstUtilGetInstallProgressInfoForMultiDisc(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilGetInstallProgressInfoForMultiDisc must refuse a null argument 1");
    Require(sceAppInstUtilGetInstallProgressInfoForMultiDisc(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilGetInstallProgressInfoForMultiDisc must refuse a null argument 2");
    Require(sceAppInstUtilMountBdEmu(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilMountBdEmu answered before initialization");
    Require(sceAppInstUtilMountBdEmu(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilMountBdEmu must refuse a null argument 1");
    Require(sceAppInstUtilMountBdEmu(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilMountBdEmu must refuse a null argument 2");
    Require(sceAppInstUtilUnmountBdEmu() == notInitialized, "sceAppInstUtilUnmountBdEmu answered before initialization");
    Require(sceAppInstUtilGetInstallStatus(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilGetInstallStatus answered before initialization");
    Require(sceAppInstUtilGetInstallStatus(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilGetInstallStatus must refuse a null argument 1");
    Require(sceAppInstUtilGetInstallStatus(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilGetInstallStatus must refuse a null argument 2");
    Require(sceAppInstUtilResumeInstall(reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilResumeInstall answered before initialization");
    Require(sceAppInstUtilResumeInstall(nullptr) == nullArgument, "sceAppInstUtilResumeInstall must refuse a null argument 1");
    Require(sceAppInstUtilPauseInstall(reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilPauseInstall answered before initialization");
    Require(sceAppInstUtilPauseInstall(nullptr) == nullArgument, "sceAppInstUtilPauseInstall must refuse a null argument 1");
    Require(sceAppInstUtilRegisterDownload(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilRegisterDownload answered before initialization");
    Require(sceAppInstUtilRegisterDownload(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilRegisterDownload must refuse a null argument 1");
    Require(sceAppInstUtilRegisterDownload(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilRegisterDownload must refuse a null argument 2");
    Require(sceAppInstUtilStartInstall(reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilStartInstall answered before initialization");
    Require(sceAppInstUtilStartInstall(nullptr) == nullArgument, "sceAppInstUtilStartInstall must refuse a null argument 1");
    Require(sceAppInstUtilInstallPatch(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilInstallPatch answered before initialization");
    Require(sceAppInstUtilInstallPatch(nullptr, reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilInstallPatch must refuse a null argument 1");
    Require(sceAppInstUtilInstallPatch(reinterpret_cast<const void*>(block), nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilInstallPatch must refuse a null argument 2");
    Require(sceAppInstUtilInstallPatch(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilInstallPatch must refuse a null argument 3");
    Require(sceAppInstUtilGetPatchInstallStatus(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilGetPatchInstallStatus answered before initialization");
    Require(sceAppInstUtilGetPatchInstallStatus(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilGetPatchInstallStatus must refuse a null argument 1");
    Require(sceAppInstUtilGetPatchInstallStatus(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilGetPatchInstallStatus must refuse a null argument 2");
    Require(sceAppInstUtilResumePatchInstall(reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilResumePatchInstall answered before initialization");
    Require(sceAppInstUtilResumePatchInstall(nullptr) == nullArgument, "sceAppInstUtilResumePatchInstall must refuse a null argument 1");
    Require(sceAppInstUtilPausePatchInstall(reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilPausePatchInstall answered before initialization");
    Require(sceAppInstUtilPausePatchInstall(nullptr) == nullArgument, "sceAppInstUtilPausePatchInstall must refuse a null argument 1");
    Require(sceAppInstUtilCancelInstallPatch(reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilCancelInstallPatch answered before initialization");
    Require(sceAppInstUtilCancelInstallPatch(nullptr) == nullArgument, "sceAppInstUtilCancelInstallPatch must refuse a null argument 1");
    Require(sceAppInstUtilCancelInstallAddcont(reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilCancelInstallAddcont answered before initialization");
    Require(sceAppInstUtilCancelInstallAddcont(nullptr) == nullArgument, "sceAppInstUtilCancelInstallAddcont must refuse a null argument 1");
    Require(sceAppInstUtilCancelInstall(reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilCancelInstall answered before initialization");
    Require(sceAppInstUtilCancelInstall(nullptr) == nullArgument, "sceAppInstUtilCancelInstall must refuse a null argument 1");
    Require(sceAppInstUtilCancelForMultiInstall(reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilCancelForMultiInstall answered before initialization");
    Require(sceAppInstUtilCancelForMultiInstall(nullptr) == nullArgument, "sceAppInstUtilCancelForMultiInstall must refuse a null argument 1");
    Require(sceAppInstUtilIsRestoreState(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilIsRestoreState answered before initialization");
    Require(sceAppInstUtilIsRestoreState(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilIsRestoreState must refuse a null argument 1");
    Require(sceAppInstUtilIsRestoreState(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilIsRestoreState must refuse a null argument 2");
    Require(sceAppInstUtilGetRestoreState(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilGetRestoreState answered before initialization");
    Require(sceAppInstUtilGetRestoreState(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilGetRestoreState must refuse a null argument 1");
    Require(sceAppInstUtilGetRestoreState(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilGetRestoreState must refuse a null argument 2");
    Require(sceAppInstUtilGetAddons(reinterpret_cast<const void*>(block), static_cast<std::uint32_t>(2), static_cast<std::uint32_t>(3), static_cast<std::uint32_t>(4), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilGetAddons answered before initialization");
    Require(sceAppInstUtilGetAddons(nullptr, static_cast<std::uint32_t>(2), static_cast<std::uint32_t>(3), static_cast<std::uint32_t>(4), reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilGetAddons must refuse a null argument 1");
    Require(sceAppInstUtilGetAddons(reinterpret_cast<const void*>(block), static_cast<std::uint32_t>(2), static_cast<std::uint32_t>(3), static_cast<std::uint32_t>(4), nullptr) == nullArgument, "sceAppInstUtilGetAddons must refuse a null argument 5");
    Require(sceAppInstUtilGetWorkspaceStatus(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilGetWorkspaceStatus answered before initialization");
    Require(sceAppInstUtilGetWorkspaceStatus(nullptr, reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilGetWorkspaceStatus must refuse a null argument 1");
    Require(sceAppInstUtilGetWorkspaceStatus(reinterpret_cast<const void*>(block), nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilGetWorkspaceStatus must refuse a null argument 2");
    Require(sceAppInstUtilGetWorkspaceStatus(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilGetWorkspaceStatus must refuse a null argument 3");
    Require(sceAppInstUtilDeleteWorkspace(reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilDeleteWorkspace answered before initialization");
    Require(sceAppInstUtilDeleteWorkspace(nullptr) == nullArgument, "sceAppInstUtilDeleteWorkspace must refuse a null argument 1");
    Require(sceAppInstUtilGetParentalControlLevel(reinterpret_cast<const void*>(block), static_cast<std::uint32_t>(2), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilGetParentalControlLevel answered before initialization");
    Require(sceAppInstUtilGetParentalControlLevel(nullptr, static_cast<std::uint32_t>(2), reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilGetParentalControlLevel must refuse a null argument 1");
    Require(sceAppInstUtilGetParentalControlLevel(reinterpret_cast<const void*>(block), static_cast<std::uint32_t>(2), nullptr) == nullArgument, "sceAppInstUtilGetParentalControlLevel must refuse a null argument 3");
    Require(sceAppInstUtilCheckLatestPatch(static_cast<std::uint32_t>(1), reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilCheckLatestPatch answered before initialization");
    Require(sceAppInstUtilCheckLatestPatch(static_cast<std::uint32_t>(1), nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilCheckLatestPatch must refuse a null argument 2");
    Require(sceAppInstUtilCheckLatestPatch(static_cast<std::uint32_t>(1), reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilCheckLatestPatch must refuse a null argument 3");
    Require(sceAppInstUtilCancelCheckPatch() == notInitialized, "sceAppInstUtilCancelCheckPatch answered before initialization");
    Require(sceAppInstUtilAppInstallPsNowIcon(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilAppInstallPsNowIcon answered before initialization");
    Require(sceAppInstUtilAppInstallPsNowIcon(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilAppInstallPsNowIcon must refuse a null argument 1");
    Require(sceAppInstUtilAppInstallPsNowIcon(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilAppInstallPsNowIcon must refuse a null argument 2");
    Require(sceAppInstUtilAppInstallContentPush(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilAppInstallContentPush answered before initialization");
    Require(sceAppInstUtilAppInstallContentPush(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilAppInstallContentPush must refuse a null argument 1");
    Require(sceAppInstUtilAppInstallContentPush(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilAppInstallContentPush must refuse a null argument 2");
    Require(sceAppInstUtilSwitchInstallLocation() == notInitialized, "sceAppInstUtilSwitchInstallLocation answered before initialization");
    Require(sceAppInstUtilGetConfigList(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilGetConfigList answered before initialization");
    Require(sceAppInstUtilGetConfigList(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilGetConfigList must refuse a null argument 1");
    Require(sceAppInstUtilGetConfigList(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilGetConfigList must refuse a null argument 2");
    Require(sceAppInstUtilSwitchConfig(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilSwitchConfig answered before initialization");
    Require(sceAppInstUtilSwitchConfig(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilSwitchConfig must refuse a null argument 1");
    Require(sceAppInstUtilSwitchConfig(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilSwitchConfig must refuse a null argument 2");
    Require(sceAppInstUtilRestoreDataSource(reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilRestoreDataSource answered before initialization");
    Require(sceAppInstUtilRestoreDataSource(nullptr) == nullArgument, "sceAppInstUtilRestoreDataSource must refuse a null argument 1");
    Require(sceAppInstUtilGetNeededStorageSize(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilGetNeededStorageSize answered before initialization");
    Require(sceAppInstUtilGetNeededStorageSize(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilGetNeededStorageSize must refuse a null argument 1");
    Require(sceAppInstUtilGetNeededStorageSize(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilGetNeededStorageSize must refuse a null argument 2");
    Require(sceAppInstUtilAppGetStorageDestType(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "sceAppInstUtilAppGetStorageDestType answered before initialization");
    Require(sceAppInstUtilAppGetStorageDestType(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "sceAppInstUtilAppGetStorageDestType must refuse a null argument 1");
    Require(sceAppInstUtilAppGetStorageDestType(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "sceAppInstUtilAppGetStorageDestType must refuse a null argument 2");
    Require(libSceAppInstUtilUnknown_2WQuFX8yYXg_nid_no_patch(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "libSceAppInstUtilUnknown_2WQuFX8yYXg_nid_no_patch answered before initialization");
    Require(libSceAppInstUtilUnknown_2WQuFX8yYXg_nid_no_patch(nullptr, reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == nullArgument, "libSceAppInstUtilUnknown_2WQuFX8yYXg_nid_no_patch must refuse a null argument 1");
    Require(libSceAppInstUtilUnknown_2WQuFX8yYXg_nid_no_patch(reinterpret_cast<const void*>(block), nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "libSceAppInstUtilUnknown_2WQuFX8yYXg_nid_no_patch must refuse a null argument 2");
    Require(libSceAppInstUtilUnknown_2WQuFX8yYXg_nid_no_patch(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block), nullptr) == nullArgument, "libSceAppInstUtilUnknown_2WQuFX8yYXg_nid_no_patch must refuse a null argument 3");
    Require(libSceAppInstUtilUnknown_4vqRxwpIE1M_nid_no_patch(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "libSceAppInstUtilUnknown_4vqRxwpIE1M_nid_no_patch answered before initialization");
    Require(libSceAppInstUtilUnknown_4vqRxwpIE1M_nid_no_patch(nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "libSceAppInstUtilUnknown_4vqRxwpIE1M_nid_no_patch must refuse a null argument 1");
    Require(libSceAppInstUtilUnknown_4vqRxwpIE1M_nid_no_patch(reinterpret_cast<const void*>(block), nullptr) == nullArgument, "libSceAppInstUtilUnknown_4vqRxwpIE1M_nid_no_patch must refuse a null argument 2");
    Require(libSceAppInstUtilUnknown_9QIC1kZpSG4_nid_no_patch() == notInitialized, "libSceAppInstUtilUnknown_9QIC1kZpSG4_nid_no_patch answered before initialization");
    Require(libSceAppInstUtilUnknown_K73u2VX0JFo_nid_no_patch(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block), static_cast<std::uint64_t>(3), reinterpret_cast<const void*>(block)) == notInitialized, "libSceAppInstUtilUnknown_K73u2VX0JFo_nid_no_patch answered before initialization");
    Require(libSceAppInstUtilUnknown_K73u2VX0JFo_nid_no_patch(nullptr, reinterpret_cast<const void*>(block), static_cast<std::uint64_t>(3), reinterpret_cast<const void*>(block)) == nullArgument, "libSceAppInstUtilUnknown_K73u2VX0JFo_nid_no_patch must refuse a null argument 1");
    Require(libSceAppInstUtilUnknown_K73u2VX0JFo_nid_no_patch(reinterpret_cast<const void*>(block), nullptr, static_cast<std::uint64_t>(3), reinterpret_cast<const void*>(block)) == nullArgument, "libSceAppInstUtilUnknown_K73u2VX0JFo_nid_no_patch must refuse a null argument 2");
    Require(libSceAppInstUtilUnknown_K73u2VX0JFo_nid_no_patch(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block), static_cast<std::uint64_t>(3), nullptr) == nullArgument, "libSceAppInstUtilUnknown_K73u2VX0JFo_nid_no_patch must refuse a null argument 4");
    Require(libSceAppInstUtilUnknown_N6H7LMfZD5Q_nid_no_patch() == notInitialized, "libSceAppInstUtilUnknown_N6H7LMfZD5Q_nid_no_patch answered before initialization");
    Require(libSceAppInstUtilUnknown_QMjuGVPtjUY_nid_no_patch(reinterpret_cast<const void*>(block)) == notInitialized, "libSceAppInstUtilUnknown_QMjuGVPtjUY_nid_no_patch answered before initialization");
    Require(libSceAppInstUtilUnknown_QMjuGVPtjUY_nid_no_patch(nullptr) == nullArgument, "libSceAppInstUtilUnknown_QMjuGVPtjUY_nid_no_patch must refuse a null argument 1");
    Require(libSceAppInstUtilUnknown_R_minus_OUApMZ1d8_nid_no_patch(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "libSceAppInstUtilUnknown_R_minus_OUApMZ1d8_nid_no_patch answered before initialization");
    Require(libSceAppInstUtilUnknown_R_minus_OUApMZ1d8_nid_no_patch(nullptr, reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == nullArgument, "libSceAppInstUtilUnknown_R_minus_OUApMZ1d8_nid_no_patch must refuse a null argument 1");
    Require(libSceAppInstUtilUnknown_R_minus_OUApMZ1d8_nid_no_patch(reinterpret_cast<const void*>(block), nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "libSceAppInstUtilUnknown_R_minus_OUApMZ1d8_nid_no_patch must refuse a null argument 2");
    Require(libSceAppInstUtilUnknown_R_minus_OUApMZ1d8_nid_no_patch(reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block), nullptr) == nullArgument, "libSceAppInstUtilUnknown_R_minus_OUApMZ1d8_nid_no_patch must refuse a null argument 3");
    Require(libSceAppInstUtilUnknown_T9h6l8bFKGE_nid_no_patch() == notInitialized, "libSceAppInstUtilUnknown_T9h6l8bFKGE_nid_no_patch answered before initialization");
    Require(libSceAppInstUtilUnknown_XWjOLtdgxa4_nid_no_patch() == notInitialized, "libSceAppInstUtilUnknown_XWjOLtdgxa4_nid_no_patch answered before initialization");
    Require(libSceAppInstUtilUnknown_Y_plus_ln_minus_TkY_plus_hA_nid_no_patch(reinterpret_cast<const void*>(block)) == notInitialized, "libSceAppInstUtilUnknown_Y_plus_ln_minus_TkY_plus_hA_nid_no_patch answered before initialization");
    Require(libSceAppInstUtilUnknown_Y_plus_ln_minus_TkY_plus_hA_nid_no_patch(nullptr) == nullArgument, "libSceAppInstUtilUnknown_Y_plus_ln_minus_TkY_plus_hA_nid_no_patch must refuse a null argument 1");
    Require(libSceAppInstUtilUnknown_Zv_plus_26GDSKcQ_nid_no_patch(reinterpret_cast<const void*>(block), static_cast<std::uint64_t>(2), reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == notInitialized, "libSceAppInstUtilUnknown_Zv_plus_26GDSKcQ_nid_no_patch answered before initialization");
    Require(libSceAppInstUtilUnknown_Zv_plus_26GDSKcQ_nid_no_patch(nullptr, static_cast<std::uint64_t>(2), reinterpret_cast<const void*>(block), reinterpret_cast<const void*>(block)) == nullArgument, "libSceAppInstUtilUnknown_Zv_plus_26GDSKcQ_nid_no_patch must refuse a null argument 1");
    Require(libSceAppInstUtilUnknown_Zv_plus_26GDSKcQ_nid_no_patch(reinterpret_cast<const void*>(block), static_cast<std::uint64_t>(2), nullptr, reinterpret_cast<const void*>(block)) == nullArgument, "libSceAppInstUtilUnknown_Zv_plus_26GDSKcQ_nid_no_patch must refuse a null argument 3");
    Require(libSceAppInstUtilUnknown_Zv_plus_26GDSKcQ_nid_no_patch(reinterpret_cast<const void*>(block), static_cast<std::uint64_t>(2), reinterpret_cast<const void*>(block), nullptr) == nullArgument, "libSceAppInstUtilUnknown_Zv_plus_26GDSKcQ_nid_no_patch must refuse a null argument 4");
    Require(libSceAppInstUtilUnknown_ecad2GAw55Y_nid_no_patch() == notInitialized, "libSceAppInstUtilUnknown_ecad2GAw55Y_nid_no_patch answered before initialization");
    Require(libSceAppInstUtilUnknown_gFlBHjtrMDI_nid_no_patch() == notInitialized, "libSceAppInstUtilUnknown_gFlBHjtrMDI_nid_no_patch answered before initialization");
    Require(libSceAppInstUtilUnknown_j80u95nZ_plus_3A_nid_no_patch() == notInitialized, "libSceAppInstUtilUnknown_j80u95nZ_plus_3A_nid_no_patch answered before initialization");
    Require(libSceAppInstUtilUnknown_qn0aziKw_minus_kE_nid_no_patch(reinterpret_cast<const void*>(block)) == notInitialized, "libSceAppInstUtilUnknown_qn0aziKw_minus_kE_nid_no_patch answered before initialization");
    Require(libSceAppInstUtilUnknown_qn0aziKw_minus_kE_nid_no_patch(nullptr) == nullArgument, "libSceAppInstUtilUnknown_qn0aziKw_minus_kE_nid_no_patch must refuse a null argument 1");
    Require(libSceAppInstUtilUnknown_yYVV_minus_f0zoWY_nid_no_patch(reinterpret_cast<const void*>(block)) == notInitialized, "libSceAppInstUtilUnknown_yYVV_minus_f0zoWY_nid_no_patch answered before initialization");
    Require(libSceAppInstUtilUnknown_yYVV_minus_f0zoWY_nid_no_patch(nullptr) == nullArgument, "libSceAppInstUtilUnknown_yYVV_minus_f0zoWY_nid_no_patch must refuse a null argument 1");
}

int main() {
    CheckServiceChecks();
    constexpr int nullArgument = static_cast<int>(0x80A30003u);
    constexpr int notInitialized = static_cast<int>(0x80A31000u);
    std::int32_t untouched = 7;
    Require(sceAppInstUtilAppExists("PPSA00000", &untouched) == notInitialized && untouched == 7, "a query before initialization was answered");
    Require(sceAppInstUtilAppExists(nullptr, &untouched) == nullArgument && untouched == 7, "a null title id was not refused before the initialization check");
    Require(sceAppInstUtilAppExists("PPSA00000", nullptr) == nullArgument, "a null output was not refused before the initialization check");
    Require(sceAppInstUtilAppExists(reinterpret_cast<const char*>(0x10), &untouched) == notInitialized && untouched == 7, "an unreadable title id was read before the initialization check");
    bool unchanged = true;
    Require(sceAppInstUtilAppIsInUpdating("PPSA00000", &unchanged) == notInitialized && unchanged, "an update query before initialization was answered");
    Require(sceAppInstUtilAppIsInUpdating(nullptr, &unchanged) == nullArgument && unchanged, "a null title id was not refused by the update query");
    Require(sceAppInstUtilAppIsInUpdating("PPSA00000", nullptr) == nullArgument, "a null update flag was not refused");
    Require(sceAppInstUtilAppIsInUpdating(reinterpret_cast<const char*>(0x10), &unchanged) == notInitialized && unchanged, "an unreadable title id was read by the update query before the initialization check");

    Require(sceAppInstUtilTerminate() == 0, "an unbalanced termination failed");
    Require(sceAppInstUtilInitialize() == 0, "initialization after an unbalanced termination failed");
    Require(sceAppInstUtilAppExists("PPSA00000", &untouched) == notInitialized && untouched == 7, "one initialization undid an unbalanced termination");
    Require(sceAppInstUtilInitialize() == 0, "initialization failed");
    Require(sceAppInstUtilInitialize() == 0, "a nested initialization failed");
    Require(sceAppInstUtilTerminate() == 0, "the nested termination failed");
    Require(sceAppInstUtilAppExists(nullptr, &untouched) == nullArgument && untouched == 7, "a null title id was not refused");

    errno = 71;
    Require(sceAppInstUtilAppExists("PPSA00000", reinterpret_cast<std::int32_t*>(0x10)) == nullArgument, "unwritable exists rejected");
    Require(errno == 71, "errno preserved");
    errno = 71;
    Require(sceAppInstUtilAppExists(reinterpret_cast<const char*>(0x10), &untouched) == nullArgument && untouched == 7, "an unreadable title id was not refused");
    Require(errno == 71, "errno was not preserved for an unreadable title id");
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    errno = 71;
    Require(sceAppInstUtilAppExists(reinterpret_cast<const char*>(0x10), &untouched) == nullArgument && untouched == 7, "an unreadable title id was not refused after a host read hook");
    Require(errno == 71, "errno set by a host read hook leaked from an unreadable title id");
    errno = 71;
    Require(sceAppInstUtilAppIsInUpdating(reinterpret_cast<const char*>(0x10), &unchanged) == nullArgument && unchanged, "an unreadable title id was not refused by the update query after a host read hook");
    Require(errno == 71, "errno set by a host read hook leaked from the update query");
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    char* guard = GuardedPageEnd();
    errno = 71;
    Require(sceAppInstUtilAppExists(guard, &untouched) == nullArgument && untouched == 7, "a title id in a guard page was not refused");
    Require(errno == 71, "errno was not preserved for a title id in a guard page");
    std::int32_t exists = 7;
    Require(sceAppInstUtilAppExists(EndingAt(guard, "PPSA000001", 10), &exists) == 0 && exists == 0, "a title id that differs before a guard page was read past the comparison");
    exists = 7;
    Require(sceAppInstUtilAppExists(EndingAt(guard, "PPSA00000", 10), &exists) == 0 && exists == 1, "the running title id ending at a guard page was not found");
    exists = 7;
    Require(sceAppInstUtilAppExists("PPSA000000", &exists) == 0 && exists == 0, "a longer title id matched");
    exists = 0;
    Require(sceAppInstUtilAppExists("PPSA00000", &exists) == 0 && exists == 1, "the running title is not installed");
    exists = 7;
    Require(sceAppInstUtilAppExists("PPSA00001", &exists) == 0 && exists == 0, "another title is installed");
    exists = 7;
    Require(sceAppInstUtilAppExists("PPSA0000", &exists) == 0 && exists == 0, "a title id prefix matched");
    exists = 7;
    Require(sceAppInstUtilAppExists("", &exists) == 0 && exists == 0, "an empty title id matched");

    bool updating = true;
    errno = 71;
    Require(sceAppInstUtilAppIsInUpdating("PPSA00000", reinterpret_cast<bool*>(0x10)) == nullArgument, "an unwritable update flag was not refused");
    Require(errno == 71, "errno was not preserved for an unwritable update flag");
    errno = 71;
    Require(sceAppInstUtilAppIsInUpdating(reinterpret_cast<const char*>(0x10), &updating) == nullArgument && updating, "an unreadable title id was not refused by the update query");
    Require(errno == 71, "errno was not preserved for an unreadable title id in the update query");
    Require(sceAppInstUtilAppIsInUpdating(guard, &updating) == nullArgument && updating, "a title id in a guard page was not refused by the update query");
    Require(sceAppInstUtilAppIsInUpdating(EndingAt(guard, "PPSA000001", 10), &updating) == 0 && !updating, "the update query read past the title id window");
    updating = true;
    Require(sceAppInstUtilAppIsInUpdating("PPSA00000", &updating) == 0 && !updating, "the running title is updating");
    updating = true;
    Require(sceAppInstUtilAppIsInUpdating("PPSA00001", &updating) == 0 && !updating, "another title is updating");

    Require(sceAppInstUtilTerminate() == 0, "termination failed");
    Require(sceAppInstUtilInitialize() == 0, "initialization after termination failed");
    exists = 0;
    Require(sceAppInstUtilAppExists("PPSA00000", &exists) == 0 && exists == 1, "the running title is not installed after a restart");
    Require(sceAppInstUtilTerminate() == 0, "the second termination failed");
    std::puts("AppInstUtil tests passed");
    return 0;
}
