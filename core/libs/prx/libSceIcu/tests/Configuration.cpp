#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <source_location>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr std::int32_t Unsupported = 16;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU configuration check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false);
}

void Locking() {
    int marker = 0;
    for (std::int32_t incoming : {0, -128, -124, -1}) {
        auto status = incoming;
        u_setMutexFunctions_nid_postfix(&marker, nullptr, nullptr, nullptr, nullptr, &status);
        Require(status == Unsupported);
        status = incoming;
        u_setMutexFunctions_nid_postfix(&marker, &marker, &marker, &marker, &marker, &status);
        Require(status == Unsupported);
        status = incoming;
        u_setAtomicIncDecFunctions_nid_postfix(&marker, nullptr, nullptr, &status);
        Require(status == Unsupported);
        status = incoming;
        u_setAtomicIncDecFunctions_nid_postfix(nullptr, &marker, &marker, &status);
        Require(status == Unsupported);
    }
    for (std::int32_t incoming : {1, 7, 10, 15, 16, 27}) {
        auto status = incoming;
        u_setMutexFunctions_nid_postfix(&marker, &marker, &marker, &marker, &marker, &status);
        Require(status == incoming);
        u_setAtomicIncDecFunctions_nid_postfix(&marker, &marker, &marker, &status);
        Require(status == incoming);
    }
    RequireThrows([&] { u_setMutexFunctions_nid_postfix(&marker, nullptr, nullptr, nullptr, nullptr, nullptr); });
    RequireThrows([&] { u_setAtomicIncDecFunctions_nid_postfix(&marker, nullptr, nullptr, nullptr); });
}

void* APS5_VABI Allocate_nid_no_patch(const void*, std::size_t) { return nullptr; }
void* APS5_VABI Reallocate_nid_no_patch(const void*, void*, std::size_t) { return nullptr; }
void APS5_VABI Release_nid_no_patch(const void*, void*) {}

void Memory() {
    int marker = 0;
    for (std::int32_t incoming : {0, -124, -1}) {
        auto status = incoming;
        u_setMemoryFunctions_nid_postfix(&marker, Allocate_nid_no_patch, Reallocate_nid_no_patch, Release_nid_no_patch, &status);
        Require(status == incoming);
        status = incoming;
        u_setMemoryFunctions_nid_postfix(nullptr, Allocate_nid_no_patch, Reallocate_nid_no_patch, Release_nid_no_patch, &status);
        Require(status == incoming);
    }
    for (int missing = 0; missing < 8; ++missing) {
        std::int32_t status = 0;
        u_setMemoryFunctions_nid_postfix(&marker, missing & 1 ? nullptr : Allocate_nid_no_patch, missing & 2 ? nullptr : Reallocate_nid_no_patch,
                                         missing & 4 ? nullptr : Release_nid_no_patch, &status);
        Require(status == (missing ? 1 : 0));
    }
    std::int32_t status = -124;
    u_setMemoryFunctions_nid_postfix(&marker, nullptr, nullptr, nullptr, &status);
    Require(status == 1);
    for (std::int32_t incoming : {1, 7, 16}) {
        status = incoming;
        u_setMemoryFunctions_nid_postfix(&marker, nullptr, nullptr, nullptr, &status);
        Require(status == incoming);
    }
    RequireThrows([&] { u_setMemoryFunctions_nid_postfix(&marker, Allocate_nid_no_patch, Reallocate_nid_no_patch, Release_nid_no_patch, nullptr); });

    std::vector<std::thread> threads;
    std::vector<std::int32_t> results(8, 99);
    for (std::size_t index = 0; index < results.size(); ++index) {
        threads.emplace_back([&, index] {
            std::int32_t local = 0;
            for (int round = 0; round < 1000; ++round) {
                local = 0;
                u_setMemoryFunctions_nid_postfix(&marker, Allocate_nid_no_patch, Reallocate_nid_no_patch, Release_nid_no_patch, &local);
                if (local != 0) break;
            }
            results[index] = local;
        });
    }
    for (auto& thread : threads) thread.join();
    for (auto value : results) Require(value == 0);
}

