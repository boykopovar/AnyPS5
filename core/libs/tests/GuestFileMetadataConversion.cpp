#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace {

constexpr std::int64_t FixtureSize = 0x100000035;
constexpr timespec FixtureTimes[2]{{1000000000, 123456789}, {1000000001, 987654321}};

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool CheckValue(const char* api, const char* field, std::int64_t actual, std::int64_t expected) {
    if (actual == expected) {
        return true;
    }
    std::fprintf(stderr, "%s %s: expected %lld, actual %lld\n", api, field,
        static_cast<long long>(expected), static_cast<long long>(actual));
    return false;
}

template<typename TTimespec>
bool CheckTime(const char* api, const char* field, const TTimespec& actual, const timespec& expected) {
    bool matches = true;
    if (actual.tv_sec != expected.tv_sec) {
        std::fprintf(stderr, "%s %s.tv_sec: expected %lld, actual %lld\n", api, field,
            static_cast<long long>(expected.tv_sec), static_cast<long long>(actual.tv_sec));
        matches = false;
    }
    if (actual.tv_nsec != expected.tv_nsec) {
        std::fprintf(stderr, "%s %s.tv_nsec: expected %lld, actual %lld\n", api, field,
            static_cast<long long>(expected.tv_nsec), static_cast<long long>(actual.tv_nsec));
        matches = false;
    }
    return matches;
}

struct Fixture {
    char path[40] = "anyps5-file-metadata-XXXXXX";
    int nativeFd = -1;
    int guestFd = -1;
    bool exists = false;

    bool Cleanup() {
        bool success = true;
        if (guestFd >= 0) {
            const int descriptor = guestFd;
            guestFd = -1;
            success &= CheckValue("cleanup", "guest close", close_nid_postfix(descriptor), 0);
        }
        if (nativeFd >= 0) {
            const int descriptor = nativeFd;
            nativeFd = -1;
            success &= CheckValue("cleanup", "native close", ::close(descriptor), 0);
        }
        if (exists) {
            exists = false;
            success &= CheckValue("cleanup", "unlink", ::unlink(path), 0);
        }
        return success;
    }

    ~Fixture() {
        Cleanup();
    }
};

bool CheckMetadata(const char* api, std::int64_t result, const FileStat& actual, const struct stat& expected) {
    if (!CheckValue(api, "result", result, 0)) {
        return false;
    }
    bool matches = CheckValue(api, "st_size", actual.st_size, FixtureSize);
    matches &= CheckValue(api, "st_mode type", actual.st_mode & 0170000, 0100000);
    matches &= CheckTime(api, "st_atim", actual.st_atim, expected.st_atim);
    matches &= CheckTime(api, "st_mtim", actual.st_mtim, expected.st_mtim);
    matches &= CheckTime(api, "st_ctim", actual.st_ctim, expected.st_ctim);
    return matches;
}

void CheckFileMetadataConversion() {
    Fixture fixture;
    fixture.nativeFd = ::mkstemp(fixture.path);
    fixture.exists = fixture.nativeFd >= 0;
    Require(fixture.exists, "native mkstemp failed");
    Require(::ftruncate(fixture.nativeFd, FixtureSize) == 0, "native sparse ftruncate failed");
    fixture.guestFd = open_nid_postfix(fixture.path, 0, 0);
    Require(fixture.guestFd >= 0, "guest read-only open failed");
    Require(::futimens(fixture.nativeFd, FixtureTimes) == 0, "native futimens failed");

    struct stat before{};
    Require(::fstat(fixture.nativeFd, &before) == 0, "native initial fstat failed");
    bool validFixture = CheckValue("native fixture", "st_size", before.st_size, FixtureSize);
    validFixture &= CheckValue("native fixture", "st_mode type", before.st_mode & S_IFMT, S_IFREG);
    validFixture &= CheckTime("native fixture", "st_atim", before.st_atim, FixtureTimes[0]);
    validFixture &= CheckTime("native fixture", "st_mtim", before.st_mtim, FixtureTimes[1]);
    Require(validFixture, "native filesystem does not retain the exact fixture metadata");

    FileStat byPath{}, byKernelPath{}, byFd{}, byAlias{}, byKernelFd{};
    bool matches = CheckMetadata("stat", stat_nid_postfix(fixture.path, &byPath), byPath, before);
    matches &= CheckMetadata("sceKernelStat", sceKernelStat(fixture.path, &byKernelPath), byKernelPath, before);
    matches &= CheckMetadata("fstat", fstat_nid_disambig1_nid_postfix(fixture.guestFd, &byFd), byFd, before);
    matches &= CheckMetadata("_fstat", _fstat_nid_postfix(fixture.guestFd, &byAlias), byAlias, before);
    matches &= CheckMetadata("sceKernelFstat", sceKernelFstat(fixture.guestFd, &byKernelFd), byKernelFd, before);

    struct stat after{};
    Require(::fstat(fixture.nativeFd, &after) == 0, "native final fstat failed");
    matches &= CheckValue("native stability", "st_size", after.st_size, before.st_size);
    matches &= CheckValue("native stability", "st_mode type", after.st_mode & S_IFMT, before.st_mode & S_IFMT);
    matches &= CheckTime("native stability", "st_atim", after.st_atim, before.st_atim);
    matches &= CheckTime("native stability", "st_mtim", after.st_mtim, before.st_mtim);
    matches &= CheckTime("native stability", "st_ctim", after.st_ctim, before.st_ctim);
    Require(matches, "guest metadata conversion or native stability check failed");
    Require(fixture.Cleanup(), "fixture cleanup failed");
}

} // namespace

int main() {
    try {
        CheckFileMetadataConversion();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    std::puts("GuestFileMetadataConversion tests passed");
    return 0;
}
