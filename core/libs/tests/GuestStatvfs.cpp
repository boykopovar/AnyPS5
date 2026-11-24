#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#ifdef _WIN32
#include "prx/libc/include/WindowsVerbatimPath.hpp"
#include <windows.h>
#else
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#endif

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "statvfs check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static void RequireVolume(const char* path, const std::filesystem::path& native) {
    std::array<std::uint64_t, 13> output;
    output.fill(0xA5A5A5A5A5A5A5A5ull);
    *__error_nid_postfix() = 12345;
    Require(statvfs_nid_postfix(path, reinterpret_cast<LibcStatvfs*>(output.data() + 1)) == 0);
    Require(*__error_nid_postfix() == 12345);
    Require(output.front() == 0xA5A5A5A5A5A5A5A5ull && output.back() == output.front());
    const auto* fields = output.data() + 1;
    Require(fields[6] > 0 && fields[8] > 0);
    Require(fields[2] == std::filesystem::space(native).capacity / fields[8]);
    Require(fields[0] <= fields[1] && fields[1] <= fields[2]);
    Require((fields[7] & ~std::uint64_t{3}) == 0);
    Require(fields[9] == 0 && fields[10] >= 255);
}

static void RequireError(const char* path, int error) {
    std::array<std::uint64_t, 11> output;
    output.fill(0x5A5A5A5A5A5A5A5Aull);
    const auto saved = output;
    *__error_nid_postfix() = 0;
    const int result = statvfs_nid_postfix(path, reinterpret_cast<LibcStatvfs*>(output.data()));
    if (result != -1)
        std::fprintf(stderr, "path %s: expected failure, got %d\n", path ? path : "(null)", result);
    Require(result == -1);
    if (*__error_nid_postfix() != error)
        std::fprintf(stderr, "path %s: expected errno %d, got %d\n", path ? path : "(null)", error, *__error_nid_postfix());
    Require(*__error_nid_postfix() == error && output == saved);
}

static std::size_t OpenHandles() {
#ifdef _WIN32
    DWORD count = 0;
    Require(GetProcessHandleCount(GetCurrentProcess(), &count) != 0);
    return count;
#else
#ifdef __APPLE__
    const std::filesystem::path descriptors = "/dev/fd";
#else
    const std::filesystem::path descriptors = "/proc/self/fd";
#endif
    return static_cast<std::size_t>(std::distance(std::filesystem::directory_iterator(descriptors),
        std::filesystem::directory_iterator{}));
#endif
}

int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-statvfs-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto file = root / "file.txt";
    {
        std::ofstream stream(file, std::ios::binary);
        stream << "unchanged";
        Require(static_cast<bool>(stream));
    }
    AddPathAlias_nid_no_patch("/anyps5-statvfs-test", root.string().c_str());
    RequireVolume("/anyps5-statvfs-test", root);
    RequireVolume("/anyps5-statvfs-test/file.txt", file);
    RequireVolume("/anyps5-statvfs-test/", root);
    RequireError(nullptr, 14);
    RequireError("", 2);
    RequireError("/anyps5-statvfs-test/missing", 2);
    RequireError("/anyps5-statvfs-test/file.txt/child", 20);
    RequireError("/anyps5-statvfs-test/file.txt/", 20);
    RequireError("/anyps5-statvfs-test/file.txt/.", 20);
    RequireError("/anyps5-statvfs-test/file.txt/../file.txt", 20);
    RequireError("/anyps5-statvfs-test/missing/../file.txt", 2);
    BlockPathAlias_nid_no_patch("/anyps5-statvfs-unavailable");
    RequireError("/anyps5-statvfs-unavailable", 2);
    AddPathAlias_nid_no_patch("/anyps5-statvfs-unavailable", root.string().c_str());
    RemovePathAlias_nid_no_patch("/anyps5-statvfs-unavailable");
    Require(statvfs_nid_postfix("/anyps5-statvfs-test/file.txt", nullptr) == -1);
    Require(*__error_nid_postfix() == 14);
    Require(statvfs_nid_postfix("/anyps5-statvfs-test/missing", nullptr) == -1);
    Require(*__error_nid_postfix() == 14);
    std::array<std::uint64_t, 11> unreadablePathOutput{};
    *__error_nid_postfix() = 0;
    Require(statvfs_nid_postfix(reinterpret_cast<const char*>(0x10), reinterpret_cast<LibcStatvfs*>(unreadablePathOutput.data())) == -1);
    const std::array<std::uint64_t, 11> untouched{};
    Require(*__error_nid_postfix() == 14 && unreadablePathOutput == untouched);
    *__error_nid_postfix() = 0;
    Require(statvfs_nid_postfix("/anyps5-statvfs-test/file.txt", reinterpret_cast<LibcStatvfs*>(0x10)) == -1);
    Require(*__error_nid_postfix() == 14);

    const auto unicodeFile = root / std::filesystem::path(u8"\u65e5.txt");
    {
        std::ofstream stream(unicodeFile);
        stream << "unicode path";
        Require(static_cast<bool>(stream));
    }
    RequireVolume("/anyps5-statvfs-test/\xE6\x97\xA5.txt", unicodeFile);
    Require(std::filesystem::remove(unicodeFile));
