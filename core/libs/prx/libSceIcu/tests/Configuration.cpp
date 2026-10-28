#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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

}

int main() {
    Locking();
    Memory();
    DataDirectory();
    TimeZoneDirectory();
    std::puts("ICU configuration tests passed");
}
