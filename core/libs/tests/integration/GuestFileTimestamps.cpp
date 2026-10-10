#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
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

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int posixEnoent = 2;
constexpr int posixEinval = 22;
constexpr int sceKernelErrorEinval = static_cast<int>(0x80020016u);
constexpr KernelTimeval first[2]{{1000000000, 123456}, {1000000001, 500000}};
constexpr KernelTimeval second[2]{{1000000002, 1}, {1000000003, 999999}};

int Errno() {
    return *__error_nid_postfix();
}

void RequireSuccess(std::int64_t result, const std::string& context) {
    const int error = Errno();
    RequireEqual(result, std::int64_t{0}, context + " (guest errno " + std::to_string(error) + ")");
}

void RequirePosixFailure(int result, int error, const std::string& context) {
    RequireEqual(result, -1, context + " result");
    RequireEqual(Errno(), error, context + " errno");
}

std::string TimeText(const KernelTimeval& time) {
    return std::to_string(time.tv_sec) + "s " + std::to_string(time.tv_usec) + "us";
}

void RequireSame(const KernelTimespec& actual, const KernelTimeval& expected, const std::string& context) {
    RequireEqual(actual.tv_sec, expected.tv_sec, context + " seconds for " + TimeText(expected));
    RequireEqual(actual.tv_nsec, expected.tv_usec * 1000, context + " nanoseconds for " + TimeText(expected));
}

class Descriptor {
public:
    explicit Descriptor(const std::string& path) : value(open_nid_postfix(path.c_str(), 0, 0)) {
        Require(value >= 0, "open read-only descriptor for " + path + " (guest errno " + std::to_string(Errno()) + ")");
    }

    ~Descriptor() {
        if (open) close_nid_postfix(value);
    }

    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;

    int Get() const noexcept { return value; }

    int Close() {
        open = false;
        return close_nid_postfix(value);
    }

private:
    int value;
    bool open = true;
};

class RootDirectory {
public:
    RootDirectory()
        : path("anyps5-file-times-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())) {
        Require(std::filesystem::create_directory(path), "create fixture directory " + path.string());
    }

    ~RootDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }

    RootDirectory(const RootDirectory&) = delete;
    RootDirectory& operator=(const RootDirectory&) = delete;

    const std::filesystem::path path;
};

std::string MakeFile(const RootDirectory& root) {
    const auto path = (root.path / "file.txt").string();
    { std::ofstream file(path); file << "timestamp fixture"; }
    return path;
}

std::string MakeDirectory(const RootDirectory& root) {
    const auto path = (root.path / "directory").string();
    Require(std::filesystem::create_directory(path), "create timestamp directory");
    return path;
}

class FileFixture {
public:
    FileFixture() : path(MakeFile(root)), descriptor(path) {}

    int Fd() const noexcept { return descriptor.Get(); }
    int Close() { return descriptor.Close(); }

private:
    RootDirectory root;

public:
    const std::string path;

private:
    Descriptor descriptor;
};

class DirectoryFixture {
public:
    DirectoryFixture() : path(MakeDirectory(root)), descriptor(path) {}

    int Fd() const noexcept { return descriptor.Get(); }
    int Close() { return descriptor.Close(); }

private:
    RootDirectory root;

public:
    const std::string path;

private:
    Descriptor descriptor;
};

