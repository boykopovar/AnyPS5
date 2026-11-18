#include "prx/libSceAppInstUtil/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <string>

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

template <typename Error, typename Call>
bool Throws(Call call) {
    try {
        call();
    } catch (const Error&) {
        return true;
    } catch (const std::exception&) {
        return false;
    }
    return false;
}

struct MetaInfo {
    const char* strings[6];
    std::uint32_t value30;
    std::uint8_t value34;
    std::uint8_t padding[3];
};
static_assert(sizeof(MetaInfo) == 0x38);

constexpr int InstallNullArgument = static_cast<int>(0x80A30003u);

template <typename Error>
bool InstallThrows(const MetaInfo& meta) {
    std::uint8_t pkgInfo[0x38] = {};
    static std::uint8_t playGo[0x2708] = {};
    return Throws<Error>([&] { sceAppInstUtilInstallByPackage(&meta, pkgInfo, playGo); });
}

bool InstallReachesService(const MetaInfo& meta) {
    return InstallThrows<std::runtime_error>(meta);
}

bool InstallFaults(const MetaInfo& meta) {
    return InstallThrows<std::logic_error>(meta);
}

int Install(const MetaInfo& meta) {
    std::uint8_t pkgInfo[0x38] = {};
    static std::uint8_t playGo[0x2708] = {};
    return sceAppInstUtilInstallByPackage(&meta, pkgInfo, playGo);
}

void CheckInstallByPackageBeforeInitialize(int notInitialized) {
    MetaInfo meta{};
    std::uint8_t pkgInfo[0x38] = {};
    std::uint8_t playGo[0x40] = {};
    Require(sceAppInstUtilInstallByPackage(nullptr, pkgInfo, playGo) == InstallNullArgument, "InstallByPackage did not refuse a null MetaInfo first");
    Require(sceAppInstUtilInstallByPackage(&meta, nullptr, playGo) == InstallNullArgument, "InstallByPackage did not refuse a null pkg_info first");
    Require(sceAppInstUtilInstallByPackage(&meta, pkgInfo, nullptr) == InstallNullArgument, "InstallByPackage did not refuse a null PlayGo info first");
    Require(Install(meta) == notInitialized, "InstallByPackage read MetaInfo before the initialization check");
}

void CheckInstallByPackage(char* guard) {
    MetaInfo meta{};
    Require(Install(meta) == InstallNullArgument, "InstallByPackage accepted a null URI");
    meta.strings[0] = "/data/test.pkg";
    Require(InstallReachesService(meta), "InstallByPackage refused a local path with no optional field");

    struct Case {
        std::size_t field;
        std::size_t longest;
    };
    const Case cases[] = {{0, 0x3ffe}, {1, 0x3ffe}, {2, 2}, {3, 0x2f}, {4, 0x7f}, {5, 0x3ffe}};
    for (const Case& limit : cases) {
        MetaInfo bounded{};
        bounded.strings[0] = "/data/test.pkg";
        const std::string fits(limit.longest, 'a');
        const std::string over(limit.longest + 1, 'a');
        bounded.strings[limit.field] = fits.c_str();
        Require(InstallReachesService(bounded), "InstallByPackage refused a MetaInfo string at its limit");
        bounded.strings[limit.field] = over.c_str();
        Require(Install(bounded) == InstallNullArgument, "InstallByPackage accepted a MetaInfo string over its limit");
    }

    meta.strings[0] = EndingAt(guard, "/data/test.pkg", 15);
    Require(InstallReachesService(meta), "InstallByPackage read past a URI that ends before a guard page");
    meta.strings[0] = EndingAt(guard, "/data/test.pkg", 14);
    Require(InstallFaults(meta), "InstallByPackage did not fault on a URI that runs into a guard page");
    meta.strings[0] = reinterpret_cast<const char*>(0x10);
    Require(InstallFaults(meta), "InstallByPackage did not fault on an unreadable URI");
    std::uint8_t pkgInfo[0x38] = {};
    std::uint8_t playGo[0x40] = {};
    Require(Throws<std::logic_error>([&] { sceAppInstUtilInstallByPackage(reinterpret_cast<const void*>(0x10), pkgInfo, playGo); }), "InstallByPackage did not fault on an unreadable MetaInfo");
}

}

int main() {
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
    CheckInstallByPackageBeforeInitialize(notInitialized);

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
    CheckInstallByPackage(guard);

    Require(sceAppInstUtilTerminate() == 0, "termination failed");
    Require(sceAppInstUtilInitialize() == 0, "initialization after termination failed");
    exists = 0;
    Require(sceAppInstUtilAppExists("PPSA00000", &exists) == 0 && exists == 1, "the running title is not installed after a restart");
    Require(sceAppInstUtilTerminate() == 0, "the second termination failed");
    std::puts("AppInstUtil tests passed");
    return 0;
}
