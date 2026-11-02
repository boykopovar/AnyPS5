#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

static_assert(sizeof(LibcUtimbuf) == 16);

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "Utime check failed at line %d\n", line);
        std::abort();
    }
}
#define Check(value) Require((value), __LINE__)

static LibcUtimbuf ReadTimes(const std::filesystem::path& path, bool wholeSeconds = false) {
#ifdef _WIN32
    const HANDLE file = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    Check(file != INVALID_HANDLE_VALUE);
    FILETIME access{}, modified{};
    Check(GetFileTime(file, nullptr, &access, &modified));
    Check(CloseHandle(file));
    const auto decode = [wholeSeconds](FILETIME time) {
        const auto ticks = (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
        if (wholeSeconds) Check(ticks % 10000000 == 0);
        return static_cast<std::int64_t>(ticks / 10000000) - 11644473600LL;
    };
    return {decode(access), decode(modified)};
#else
    struct stat info{};
    Check(::stat(path.c_str(), &info) == 0);
    if (wholeSeconds) {
#ifdef __APPLE__
        Check(info.st_atimespec.tv_nsec == 0 && info.st_mtimespec.tv_nsec == 0);
#else
        Check(info.st_atim.tv_nsec == 0 && info.st_mtim.tv_nsec == 0);
#endif
    }
    return {info.st_atime, info.st_mtime};
#endif
}

int main() {
    const auto root = std::filesystem::path("anyps5-utime-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Check(std::filesystem::create_directory(root));
    const auto file = root / "data.bin";
    { std::ofstream stream(file); stream << "timestamps"; Check(stream.good()); }
    const auto path = file.string();
    const LibcUtimbuf times{1000000000LL, 1000000123LL};
    *__error_nid_postfix() = 71;
    Check(utime_nid_postfix(path.c_str(), &times) == 0);
    Check(*__error_nid_postfix() == 71);
    const auto stored = ReadTimes(file, true);
    Check(stored.actime == times.actime && stored.modtime == times.modtime);
    const auto missing = (root / "missing.bin").string();
    *__error_nid_postfix() = 71;
    Check(utime_nid_postfix(missing.c_str(), &times) == -1 && *__error_nid_postfix() == 2);
    *__error_nid_postfix() = 71;
    Check(utime_nid_postfix("", &times) == -1 && *__error_nid_postfix() == 2);
    *__error_nid_postfix() = 71;
    Check(utime_nid_postfix(nullptr, &times) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 71;
    Check(utime_nid_postfix(nullptr, nullptr) == -1 && *__error_nid_postfix() == 14);
    const auto unchanged = ReadTimes(file, true);
    Check(unchanged.actime == times.actime && unchanged.modtime == times.modtime);
    const auto before = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    *__error_nid_postfix() = 71;
    Check(utime_nid_postfix(path.c_str(), nullptr) == 0);
    Check(*__error_nid_postfix() == 71);
    const auto after = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const auto current = ReadTimes(file);
    Check(current.actime >= before - 1 && current.actime <= after + 1);
    Check(current.modtime >= before - 1 && current.modtime <= after + 1);
    Check(std::filesystem::remove_all(root) == 2);
}