void CheckTimes(const FileFixture& fixture, const KernelTimeval* expected, const std::string& context) {
    FileStat byPath{};
    FileStat byFd{};
    FileStat byKernel{};
    RequireSuccess(stat_nid_postfix(fixture.path.c_str(), &byPath), context + ": stat");
    RequireSuccess(fstat_nid_disambig1_nid_postfix(fixture.Fd(), &byFd), context + ": fstat");
    RequireSuccess(sceKernelFstat(fixture.Fd(), &byKernel), context + ": sceKernelFstat");
    const std::pair<const char*, const FileStat*> statuses[] = {{"stat", &byPath}, {"fstat", &byFd}, {"sceKernelFstat", &byKernel}};
    for (const auto& [name, status] : statuses) {
        RequireSame(status->st_atim, expected[0], context + ": " + name + " access time");
        RequireSame(status->st_mtim, expected[1], context + ": " + name + " modification time");
    }
#ifdef _WIN32
    FILETIME access{};
    FILETIME modified{};
    Require(GetFileTime(reinterpret_cast<HANDLE>(_get_osfhandle(fixture.Fd())), nullptr, &access, &modified),
            context + ": native GetFileTime");
    const auto ticks = [](FILETIME time) {
        return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
    };
    const auto expectedTicks = [](const KernelTimeval& time) {
        return static_cast<std::uint64_t>(time.tv_sec + 11644473600LL) * 10000000 + time.tv_usec * 10;
    };
    RequireEqual(ticks(access), expectedTicks(expected[0]), context + ": native access time");
    RequireEqual(ticks(modified), expectedTicks(expected[1]), context + ": native modification time");
#endif
}

const Case utimesFractional{"Utimes_FractionalTimes_PreservesMicroseconds", [] {
    const FileFixture fixture;
    RequireSuccess(utimes_nid_postfix(fixture.path.c_str(), first), "utimes");
    CheckTimes(fixture, first, "after utimes");
}};

const Case futimesReadOnly{"Futimes_ReadOnlyDescriptor_PreservesMicroseconds", [] {
    const FileFixture fixture;
    RequireSuccess(utimes_nid_postfix(fixture.path.c_str(), first), "utimes");
    RequireSuccess(futimes_nid_postfix(fixture.Fd(), second), "futimes");
    CheckTimes(fixture, second, "after futimes");
}};

const Case sceUtimesFractional{"SceKernelUtimes_FractionalTimes_PreservesMicroseconds", [] {
    const FileFixture fixture;
    RequireSuccess(futimes_nid_postfix(fixture.Fd(), second), "futimes");
    RequireSuccess(sceKernelUtimes_nid_postfix(fixture.path.c_str(), first), "sceKernelUtimes");
    CheckTimes(fixture, first, "after sceKernelUtimes");
}};

template<typename TSetter>
void RequireInvalidMicrosecondsRejected(const FileFixture& fixture, const TSetter& setter) {
    RequireSuccess(utimes_nid_postfix(fixture.path.c_str(), first), "utimes");
    for (int index = 0; index < 2; ++index) {
        for (const auto micros : {-1LL, 1000000LL}) {
            const std::string context = "time " + std::to_string(index) + " with " + std::to_string(micros) + " microseconds";
            KernelTimeval invalid[2]{first[0], first[1]};
            invalid[index].tv_usec = micros;
            setter(invalid, context);
            CheckTimes(fixture, first, context);
        }
    }
}

const Case utimesInvalid{"Utimes_InvalidMicroseconds_FailsWithEinvalAndKeepsTimes", [] {
    const FileFixture fixture;
    RequireInvalidMicrosecondsRejected(fixture, [&fixture](const KernelTimeval* invalid, const std::string& context) {
        RequirePosixFailure(utimes_nid_postfix(fixture.path.c_str(), invalid), posixEinval, "utimes " + context);
    });
}};

const Case futimesInvalid{"Futimes_InvalidMicroseconds_FailsWithEinvalAndKeepsTimes", [] {
    const FileFixture fixture;
    RequireInvalidMicrosecondsRejected(fixture, [&fixture](const KernelTimeval* invalid, const std::string& context) {
        RequirePosixFailure(futimes_nid_postfix(fixture.Fd(), invalid), posixEinval, "futimes " + context);
    });
}};

const Case sceUtimesInvalid{"SceKernelUtimes_InvalidMicroseconds_FailsWithEinvalAndKeepsTimes", [] {
    const FileFixture fixture;
    RequireInvalidMicrosecondsRejected(fixture, [&fixture](const KernelTimeval* invalid, const std::string& context) {
        RequireEqual(sceKernelUtimes_nid_postfix(fixture.path.c_str(), invalid), sceKernelErrorEinval, "sceKernelUtimes " + context);
    });
}};

