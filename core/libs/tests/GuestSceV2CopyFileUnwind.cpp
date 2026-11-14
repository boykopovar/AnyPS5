#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FilesystemForwards.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

enum class Phase { None, TargetOpen, Read, Write, Stat, Chmod, TargetClose, SourceClose };
struct PrimaryFailure { Phase phase; };
struct CleanupFailure {};
std::array<std::FILE*, 2> Files{};
std::array<int, 2> Descriptors{-1, -1};
std::array<int, 2> Closes{};
std::array<int, 2> CloseOrder{};
#ifdef _WIN32
std::array<HANDLE, 2> Handles{};
#endif
Phase Fault;
bool ReturnError;
bool ThrowCleanup;
bool ShortWrites;
int OpenCalls;
int CloseCalls;
int ReadCalls;
std::size_t Written;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "copy-file unwind check failed: %s\n", message);
        std::abort();
    }
}

std::FILE* TemporaryFile() {
#ifdef _WIN32
    std::array<wchar_t, MAX_PATH + 1> directory{};
    std::array<wchar_t, MAX_PATH + 1> path{};
    const auto length = GetTempPathW(static_cast<DWORD>(directory.size()), directory.data());
    Require(length > 0 && length < directory.size(), "native temporary directory");
    Require(GetTempFileNameW(directory.data(), L"aps", 0, path.data()) != 0, "native temporary path");
    const auto handle = CreateFileW(path.data(), GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (handle == INVALID_HANDLE_VALUE) DeleteFileW(path.data());
    Require(handle != INVALID_HANDLE_VALUE, "native temporary handle");
    const int descriptor = _open_osfhandle(reinterpret_cast<std::intptr_t>(handle), _O_RDWR | _O_BINARY);
    if (descriptor < 0) CloseHandle(handle);
    Require(descriptor >= 0, "native temporary descriptor");
    auto* result = _fdopen(descriptor, "w+b");
    if (!result) _close(descriptor);
    return result;
#else
    return std::tmpfile();
#endif
}

bool NativeOpen(int index) {
#ifdef _WIN32
    DWORD flags = 0;
    return GetHandleInformation(Handles[index], &flags) != 0;
#else
    return fcntl(Descriptors[index], F_GETFD) != -1;
#endif
}

bool Fail(Phase phase) {
    if (Fault != phase) return false;
    if (ReturnError) return true;
    Require(Files[0] && NativeOpen(0), "source is open at injected failure");
    if (phase != Phase::TargetOpen) Require(Files[1] && NativeOpen(1), "target is open at injected failure");
    std::fprintf(stderr, "copy-file unwind qualified=1 phase=%d source=%d target=%d\n",
        static_cast<int>(phase), Descriptors[0], Descriptors[1]);
    std::fflush(stderr);
    throw PrimaryFailure{phase};
}

int APS5_VABI Open(const char* path, int flags, std::uint16_t mode) {
    const int index = OpenCalls++;
    Require(index < 2, "open count");
    Require(std::strcmp(path, index == 0 ? "source" : "target") == 0, "open path order");
    Require(flags == (index == 0 ? 0 : 0x601) && mode == (index == 0 ? 0 : 0777), "open flags");
    if (index == 1 && Fail(Phase::TargetOpen)) return static_cast<int>(0x80020002u);
    Files[index] = TemporaryFile();
    Require(Files[index] != nullptr, "native temporary file");
    if (index == 0) {
        Require(std::fwrite("data", 1, 4, Files[index]) == 4 && std::fseek(Files[index], 0, SEEK_SET) == 0,
            "native source content qualification");
    }
#ifdef _WIN32
    Descriptors[index] = _fileno(Files[index]);
    Handles[index] = reinterpret_cast<HANDLE>(_get_osfhandle(Descriptors[index]));
#else
    Descriptors[index] = fileno(Files[index]);
#endif
    Require(Descriptors[index] >= 0 && NativeOpen(index), "native descriptor qualification");
    return Descriptors[index];
}

std::int64_t APS5_VABI Read(int descriptor, void* buffer, std::size_t size) {
    Require(descriptor == Descriptors[0] && size == 0x10000, "read arguments");
    if (Fail(Phase::Read)) return static_cast<int>(0x80020005u);
    ++ReadCalls;
    const auto count = std::fread(buffer, 1, size, Files[0]);
    Require(std::ferror(Files[0]) == 0, "native read control");
    return static_cast<std::int64_t>(count);
}

std::int64_t APS5_VABI Write(int descriptor, const void* buffer, std::size_t size) {
    Require(descriptor == Descriptors[1] && size > 0 && size <= 4, "write arguments");
    if (Fail(Phase::Write)) return static_cast<int>(0x80020005u);
    const auto count = ShortWrites ? std::min<std::size_t>(size, 2) : size;
    Require(std::fwrite(buffer, 1, count, Files[1]) == count, "native write control");
    Written += count;
    return static_cast<std::int64_t>(count);
}

int APS5_VABI Stat(int descriptor, FileStat* status) {
    Require(descriptor == Descriptors[0] && status != nullptr, "stat arguments");
    if (Fail(Phase::Stat)) return static_cast<int>(0x80020005u);
    *status = {};
    status->st_mode = 0100644;
    return 0;
}

int APS5_VABI Chmod(int descriptor, std::uint16_t mode) {
    Require(descriptor == Descriptors[1] && mode == 0644, "permission copy");
    return Fail(Phase::Chmod) ? static_cast<int>(0x80020005u) : 0;
}

int APS5_VABI Close(int descriptor) {
    const int index = descriptor == Descriptors[0] ? 0 : 1;
    Require(descriptor == Descriptors[index] && Files[index] != nullptr && Closes[index] == 0, "one close per owned descriptor");
    CloseOrder[CloseCalls++] = index;
    ++Closes[index];
    auto* file = Files[index];
    Files[index] = nullptr;
    Require(std::fclose(file) == 0 && !NativeOpen(index), "native descriptor was released");
    if (ThrowCleanup || Fault == (index == 1 ? Phase::TargetClose : Phase::SourceClose)) throw CleanupFailure{};
    return 0;
}

void Run(Phase phase, bool error = false, bool cleanup = false, bool shortWrites = false) {
    Files = {};
    Descriptors = {-1, -1};
    Closes = {};
    CloseOrder = {};
    Fault = phase;
    ReturnError = error;
    ThrowCleanup = cleanup;
    ShortWrites = shortWrites;
    OpenCalls = CloseCalls = ReadCalls = 0;
    Written = 0;
    bool caught = false;
    int result = -1;
    try {
        result = _ZNSt8__sce_v210filesystem10_Copy_fileEPKcS2__nid_postfix("source", "target");
    } catch (const PrimaryFailure& failure) {
        Require(!error && failure.phase == phase, "primary exception identity");
        caught = true;
    } catch (const CleanupFailure&) {
        Require(phase == Phase::TargetClose || phase == Phase::SourceClose, "cleanup cannot replace primary failure");
        caught = true;
    }
    Require(caught == (phase != Phase::None && !error), "exception/return mode");
    const int owned = phase == Phase::TargetOpen ? 1 : 2;
    Require(Closes[0] == 1 && Closes[1] == owned - 1 && CloseCalls == owned, "all acquired descriptors close exactly once");
    Require(CloseOrder[0] == (owned == 1 ? 0 : 1) && (owned == 1 || CloseOrder[1] == 0), "target then source close order");
    Require(Files[0] == nullptr && Files[1] == nullptr, "no native descriptor remains unpublished");
    if (!caught) Require(result == (error ? phase == Phase::TargetOpen ? 2 : 5 : 0), "ordinary return code");
    if (phase == Phase::None) Require(Written == 4 && ReadCalls == 2, "normal/short-write copy control");
}

}

int main() {
    FilesystemForwards_nid_no_patch forwards{};
    forwards.open = Open;
    forwards.read = Read;
    forwards.write = Write;
    forwards.close = Close;
    forwards.fstat = Stat;
    forwards.fchmod = Chmod;
    RegisterFilesystemForwards_nid_no_patch(&forwards);
    Run(Phase::None);
    Run(Phase::None, false, false, true);
    for (const auto phase : {Phase::TargetOpen, Phase::Read, Phase::Write, Phase::Stat, Phase::Chmod}) {
        Run(phase, true);
        Run(phase);
        Run(phase, false, true);
    }
    Run(Phase::TargetClose);
    Run(Phase::SourceClose);
    RegisterFilesystemForwards_nid_no_patch(nullptr);
}
