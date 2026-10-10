#include "SceTypes.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#endif

extern "C" {
int APS5_VABI utimes_nid_postfix(const char*, const KernelTimeval*);
int APS5_VABI futimes_nid_postfix(int, const KernelTimeval*);
int APS5_VABI sceKernelUtimes_nid_postfix(const char*, const KernelTimeval*);
int APS5_VABI stat_nid_postfix(const char*, FileStat*);
int APS5_VABI sceKernelFstat(int, FileStat*);
std::int64_t APS5_VABI fstat_nid_disambig1_nid_postfix(int, FileStat*);
int APS5_VABI open_nid_postfix(const char*, int, int);
int APS5_VABI close_nid_postfix(int);
int APS5_VABI pipe_nid_postfix(int*);
int* APS5_VABI __error_nid_postfix();
}

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s (guest errno %d)\n", message, *__error_nid_postfix());
        std::exit(1);
    }
}

static bool Same(const KernelTimespec& actual, const KernelTimeval& expected) {
    return actual.tv_sec == expected.tv_sec && actual.tv_nsec == expected.tv_usec * 1000;
}

static void CheckTimes(const std::string& path, int fd, const KernelTimeval* expected) {
    FileStat byPath{}, byFd{}, byKernel{};
    Require(stat_nid_postfix(path.c_str(), &byPath) == 0, "stat");
    Require(fstat_nid_disambig1_nid_postfix(fd, &byFd) == 0, "fstat");
    Require(sceKernelFstat(fd, &byKernel) == 0, "sceKernelFstat");
    for (const auto* status : {&byPath, &byFd, &byKernel}) {
        Require(Same(status->st_atim, expected[0]), "access time preserves microseconds");
        Require(Same(status->st_mtim, expected[1]), "modification time preserves microseconds");
    }
#ifdef _WIN32
    FILETIME access{}, modified{};
    Require(GetFileTime(reinterpret_cast<HANDLE>(_get_osfhandle(fd)), nullptr, &access, &modified), "native GetFileTime");
    const auto ticks = [](FILETIME time) {
        return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
    };
    Require(ticks(access) == static_cast<std::uint64_t>(expected[0].tv_sec + 11644473600LL) * 10000000 + expected[0].tv_usec * 10,
        "native access time preserves microseconds");
    Require(ticks(modified) == static_cast<std::uint64_t>(expected[1].tv_sec + 11644473600LL) * 10000000 + expected[1].tv_usec * 10,
        "native modification time preserves microseconds");
#endif
}