void DataDirectory() {
    Require(std::string(u_getDataDirectory_nid_postfix()) == "");
    u_setDataDirectory_nid_postfix("/app0/icu");
    Require(std::string(u_getDataDirectory_nid_postfix()) == "/app0/icu");
    u_setDataDirectory_nid_postfix(nullptr);
    Require(std::string(u_getDataDirectory_nid_postfix()) == "");
    u_setDataDirectory_nid_postfix("/data");
    u_setDataDirectory_nid_postfix("");
    Require(std::string(u_getDataDirectory_nid_postfix()) == "");

    char buffer[] = "/first/path";
    u_setDataDirectory_nid_postfix(buffer);
    buffer[1] = 'X';
    Require(std::string(u_getDataDirectory_nid_postfix()) == "/first/path");
    Require(u_getDataDirectory_nid_postfix() == u_getDataDirectory_nid_postfix());
    u_setDataDirectory_nid_postfix("C:\\icu\\data/mixed");
    Require(std::string(u_getDataDirectory_nid_postfix()) == "C:\\icu\\data/mixed");
    u_setDataDirectory_nid_postfix(std::string(5000, 'd').c_str());
    Require(std::string(u_getDataDirectory_nid_postfix()) == std::string(5000, 'd'));

    std::vector<std::thread> threads;
    for (int index = 0; index < 8; ++index) {
        threads.emplace_back([index] {
            const std::string value = "/thread/" + std::to_string(index);
            for (int round = 0; round < 500; ++round) u_setDataDirectory_nid_postfix(value.c_str());
        });
    }
    for (auto& thread : threads) thread.join();
    const std::string final = u_getDataDirectory_nid_postfix();
    Require(final.size() == 9 && final.starts_with("/thread/") && final[8] >= '0' && final[8] < '8');
    u_setDataDirectory_nid_postfix(nullptr);
}

void TimeZoneDirectory() {
    std::int32_t status = 0;
    Require(std::string(u_getTimeZoneFilesDirectory_nid_postfix(&status)) == "" && status == 0);
    u_setTimeZoneFilesDirectory_nid_postfix("/app0/tz", &status);
    Require(status == 0 && std::string(u_getTimeZoneFilesDirectory_nid_postfix(&status)) == "/app0/tz" && status == 0);

    status = 7;
    Require(std::string(u_getTimeZoneFilesDirectory_nid_postfix(&status)) == "" && status == 7);
    u_setTimeZoneFilesDirectory_nid_postfix("/other", &status);
    Require(status == 7);
    status = 0;
    Require(std::string(u_getTimeZoneFilesDirectory_nid_postfix(&status)) == "/app0/tz");

    status = -124;
    Require(std::string(u_getTimeZoneFilesDirectory_nid_postfix(&status)) == "/app0/tz" && status == -124);
    u_setTimeZoneFilesDirectory_nid_postfix("/warn", &status);
    Require(status == -124);
    status = 0;
    Require(std::string(u_getTimeZoneFilesDirectory_nid_postfix(&status)) == "/warn");

    u_setTimeZoneFilesDirectory_nid_postfix(nullptr, &status);
    Require(status == 1);
    status = 0;
    Require(std::string(u_getTimeZoneFilesDirectory_nid_postfix(&status)) == "");

    u_setTimeZoneFilesDirectory_nid_postfix("/first", &status);
    u_setTimeZoneFilesDirectory_nid_postfix("", &status);
    Require(status == 0 && std::string(u_getTimeZoneFilesDirectory_nid_postfix(&status)) == "");

    char buffer[] = "/copy/me";
    u_setTimeZoneFilesDirectory_nid_postfix(buffer, &status);
    buffer[1] = 'X';
    Require(std::string(u_getTimeZoneFilesDirectory_nid_postfix(&status)) == "/copy/me");
    u_setTimeZoneFilesDirectory_nid_postfix("C:\\tz/mixed", &status);
    Require(std::string(u_getTimeZoneFilesDirectory_nid_postfix(&status)) == "C:\\tz/mixed");
    Require(std::string(u_getDataDirectory_nid_postfix()) == "");
    u_setDataDirectory_nid_postfix("/data");
    Require(std::string(u_getTimeZoneFilesDirectory_nid_postfix(&status)) == "C:\\tz/mixed");
    u_setDataDirectory_nid_postfix(nullptr);

    std::vector<std::thread> threads;
    for (int index = 0; index < 8; ++index) {
        threads.emplace_back([index] {
            const std::string value = "/zone/" + std::to_string(index);
            std::int32_t local = 0;
            for (int round = 0; round < 500; ++round) u_setTimeZoneFilesDirectory_nid_postfix(value.c_str(), &local);
            Require(local == 0);
        });
    }
    for (auto& thread : threads) thread.join();
    const std::string final = u_getTimeZoneFilesDirectory_nid_postfix(&status);
    Require(final.size() == 7 && final.starts_with("/zone/") && final[6] >= '0' && final[6] < '8');
    status = 0;
    u_setTimeZoneFilesDirectory_nid_postfix(nullptr, &status);

    RequireThrows([&] { u_getTimeZoneFilesDirectory_nid_postfix(nullptr); });
    RequireThrows([&] { u_setTimeZoneFilesDirectory_nid_postfix("/x", nullptr); });
}