#ifdef _WIN32
    const HANDLE exclusive = CreateFileW(file.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    Require(exclusive != INVALID_HANDLE_VALUE);
    RequireVolume("/anyps5-statvfs-test/file.txt", file);
    Require(CloseHandle(exclusive) != 0);
#else
    Require(::chmod(file.c_str(), 0) == 0);
    RequireVolume("/anyps5-statvfs-test/file.txt", file);
    Require(::chmod(file.c_str(), 0600) == 0);
    const auto fifo = root / "fifo";
    Require(::mkfifo(fifo.c_str(), 0600) == 0);
    RequireVolume("/anyps5-statvfs-test/fifo", fifo);
    const auto socketPath = root / "socket";
    const int socket = ::socket(AF_UNIX, SOCK_STREAM, 0);
    Require(socket >= 0);
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    const auto name = socketPath.string();
    Require(name.size() < sizeof(address.sun_path));
    std::memcpy(address.sun_path, name.c_str(), name.size() + 1);
    Require(::bind(socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0);
    RequireVolume("/anyps5-statvfs-test/socket", socketPath);
    Require(::close(socket) == 0);
    Require(std::filesystem::remove(socketPath));
    Require(std::filesystem::remove(fifo));
    std::filesystem::create_symlink(file.filename(), root / "link");
    RequireVolume("/anyps5-statvfs-test/link", file);
    Require(std::filesystem::remove(root / "link"));
    std::filesystem::create_symlink("missing", root / "dangling");
    RequireError("/anyps5-statvfs-test/dangling", 2);
    Require(std::filesystem::remove(root / "dangling"));
#endif
#ifdef _WIN32
    RequireError("/anyps5-statvfs-test/nul", 2);
    RequireError("/anyps5-statvfs-test/file.txt ", 2);
    RequireError("/anyps5-statvfs-test/file.txt.", 2);
    std::error_code nulError;
    const auto nulFile = root / "nul";
    const auto verbatimNul = NativeVerbatimPath_nid_no_patch(nulFile, nulError, WindowsMaxPathLength);
    Require(!nulError);
    const HANDLE nulHandle = CreateFileW(verbatimNul.c_str(), GENERIC_WRITE, 0,
        nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    Require(nulHandle != INVALID_HANDLE_VALUE);
    Require(CloseHandle(nulHandle) != 0);
    RequireVolume("/anyps5-statvfs-test/nul", root);
    RequireError("/anyps5-statvfs-test/nul/child", 20);
    RequireError("/anyps5-statvfs-test/nul/", 20);
    Require(DeleteFileW(verbatimNul.c_str()) != 0);
#endif
    std::string deepGuest = "/anyps5-statvfs-test";
    std::filesystem::path deepHost = root;
    std::vector<std::filesystem::path> createdDirs;
    for (int i = 0; i < 6; ++i) {
        const std::string segment(40, static_cast<char>('a' + i));
        deepGuest += '/' + segment;
        deepHost /= segment;
#ifdef _WIN32
        std::error_code dirError;
        const auto verbatimDir = NativeVerbatimPath_nid_no_patch(deepHost, dirError);
        Require(!dirError && CreateDirectoryW(verbatimDir.c_str(), nullptr) != 0);
#else
        Require(std::filesystem::create_directory(deepHost));
#endif
        createdDirs.push_back(deepHost);
    }
    const auto deepFile = deepHost / "deep.txt";
    const std::string deepFileGuest = deepGuest + "/deep.txt";
#ifdef _WIN32
    std::error_code fileError;
    const auto verbatimDeepFile = NativeVerbatimPath_nid_no_patch(deepFile, fileError, WindowsMaxPathLength);
    Require(!fileError);
    const HANDLE deepHandle = CreateFileW(verbatimDeepFile.c_str(), GENERIC_WRITE, 0,
        nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    Require(deepHandle != INVALID_HANDLE_VALUE);
    Require(CloseHandle(deepHandle) != 0);
#else
    {
        std::ofstream stream(deepFile, std::ios::binary);
        stream << "deep";
        Require(static_cast<bool>(stream));
    }
#endif
    RequireVolume(deepGuest.c_str(), root);
    RequireVolume((deepGuest + "/").c_str(), root);
    RequireVolume(deepFileGuest.c_str(), root);
    RequireError((deepGuest + "/missing").c_str(), 2);
    RequireError((deepFileGuest + "/child").c_str(), 20);
    RequireError((deepFileGuest + "/").c_str(), 20);
    RequireError((deepFileGuest + "/.").c_str(), 20);
#ifdef _WIN32
    Require(DeleteFileW(verbatimDeepFile.c_str()) != 0);
    for (auto it = createdDirs.rbegin(); it != createdDirs.rend(); ++it) {
        std::error_code removeError;
        const auto verbatimDir = NativeVerbatimPath_nid_no_patch(*it, removeError);
        Require(!removeError && RemoveDirectoryW(verbatimDir.c_str()) != 0);
    }
#else
    Require(std::filesystem::remove(deepFile));
    for (auto it = createdDirs.rbegin(); it != createdDirs.rend(); ++it)
        Require(std::filesystem::remove(*it));
#endif
    const auto before = OpenHandles();
    for (int i = 0; i != 32; ++i) {
        RequireVolume("/anyps5-statvfs-test/file.txt", file);
        Require(statvfs_nid_postfix("/anyps5-statvfs-test/file.txt", nullptr) == -1);
        Require(*__error_nid_postfix() == 14);
    }
    Require(OpenHandles() == before);
    {
        std::ifstream stream(file, std::ios::binary);
        std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        Require(text == "unchanged");
    }
    RemovePathAlias_nid_no_patch("/anyps5-statvfs-test");
    Require(std::filesystem::remove(file));
    Require(std::filesystem::remove(root));
}
