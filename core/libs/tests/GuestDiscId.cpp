#include "prx/libSceDiscId/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

constexpr int ErrorInvalidArgument = static_cast<int>(0x81680016);
constexpr int ErrorDeviceUnavailable = static_cast<int>(0x816800ff);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "DISC ID: %s\n", message);
    std::abort();
}

}

int main() {
    const auto original = std::filesystem::current_path();
    const auto directory = original / ("discid-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(directory), "create the test directory");
    std::filesystem::current_path(directory);

    Require(sceDiscIdGet(nullptr) == ErrorInvalidArgument, "Get null output");
    errno = 71;
    Require(sceDiscIdGet(reinterpret_cast<DiscId*>(0x10)) == ErrorInvalidArgument, "Get unwritable output");
    Require(errno == 71, "Get preserves errno");
    Require(sceDiscIdGet2(nullptr, 0) == ErrorInvalidArgument, "Get2 null output");
    errno = 71;
    Require(sceDiscIdGet2(reinterpret_cast<DiscId2*>(0x10), 0) == ErrorInvalidArgument, "Get2 unwritable output");
    Require(errno == 71, "Get2 preserves errno");
    Require(sceDiscIdGet2(nullptr, 1) == ErrorInvalidArgument, "Get2 null output and nonzero flags");

    std::uint8_t buffer[64];
    std::memset(buffer, 0xAB, sizeof(buffer));
    auto* id = reinterpret_cast<DiscId*>(buffer);
    auto* id2 = reinterpret_cast<DiscId2*>(buffer);
    for (const std::uint32_t flags : {1u, 3u, 0x80000000u, 0xFFFFFFFFu}) {
        Require(sceDiscIdGet2(id2, flags) == ErrorInvalidArgument, "Get2 nonzero flags");
    }
    Require(sceDiscIdGet(id) == ErrorDeviceUnavailable, "Get without /dev/duid");
    Require(sceDiscIdGet2(id2, 0) == ErrorDeviceUnavailable, "Get2 without /dev/duid");
    for (const std::uint8_t byte : buffer) Require(byte == 0xAB, "failed calls write nothing");

    if (GuestArena::GuestArenaAvailable_nid_postfix()) {
        constexpr std::size_t kArenaBytes = 0x10000;
        auto* const page = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(kArenaBytes, kArenaBytes));
        Require(page != nullptr, "allocate write-watched page");
#ifdef _WIN32
        const HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, 0, kArenaBytes, nullptr);
        Require(section != nullptr, "create write-watched section");
        GuestArena::GuestArenaMap_nid_postfix(page, kArenaBytes, section, 0, PAGE_READWRITE);
        CloseHandle(section);
        const bool watchedWrites = GuestArena::GuestArenaWriteWatched_nid_postfix();
#else
        GuestArena::GuestArenaCommit_nid_postfix(page, kArenaBytes, 0x04, kArenaBytes);
        const bool watchedWrites = GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix();
        if (watchedWrites) {
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(page, kArenaBytes);
        }
#endif
        const auto collectDirty = [&]() -> bool {
            if (!watchedWrites) {
                return true;
            }
#ifdef _WIN32
            std::array<void*, 16> pages{};
            std::size_t count = pages.size();
            Require(GuestArena::GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(page), kArenaBytes, pages.data(), &count, true),
                    "collect write-watched writes");
            return count != 0;
#else
            bool dirty = false;
            Require(GuestWriteWatch::GuestWriteWatchCollect_nid_postfix(reinterpret_cast<std::uintptr_t>(page), kArenaBytes,
                [](void* context, std::uintptr_t, std::uintptr_t) { *static_cast<bool*>(context) = true; }, &dirty),
                "collect write-watched writes");
            return dirty;
#endif
        };
        const auto armWatch = [&](void* target) {
            if (!watchedWrites) {
                return;
            }
            static_cast<void>(collectDirty());
            Require(!collectDirty(), "write-watched page was still dirty");
#ifdef _WIN32
            MEMORY_BASIC_INFORMATION mbi{};
            Require(VirtualQuery(target, &mbi, sizeof(mbi)) == sizeof(mbi) && mbi.Protect == PAGE_READONLY, "arm write-watched page");
#else
            static_cast<void>(target);
#endif
        };
        std::memset(page, 0xAB, 64);
        auto* watchedId = reinterpret_cast<DiscId*>(page);
        auto* watchedId2 = reinterpret_cast<DiscId2*>(page);
        armWatch(watchedId);
        Require(sceDiscIdGet(watchedId) == ErrorDeviceUnavailable, "Get on write-watched page without /dev/duid");
        armWatch(watchedId2);
        Require(sceDiscIdGet2(watchedId2, 0) == ErrorDeviceUnavailable, "Get2 on write-watched page without /dev/duid");
        for (std::size_t i = 0; i < 64; ++i) Require(page[i] == 0xAB, "write-watched probes write nothing");

        GuestArena::GuestArenaSetProtection_nid_postfix(reinterpret_cast<std::uintptr_t>(page), kArenaBytes, 0x02);
        errno = 71;
        Require(sceDiscIdGet(watchedId) == ErrorInvalidArgument, "Get read-only output");
        Require(errno == 71, "Get read-only preserves errno");
        errno = 71;
        Require(sceDiscIdGet2(watchedId2, 0) == ErrorInvalidArgument, "Get2 read-only output");
        Require(errno == 71, "Get2 read-only preserves errno");
        for (std::size_t i = 0; i < 64; ++i) Require(page[i] == 0xAB, "read-only probes write nothing");
        GuestArena::GuestArenaSetProtection_nid_postfix(reinterpret_cast<std::uintptr_t>(page), kArenaBytes, 0x04);
#ifndef _WIN32
        if (watchedWrites) {
            GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(page, kArenaBytes);
        }
#endif
        GuestArena::GuestArenaReset_nid_postfix(page, kArenaBytes);
        GuestArena::GuestArenaRelease_nid_postfix(page, kArenaBytes);
    }

    std::filesystem::current_path(original);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    std::puts("DiscId tests passed");
    return 0;
}