void Cleanup() {
    std::int32_t status = 0;
    u_cleanup_nid_postfix();
    Require(std::string(u_getDataDirectory_nid_postfix()) == "");
    Require(std::string(u_getTimeZoneFilesDirectory_nid_postfix(&status)) == "" && status == 0);

    u_setDataDirectory_nid_postfix("/app0/icu");
    u_setTimeZoneFilesDirectory_nid_postfix("/app0/tz", &status);
    int marker = 0;
    u_setMemoryFunctions_nid_postfix(&marker, Allocate_nid_no_patch, Reallocate_nid_no_patch, Release_nid_no_patch, &status);
    Require(status == 0 && std::string(u_getDataDirectory_nid_postfix()) == "/app0/icu");
    u_cleanup_nid_postfix();
    Require(std::string(u_getDataDirectory_nid_postfix()) == "");
    Require(std::string(u_getTimeZoneFilesDirectory_nid_postfix(&status)) == "" && status == 0);
    u_cleanup_nid_postfix();
    u_cleanup_nid_postfix();
    Require(std::string(u_getDataDirectory_nid_postfix()) == "");

    u_setDataDirectory_nid_postfix("/again");
    u_setTimeZoneFilesDirectory_nid_postfix("/again/tz", &status);
    u_setMemoryFunctions_nid_postfix(nullptr, Allocate_nid_no_patch, Reallocate_nid_no_patch, Release_nid_no_patch, &status);
    Require(status == 0);
    Require(std::string(u_getDataDirectory_nid_postfix()) == "/again");
    Require(std::string(u_getTimeZoneFilesDirectory_nid_postfix(&status)) == "/again/tz");
    u_cleanup_nid_postfix();

    std::vector<std::thread> threads;
    for (int index = 0; index < 4; ++index) {
        threads.emplace_back([index] {
            std::int32_t local = 0;
            for (int round = 0; round < 300; ++round) {
                if (index % 2) u_cleanup_nid_postfix();
                else u_setDataDirectory_nid_postfix("/busy");
            }
            u_getTimeZoneFilesDirectory_nid_postfix(&local);
        });
    }
    for (auto& thread : threads) thread.join();
    u_cleanup_nid_postfix();
    Require(std::string(u_getDataDirectory_nid_postfix()) == "");
}

}

struct AllocatorLog {
    int allocations = 0;
    int reallocations = 0;
    int releases = 0;
    const void* context = nullptr;
    void* lastRelease = nullptr;
    std::size_t lastSize = 0;
};

AllocatorLog g_log;
alignas(16) unsigned char g_arena[256];

void* APS5_VABI LoggingAllocate(const void* context, std::size_t size) {
    ++g_log.allocations;
    g_log.context = context;
    g_log.lastSize = size;
    return g_arena;
}

void* APS5_VABI LoggingReallocate(const void* context, void* buffer, std::size_t size) {
    ++g_log.reallocations;
    g_log.context = context;
    g_log.lastSize = size;
    return buffer;
}