#ifdef _WIN32
class NativeHandle {
public:
    explicit NativeHandle(const std::string& path)
        : handle(CreateFileW(std::filesystem::path(path).c_str(), FILE_WRITE_ATTRIBUTES | FILE_READ_ATTRIBUTES,
                             FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr)) {
        Require(handle != INVALID_HANDLE_VALUE, "open native timestamp handle");
    }

    ~NativeHandle() {
        if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
    }

    NativeHandle(const NativeHandle&) = delete;
    NativeHandle& operator=(const NativeHandle&) = delete;

    HANDLE Get() const noexcept { return handle; }

    bool Close() {
        const HANDLE closing = handle;
        handle = INVALID_HANDLE_VALUE;
        return CloseHandle(closing) != 0;
    }

private:
    HANDLE handle;
};

const Case nativePrecision{"Stat_NativeHundredNanosecondTimes_RetainsNativePrecision", [] {
    const FileFixture fixture;
    NativeHandle handle(fixture.path);
    constexpr std::uint64_t ticks = 116444736000000000ULL + 1000000000ULL * 10000000 + 1234567;
    const FILETIME precise{static_cast<DWORD>(ticks), static_cast<DWORD>(ticks >> 32)};
    Require(SetFileTime(handle.Get(), &precise, &precise, &precise), "set native 100-nanosecond timestamps");
    FILE_BASIC_INFO native{};
    Require(GetFileInformationByHandleEx(handle.Get(), FileBasicInfo, &native, sizeof(native)), "read native basic info");
    FileStat status{};
    RequireSuccess(stat_nid_postfix(fixture.path.c_str(), &status), "stat");
    RequireEqual(status.st_birthtim.tv_sec, std::int64_t{1000000000}, "creation seconds");
    RequireEqual(status.st_birthtim.tv_nsec, std::int64_t{123456700}, "creation nanoseconds");
    RequireEqual(status.st_atim.tv_sec, std::int64_t{1000000000}, "access seconds");
    RequireEqual(status.st_atim.tv_nsec, std::int64_t{123456700}, "access nanoseconds");
    RequireEqual(status.st_mtim.tv_sec, std::int64_t{1000000000}, "modification seconds");
    RequireEqual(status.st_mtim.tv_nsec, std::int64_t{123456700}, "modification nanoseconds");
    RequireEqual(status.st_ctim.tv_sec, static_cast<std::int64_t>(native.ChangeTime.QuadPart / 10000000 - 11644473600LL),
                 "change seconds from native change time");
    RequireEqual(status.st_ctim.tv_nsec, static_cast<std::int64_t>(native.ChangeTime.QuadPart % 10000000 * 100),
                 "change nanoseconds from native change time");
    Require(handle.Close(), "close native timestamp handle");
}};

const Case aroundEpoch{"Utimes_TimesAroundUnixEpoch_PreservesMicroseconds", [] {
    const FileFixture fixture;
    const KernelTimeval preEpoch[2]{{-1, 500000}, {0, 1}};
    RequireSuccess(utimes_nid_postfix(fixture.path.c_str(), preEpoch), "utimes around epoch");
    CheckTimes(fixture, preEpoch, "after utimes around epoch");
}};

const Case unrepresentable{"UtimesAndFutimes_UnrepresentableWindowsTime_FailWithEinvalAndKeepTimes", [] {
    const FileFixture fixture;
    RequireSuccess(utimes_nid_postfix(fixture.path.c_str(), first), "utimes valid times");
    for (const auto seconds : {std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max(),
                               std::int64_t{-11644473601LL}, std::int64_t{-11644473600LL}}) {
        const std::string context = "modification seconds " + std::to_string(seconds);
        const KernelTimeval invalid[2]{first[0], {seconds, 0}};
        RequirePosixFailure(utimes_nid_postfix(fixture.path.c_str(), invalid), posixEinval, "utimes " + context);
        RequirePosixFailure(futimes_nid_postfix(fixture.Fd(), invalid), posixEinval, "futimes " + context);
        CheckTimes(fixture, first, context);
    }
}};
#endif

