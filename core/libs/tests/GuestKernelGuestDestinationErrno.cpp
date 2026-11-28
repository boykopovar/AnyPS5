#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "guest kernel destination errno check failed at %s:%u\n",
            location.file_name(), static_cast<unsigned>(location.line()));
        std::abort();
    }
}

std::size_t HostPageSize() {
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    return info.dwPageSize;
#else
    return static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
#endif
}

void* AllocateGuardPages(std::size_t count) {
    const std::size_t page = HostPageSize();
#ifdef _WIN32
    void* pages = VirtualAlloc(nullptr, count * page, MEM_RESERVE, PAGE_NOACCESS);
    Require(pages != nullptr);
    return pages;
#else
    void* pages = mmap(nullptr, count * page, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(pages != MAP_FAILED);
    return pages;
#endif
}

void FreeGuardPages(void* pages, std::size_t count) {
#ifdef _WIN32
    static_cast<void>(count);
    Require(VirtualFree(pages, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(pages, count * HostPageSize()) == 0);
#endif
}

void CheckBatchMap2DestinationAndErrno() {
    void* unwritable = AllocateGuardPages(1);
    KernelBatchMapEntry entry{};
    entry.operation = 3;
    entry.length = 0x4000;
    entry.protection = 3;
    entry.start = nullptr;
    errno = 71;
    const int status = sceKernelBatchMap2(&entry, 1, static_cast<int*>(unwritable), 0);
    Require(status == SCE_KERNEL_ERROR_EFAULT);
    Require(errno == 71);
    Require(entry.start == nullptr);
    FreeGuardPages(unwritable, 1);
}

void CheckStopUnloadModuleDestinationAndErrno() {
    void* unwritable = AllocateGuardPages(1);
    errno = 71;
    const int status = sceKernelStopUnloadModule(0x12345678, 0, nullptr, 0, nullptr, static_cast<int*>(unwritable));
    Require(status == SCE_KERNEL_ERROR_EFAULT);
    Require(errno == 71);
    FreeGuardPages(unwritable, 1);
}

void CheckWriteWatchedDestination() {
    if (!GuestArena::GuestArenaAvailable_nid_postfix()) return;
    constexpr std::size_t kBytes = 0x10000;
    auto* const watched = static_cast<unsigned char*>(GuestArena::GuestArenaAllocate_nid_postfix(kBytes, kBytes));
    Require(watched != nullptr);
#ifdef _WIN32
    const HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, 0, kBytes, nullptr);
    Require(section != nullptr);
    GuestArena::GuestArenaMap_nid_postfix(watched, kBytes, section, 0, PAGE_READWRITE);
    CloseHandle(section);
    const bool watchActive = GuestArena::GuestArenaWriteWatched_nid_postfix();
#else
    GuestArena::GuestArenaCommit_nid_postfix(watched, kBytes, 0x04, kBytes);
    const bool watchActive = GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix();
    if (watchActive) {
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(watched, kBytes);
    }
#endif
    const auto collectDirty = [&]() -> bool {
        if (!watchActive) return true;
#ifdef _WIN32
        std::array<void*, 16> pages{};
        std::size_t count = pages.size();
        Require(GuestArena::GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(watched), kBytes, pages.data(), &count, true));
        return count != 0;
#else
        bool dirty = false;
        Require(GuestWriteWatch::GuestWriteWatchCollect_nid_postfix(reinterpret_cast<std::uintptr_t>(watched), kBytes,
            [](void* context, std::uintptr_t, std::uintptr_t) { *static_cast<bool*>(context) = true; }, &dirty));
        return dirty;
#endif
    };
    const auto armWatch = [&](void* target) {
        if (!watchActive) return;
        static_cast<void>(collectDirty());
        Require(!collectDirty());
#ifdef _WIN32
        MEMORY_BASIC_INFORMATION mbi{};
        Require(VirtualQuery(target, &mbi, sizeof(mbi)) == sizeof(mbi) && mbi.Protect == PAGE_READONLY);
#else
        static_cast<void>(target);
#endif
    };

    KernelBatchMapEntry dummyEntry{};
    int* watchedEntriesOut = reinterpret_cast<int*>(watched);
    *watchedEntriesOut = -1;
    armWatch(watchedEntriesOut);
    errno = 71;
    Require(sceKernelBatchMap2(&dummyEntry, 0, watchedEntriesOut, 0) == 0);
    Require(*watchedEntriesOut == 0);
    Require(errno == 71);
    Require(collectDirty());

    int* watchedRes = reinterpret_cast<int*>(watched);
    armWatch(watchedRes);
    errno = 71;
    Require(sceKernelStopUnloadModule(0x12345678, 0, nullptr, 0, nullptr, watchedRes) == SCE_KERNEL_ERROR_ESRCH);
    Require(errno == 71);

    GuestArena::GuestArenaSetProtection_nid_postfix(reinterpret_cast<std::uintptr_t>(watched), kBytes, 0x02);
    KernelBatchMapEntry mapEntry{};
    mapEntry.operation = 3;
    mapEntry.length = 0x4000;
    mapEntry.protection = 3;
    mapEntry.start = nullptr;
    errno = 71;
    Require(sceKernelBatchMap2(&mapEntry, 1, watchedEntriesOut, 0) == SCE_KERNEL_ERROR_EFAULT);
    Require(errno == 71);
    Require(mapEntry.start == nullptr);
    errno = 71;
    Require(sceKernelStopUnloadModule(0x12345678, 0, nullptr, 0, nullptr, watchedRes) == SCE_KERNEL_ERROR_EFAULT);
    Require(errno == 71);
    GuestArena::GuestArenaSetProtection_nid_postfix(reinterpret_cast<std::uintptr_t>(watched), kBytes, 0x04);

#ifndef _WIN32
    if (watchActive) {
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(watched, kBytes);
    }
#endif
    GuestArena::GuestArenaReset_nid_postfix(watched, kBytes);
    GuestArena::GuestArenaRelease_nid_postfix(watched, kBytes);
}

}

int main() {
    CheckBatchMap2DestinationAndErrno();
    CheckStopUnloadModuleDestinationAndErrno();
    CheckWriteWatchedDestination();
    return 0;
}