void APS5_VABI LoggingRelease(const void* context, void* buffer) {
    ++g_log.releases;
    g_log.context = context;
    g_log.lastRelease = buffer;
}

void UprvMemoryWithUserFunctions() {
    int marker = 0;
    std::int32_t status = 0;
    u_cleanup_nid_postfix();
    u_setMemoryFunctions_nid_postfix(&marker, LoggingAllocate, LoggingReallocate, LoggingRelease, &status);
    Require(status == 0);
    g_log = {};

    void* block = uprv_malloc_nid_postfix(40);
    Require(block == g_arena && g_log.allocations == 1 && g_log.context == &marker && g_log.lastSize == 40);

    void* zero = uprv_malloc_nid_postfix(0);
    Require(zero != nullptr && zero != g_arena && g_log.allocations == 1);

    Require(uprv_realloc_nid_postfix(block, 80) == block && g_log.reallocations == 1 && g_log.lastSize == 80);
    Require(g_log.releases == 0);

    Require(uprv_realloc_nid_postfix(block, 0) == zero && g_log.releases == 1 && g_log.lastRelease == block);

    Require(uprv_realloc_nid_postfix(zero, 24) == g_arena && g_log.allocations == 2 && g_log.lastSize == 24 && g_log.reallocations == 1);
    Require(uprv_realloc_nid_postfix(zero, 0) == zero && g_log.allocations == 2 && g_log.releases == 1);

    std::memset(g_arena, 0x55, sizeof(g_arena));
    char* copy = uprv_strdup_nid_postfix("icu");
    Require(copy == reinterpret_cast<char*>(g_arena) && std::memcmp(copy, "icu", 4) == 0 && g_log.lastSize == 4);

    std::memset(g_arena, 0x55, sizeof(g_arena));
    char* prefix = uprv_strndup_nid_postfix("abcdef", 3);
    Require(prefix == reinterpret_cast<char*>(g_arena) && std::memcmp(prefix, "abc", 4) == 0 && g_log.lastSize == 4);

    std::memset(g_arena, 0x55, sizeof(g_arena));
    char* whole = uprv_strndup_nid_postfix("abcdef", -1);
    Require(std::memcmp(whole, "abcdef", 7) == 0 && g_log.lastSize == 7);

    std::memset(g_arena, 0x55, sizeof(g_arena));
    char* longer = uprv_strndup_nid_postfix("ab", 2);
    Require(std::memcmp(longer, "ab", 3) == 0 && g_log.lastSize == 3);

    std::memset(g_arena, 0x55, sizeof(g_arena));
    char* empty = uprv_strndup_nid_postfix("abc", 0);
    Require(empty[0] == 0 && g_log.lastSize == 1);
    u_cleanup_nid_postfix();
}

void UprvMemoryWithDefaultHeap() {
    std::array<void*, 10> api{};
    ApplicationHeapRegister_nid_no_patch(api.data());
    u_cleanup_nid_postfix();

    auto* block = static_cast<unsigned char*>(uprv_malloc_nid_postfix(16));
    Require(block != nullptr);
    std::memset(block, 0xA5, 16);
    block = static_cast<unsigned char*>(uprv_realloc_nid_postfix(block, 4096));
    Require(block != nullptr && block[0] == 0xA5 && block[15] == 0xA5);
    void* zero = uprv_realloc_nid_postfix(block, 0);
    Require(zero != nullptr && zero == uprv_malloc_nid_postfix(0));

    char* copy = uprv_strdup_nid_postfix("default heap");
    Require(copy != nullptr && std::string(copy) == "default heap");
    free_nid_postfix(copy);
    char* prefix = uprv_strndup_nid_postfix("default heap", 7);
    Require(prefix != nullptr && std::string(prefix) == "default");
    free_nid_postfix(prefix);
    char* whole = uprv_strndup_nid_postfix("heap", -5);
    Require(whole != nullptr && std::string(whole) == "heap");
    free_nid_postfix(whole);
}

int main() {
    Locking();
    Memory();
    UprvMemoryWithUserFunctions();
    UprvMemoryWithDefaultHeap();
    DataDirectory();
    TimeZoneDirectory();
    Cleanup();
    std::puts("ICU configuration tests passed");
}