int main() {
    const auto root = std::filesystem::path("anyps5-file-times-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root), "create fixture directory");
    const auto path = (root / "file.txt").string();
    { std::ofstream file(path); file << "timestamp fixture"; }
    const int fd = open_nid_postfix(path.c_str(), 0, 0);
    Require(fd >= 0, "open read-only descriptor");

    const KernelTimeval first[2]{{1000000000, 123456}, {1000000001, 500000}};
    Require(utimes_nid_postfix(path.c_str(), first) == 0, "utimes fractional timestamp");
    CheckTimes(path, fd, first);
    const KernelTimeval second[2]{{1000000002, 1}, {1000000003, 999999}};
    Require(futimes_nid_postfix(fd, second) == 0, "futimes on read-only descriptor");
    CheckTimes(path, fd, second);
    Require(sceKernelUtimes_nid_postfix(path.c_str(), first) == 0, "sceKernelUtimes fractional timestamp");
    CheckTimes(path, fd, first);

    for (int index = 0; index < 2; ++index) {
        for (const auto micros : {-1LL, 1000000LL}) {
            KernelTimeval invalid[2]{first[0], first[1]};
            invalid[index].tv_usec = micros;
            Require(utimes_nid_postfix(path.c_str(), invalid) == -1 && *__error_nid_postfix() == 22, "utimes rejects invalid microseconds");
            Require(futimes_nid_postfix(fd, invalid) == -1 && *__error_nid_postfix() == 22, "futimes rejects invalid microseconds");
            Require(sceKernelUtimes_nid_postfix(path.c_str(), invalid) == static_cast<int>(0x80020016u), "sceKernelUtimes rejects invalid microseconds");
            CheckTimes(path, fd, first);
        }
    }

#ifdef _WIN32
    const auto handle = CreateFileW(std::filesystem::path(path).c_str(), FILE_WRITE_ATTRIBUTES | FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    Require(handle != INVALID_HANDLE_VALUE, "open native timestamp handle");
    constexpr std::uint64_t ticks = 116444736000000000ULL + 1000000000ULL * 10000000 + 1234567;
    const FILETIME precise{static_cast<DWORD>(ticks), static_cast<DWORD>(ticks >> 32)};
    Require(SetFileTime(handle, &precise, &precise, &precise), "set native 100-nanosecond timestamps");
    FILE_BASIC_INFO native{};
    Require(GetFileInformationByHandleEx(handle, FileBasicInfo, &native, sizeof(native)), "read native basic info");
    FileStat status{};
    Require(stat_nid_postfix(path.c_str(), &status) == 0, "stat native timestamps");
    Require(status.st_birthtim.tv_sec == 1000000000 && status.st_birthtim.tv_nsec == 123456700, "creation time retains native precision");
    Require(status.st_atim.tv_sec == 1000000000 && status.st_atim.tv_nsec == 123456700, "access time retains native precision");
    Require(status.st_mtim.tv_sec == 1000000000 && status.st_mtim.tv_nsec == 123456700, "modification time retains native precision");
    Require(status.st_ctim.tv_sec == native.ChangeTime.QuadPart / 10000000 - 11644473600LL &&
        status.st_ctim.tv_nsec == native.ChangeTime.QuadPart % 10000000 * 100, "change time uses native metadata change time");
    Require(CloseHandle(handle), "close native timestamp handle");

    const KernelTimeval preEpoch[2]{{-1, 500000}, {0, 1}};
    Require(utimes_nid_postfix(path.c_str(), preEpoch) == 0, "timestamps around Unix epoch");
    CheckTimes(path, fd, preEpoch);
    Require(utimes_nid_postfix(path.c_str(), first) == 0, "restore valid times");
    for (const auto seconds : {std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max(), -11644473601LL, -11644473600LL}) {
        const KernelTimeval invalid[2]{first[0], {seconds, 0}};
        Require(utimes_nid_postfix(path.c_str(), invalid) == -1 && *__error_nid_postfix() == 22, "reject unrepresentable Windows timestamp");
        Require(futimes_nid_postfix(fd, invalid) == -1 && *__error_nid_postfix() == 22, "futimes rejects unrepresentable Windows timestamp");
        CheckTimes(path, fd, first);
    }
#endif

    const auto before = std::chrono::system_clock::now();
    Require(futimes_nid_postfix(fd, nullptr) == 0, "futimes current time");
    const auto after = std::chrono::system_clock::now();
    FileStat now{};
    Require(stat_nid_postfix(path.c_str(), &now) == 0, "stat current time");
    const auto recorded = std::chrono::seconds(now.st_mtim.tv_sec) + std::chrono::nanoseconds(now.st_mtim.tv_nsec);
    Require(recorded >= before.time_since_epoch() - std::chrono::seconds(1) &&
        recorded <= after.time_since_epoch() + std::chrono::seconds(1), "null times use current clock");
    Require(utimes_nid_postfix(path.c_str(), nullptr) == 0, "utimes current time");
    Require(close_nid_postfix(fd) == 0, "close fixture descriptor");

    const auto directory = (root / "directory").string();
    Require(std::filesystem::create_directory(directory), "create timestamp directory");
    const int directoryFd = open_nid_postfix(directory.c_str(), 0, 0);
    Require(directoryFd >= 0, "open directory descriptor");
    Require(utimes_nid_postfix(directory.c_str(), first) == 0, "directory utimes");
    FileStat directoryStatus{};
    Require(stat_nid_postfix(directory.c_str(), &directoryStatus) == 0 && Same(directoryStatus.st_mtim, first[1]), "directory stat precision");
    Require(futimes_nid_postfix(directoryFd, second) == 0, "directory futimes");
    Require(sceKernelFstat(directoryFd, &directoryStatus) == 0 && Same(directoryStatus.st_mtim, second[1]), "directory fstat precision");
    Require(close_nid_postfix(directoryFd) == 0, "close directory descriptor");

    Require(utimes_nid_postfix((root / "missing").string().c_str(), first) == -1 && *__error_nid_postfix() == 2, "missing path reports ENOENT");
    int pipe[2];
    Require(pipe_nid_postfix(pipe) == 0, "create pipe");
    FileStat pipeStatus{};
    Require(sceKernelFstat(pipe[0], &pipeStatus) == 0, "pipe stat still works");
#ifdef _WIN32
    Require(futimes_nid_postfix(pipe[0], first) == -1 && *__error_nid_postfix() == 22, "pipe timestamp update fails");
#endif
    Require(close_nid_postfix(pipe[0]) == 0 && close_nid_postfix(pipe[1]) == 0, "close pipe");
    std::filesystem::remove_all(root);
}
