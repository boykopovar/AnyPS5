#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <cstddef>
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

struct HookCalls {
    int allocations = 0;
    int reallocations = 0;
    int releases = 0;
    std::size_t lastSize = 0;
    const void* lastContext = nullptr;
};

HookCalls Calls;

void* APS5_VABI CountingAllocate_nid_no_patch(const void* context, std::size_t size) {
    ++Calls.allocations;
    Calls.lastSize = size;
    Calls.lastContext = context;
    return std::malloc(size);
}

void* APS5_VABI CountingReallocate_nid_no_patch(const void* context, void* block, std::size_t size) {
    ++Calls.reallocations;
    Calls.lastSize = size;
    Calls.lastContext = context;
    return std::realloc(block, size);
}

void APS5_VABI CountingRelease_nid_no_patch(const void* context, void* block) {
    ++Calls.releases;
    Calls.lastContext = context;
    std::free(block);
}

void DefaultAllocation() {
    u_cleanup_nid_postfix();
    void* empty = uprv_malloc_nid_postfix(0);
    Require(empty != nullptr && empty == uprv_malloc_nid_postfix(0));

    auto* bytes = static_cast<char*>(uprv_malloc_nid_postfix(4));
    Require(bytes != nullptr && bytes != empty);
    std::memcpy(bytes, "abc", 4);
    bytes = static_cast<char*>(uprv_realloc_nid_postfix(bytes, 4096));
    Require(bytes != nullptr && std::string(bytes) == "abc");
    Require(uprv_realloc_nid_postfix(bytes, 0) == empty);

    auto* grown = static_cast<char*>(uprv_realloc_nid_postfix(empty, 8));
    Require(grown != nullptr && grown != empty);
    std::memcpy(grown, "growing", 8);
    Require(std::string(grown) == "growing");
    Require(uprv_realloc_nid_postfix(empty, 0) == empty);
    free_nid_postfix(grown);

    char* copy = uprv_strdup_nid_postfix("icu data");
    Require(copy != nullptr && std::string(copy) == "icu data");
    free_nid_postfix(copy);
    copy = uprv_strdup_nid_postfix("");
    Require(copy != nullptr && copy != empty && copy[0] == '\0');
    free_nid_postfix(copy);

    copy = uprv_strndup_nid_postfix("abcdef", 3);
    Require(copy != nullptr && std::string(copy) == "abc");
    free_nid_postfix(copy);
    copy = uprv_strndup_nid_postfix("abcdef", -1);
    Require(copy != nullptr && std::string(copy) == "abcdef");
    free_nid_postfix(copy);
    copy = uprv_strndup_nid_postfix("abcdef", 0);
    Require(copy != nullptr && copy[0] == '\0');
    free_nid_postfix(copy);
    const char embedded[] = {'a', '\0', 'b', 'c'};
    copy = uprv_strndup_nid_postfix(embedded, 4);
    Require(copy != nullptr && std::memcmp(copy, embedded, 4) == 0 && copy[4] == '\0');
    free_nid_postfix(copy);

    RequireThrows([] { uprv_strdup_nid_postfix(nullptr); });
    RequireThrows([] { uprv_strndup_nid_postfix(nullptr, 2); });
    RequireThrows([] { uprv_strndup_nid_postfix(nullptr, -1); });
}

void HookedAllocation() {
    int marker = 0;
    std::int32_t status = 0;
    u_setMemoryFunctions_nid_postfix(&marker, CountingAllocate_nid_no_patch, CountingReallocate_nid_no_patch, CountingRelease_nid_no_patch, &status);
    Require(status == 0);
    Calls = {};

    void* empty = uprv_malloc_nid_postfix(0);
    Require(empty != nullptr && Calls.allocations == 0);
    void* block = uprv_malloc_nid_postfix(24);
    Require(block != nullptr && Calls.allocations == 1 && Calls.lastSize == 24 && Calls.lastContext == &marker);
    block = uprv_realloc_nid_postfix(block, 48);
    Require(block != nullptr && Calls.reallocations == 1 && Calls.lastSize == 48 && Calls.lastContext == &marker);
    Require(uprv_realloc_nid_postfix(block, 0) == empty && Calls.releases == 1 && Calls.lastContext == &marker);
    block = uprv_realloc_nid_postfix(empty, 16);
    Require(block != nullptr && Calls.allocations == 2 && Calls.reallocations == 1 && Calls.lastSize == 16);
    CountingRelease_nid_no_patch(&marker, block);

    char* copy = uprv_strdup_nid_postfix("hook");
    Require(copy != nullptr && std::string(copy) == "hook" && Calls.allocations == 3 && Calls.lastSize == 5);
    std::free(copy);
    copy = uprv_strndup_nid_postfix("hooked", 2);
    Require(copy != nullptr && std::string(copy) == "ho" && Calls.allocations == 4 && Calls.lastSize == 3);
    std::free(copy);

    u_setMemoryFunctions_nid_postfix(&marker, Allocate_nid_no_patch, Reallocate_nid_no_patch, Release_nid_no_patch, &status);
    Require(uprv_malloc_nid_postfix(8) == nullptr);
    Require(uprv_strdup_nid_postfix("lost") == nullptr);
    Require(uprv_strndup_nid_postfix("lost", 2) == nullptr);
    Require(uprv_malloc_nid_postfix(0) == empty);
    u_cleanup_nid_postfix();
}

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    Require(ApplicationHeapDefault_nid_no_patch());
    Locking();
    Memory();
    DataDirectory();
    TimeZoneDirectory();
    Cleanup();
    DefaultAllocation();
    HookedAllocation();
    std::puts("ICU configuration tests passed");
}