const Case futimesNow{"Futimes_NullTimes_UsesCurrentClock", [] {
    const FileFixture fixture;
    RequireSuccess(utimes_nid_postfix(fixture.path.c_str(), first), "utimes");
    const auto before = std::chrono::system_clock::now();
    RequireSuccess(futimes_nid_postfix(fixture.Fd(), nullptr), "futimes current time");
    const auto after = std::chrono::system_clock::now();
    FileStat now{};
    RequireSuccess(stat_nid_postfix(fixture.path.c_str(), &now), "stat current time");
    const auto recorded = std::chrono::seconds(now.st_mtim.tv_sec) + std::chrono::nanoseconds(now.st_mtim.tv_nsec);
    Require(recorded >= before.time_since_epoch() - std::chrono::seconds(1) &&
            recorded <= after.time_since_epoch() + std::chrono::seconds(1),
            "modification time " + std::to_string(now.st_mtim.tv_sec) + "s within one second of the current clock");
}};

const Case utimesNow{"Utimes_NullTimes_Succeeds", [] {
    FileFixture fixture;
    RequireSuccess(utimes_nid_postfix(fixture.path.c_str(), nullptr), "utimes current time");
    RequireSuccess(fixture.Close(), "close fixture descriptor");
}};

const Case directoryUtimes{"Utimes_Directory_PreservesMicroseconds", [] {
    DirectoryFixture fixture;
    RequireSuccess(utimes_nid_postfix(fixture.path.c_str(), first), "directory utimes");
    FileStat status{};
    RequireSuccess(stat_nid_postfix(fixture.path.c_str(), &status), "directory stat");
    RequireSame(status.st_mtim, first[1], "directory stat modification time");
}};

const Case directoryFutimes{"Futimes_DirectoryDescriptor_PreservesMicroseconds", [] {
    DirectoryFixture fixture;
    RequireSuccess(futimes_nid_postfix(fixture.Fd(), second), "directory futimes");
    FileStat status{};
    RequireSuccess(sceKernelFstat(fixture.Fd(), &status), "directory fstat");
    RequireSame(status.st_mtim, second[1], "directory fstat modification time");
    RequireSuccess(fixture.Close(), "close directory descriptor");
}};

const Case missingPath{"Utimes_MissingPath_FailsWithEnoent", [] {
    const RootDirectory root;
    RequirePosixFailure(utimes_nid_postfix((root.path / "missing").string().c_str(), first), posixEnoent, "utimes missing");
}};

class Pipe {
public:
    Pipe() {
        RequireSuccess(pipe_nid_postfix(descriptors), "create pipe");
        open = true;
    }

    ~Pipe() {
        if (open) {
            close_nid_postfix(descriptors[0]);
            close_nid_postfix(descriptors[1]);
        }
    }

    Pipe(const Pipe&) = delete;
    Pipe& operator=(const Pipe&) = delete;

    int Read() const noexcept { return descriptors[0]; }

    bool Close() {
        open = false;
        const bool readClosed = close_nid_postfix(descriptors[0]) == 0;
        const bool writeClosed = close_nid_postfix(descriptors[1]) == 0;
        return readClosed && writeClosed;
    }

private:
    int descriptors[2] = {-1, -1};
    bool open = false;
};

const Case pipeFstat{"SceKernelFstat_Pipe_Succeeds", [] {
    Pipe pipe;
    FileStat status{};
    RequireSuccess(sceKernelFstat(pipe.Read(), &status), "pipe fstat");
    Require(pipe.Close(), "close pipe");
}};

#ifdef _WIN32
const Case pipeFutimes{"Futimes_Pipe_FailsWithEinval", [] {
    Pipe pipe;
    RequirePosixFailure(futimes_nid_postfix(pipe.Read(), first), posixEinval, "pipe futimes");
    Require(pipe.Close(), "close pipe");
}};
#endif

} // namespace
