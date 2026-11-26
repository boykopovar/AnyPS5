#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#ifdef _WIN32
#include "prx/libc/include/WindowsVerbatimPath.hpp"
#include <windows.h>
#endif

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "fstatvfs check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static void RequireFailure(int descriptor, int error) {
    std::array<std::uint64_t, 11> output;
    output.fill(0x5A5A5A5A5A5A5A5Aull);
    const auto saved = output;
    *__error_nid_postfix() = 0;
    Require(fstatvfs_nid_postfix(descriptor, reinterpret_cast<LibcStatvfs*>(output.data())) == -1);
    Require(*__error_nid_postfix() == error);
    Require(output == saved);
    *__error_nid_postfix() = 0;
    Require(fstatvfs_nid_postfix(descriptor, nullptr) == -1);
    Require(*__error_nid_postfix() == error);
}

static void RequireVolume(int descriptor, const std::filesystem::path& path) {
    std::array<std::uint64_t, 13> guarded;
    guarded.fill(0xA5A5A5A5A5A5A5A5ull);
    *__error_nid_postfix() = 12345;
    Require(fstatvfs_nid_postfix(descriptor, reinterpret_cast<LibcStatvfs*>(guarded.data() + 1)) == 0);
    Require(*__error_nid_postfix() == 12345);
    Require(guarded.front() == 0xA5A5A5A5A5A5A5A5ull && guarded.back() == guarded.front());
    const auto* fields = guarded.data() + 1;
    const auto capacity = std::filesystem::space(path).capacity;
    Require(fields[6] > 0 && fields[8] > 0);
    Require(fields[2] > 0 && fields[2] == capacity / fields[8]);
    Require(fields[0] <= fields[1] && fields[1] <= fields[2]);
    Require(fields[3] == fields[4]);
    Require((fields[7] & ~std::uint64_t{3}) == 0);
    Require(fields[9] == 0 && fields[10] >= 255);
}

static void RequireDevice(int descriptor) {
    std::array<std::uint64_t, 13> guarded;
    guarded.fill(0xA5A5A5A5A5A5A5A5ull);
    *__error_nid_postfix() = 12345;
    Require(fstatvfs_nid_postfix(descriptor, reinterpret_cast<LibcStatvfs*>(guarded.data() + 1)) == 0);
    Require(*__error_nid_postfix() == 12345);
    Require(guarded.front() == 0xA5A5A5A5A5A5A5A5ull && guarded.back() == guarded.front());
    const auto* fields = guarded.data() + 1;
    const std::array<std::uint64_t, 11> expected{0, 0, 2, 0, 0, 0, 512, 0, 512, 0, 255};
    Require(std::memcmp(fields, expected.data(), sizeof(expected)) == 0);
}

int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-fstatvfs-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto filePath = root / "file.txt";
    {
        std::ofstream file(filePath, std::ios::binary);
        file << "fstatvfs";
        Require(static_cast<bool>(file));
    }
    AddPathAlias_nid_no_patch("/anyps5-fstatvfs-test", root.string().c_str());
    const int file = sceKernelOpen("/anyps5-fstatvfs-test/file.txt", SCE_KERNEL_O_RDONLY, 0);
    Require(file >= 0);
    Require(sceKernelLseek(file, 3, 0) == 3);
    RequireVolume(file, filePath);
    Require(sceKernelLseek(file, 0, 1) == 3);
    Require(fstatvfs_nid_postfix(file, nullptr) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(fstatvfs_nid_postfix(file, reinterpret_cast<LibcStatvfs*>(0x10)) == -1 && *__error_nid_postfix() == 14);

    const int directory = sceKernelOpen("/anyps5-fstatvfs-test", SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(directory >= 0);
    RequireVolume(directory, root);
    Require(fstatvfs_nid_postfix(directory, nullptr) == -1 && *__error_nid_postfix() == 14);
    Require(close_nid_postfix(directory) == 0);
    RequireFailure(directory, 9);

    const int random = sceKernelOpen("/dev/urandom", SCE_KERNEL_O_RDONLY, 0);
    Require(random >= 0);
    RequireDevice(random);
    Require(fstatvfs_nid_postfix(random, nullptr) == -1 && *__error_nid_postfix() == 14);
    Require(close_nid_postfix(random) == 0);
    RequireFailure(random, 9);

    const int queue = kqueue_nid_postfix();
    Require(queue >= 0);
    RequireFailure(queue, 22);
    Require(close_nid_postfix(queue) == 0);
    RequireFailure(queue, 9);

    int pipes[2];
    Require(pipe_nid_postfix(pipes) == 0);
    RequireFailure(pipes[0], 22);
    RequireFailure(pipes[1], 22);
    for (int fd : pipes) Require(close_nid_postfix(fd) == 0);
    RequireFailure(pipes[0], 9);
    RequireFailure(pipes[1], 9);

    int sockets[2];
    Require(socketpair_nid_postfix(1, 1, 0, sockets) == 0);
    RequireFailure(sockets[0], 22);
    RequireFailure(sockets[1], 22);
    for (int fd : sockets) Require(close_nid_postfix(fd) == 0);
    RequireFailure(sockets[0], 9);
    RequireFailure(sockets[1], 9);

    RequireFailure(-1, 9);
    RequireFailure(-100, 9);
    RequireFailure(1024, 9);

    Require(close_nid_postfix(file) == 0);
    RequireFailure(file, 9);

#ifdef _WIN32
    const int nulFile = sceKernelOpen("/anyps5-fstatvfs-test/nul", SCE_KERNEL_O_CREAT | SCE_KERNEL_O_RDWR, 0666);
    Require(nulFile >= 0);
    RequireVolume(nulFile, root);
    Require(close_nid_postfix(nulFile) == 0);
    std::error_code nulError;
    const auto verbatimNul = NativeVerbatimPath_nid_no_patch(root / "nul", nulError, WindowsMaxPathLength);
    Require(!nulError && DeleteFileW(verbatimNul.c_str()) != 0);
#endif
    std::string deepGuest = "/anyps5-fstatvfs-test";
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
    const int deepDir = sceKernelOpen(deepGuest.c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(deepDir >= 0);
    RequireVolume(deepDir, root);
    Require(close_nid_postfix(deepDir) == 0);
    const int deepFile = sceKernelOpen((deepGuest + "/deep.txt").c_str(), SCE_KERNEL_O_CREAT | SCE_KERNEL_O_RDWR, 0666);
    Require(deepFile >= 0);
    RequireVolume(deepFile, root);
    Require(close_nid_postfix(deepFile) == 0);
#ifdef _WIN32
    std::error_code fileError;
    const auto verbatimDeepFile = NativeVerbatimPath_nid_no_patch(deepHost / "deep.txt", fileError, WindowsMaxPathLength);
    Require(!fileError && DeleteFileW(verbatimDeepFile.c_str()) != 0);
    for (auto it = createdDirs.rbegin(); it != createdDirs.rend(); ++it) {
        std::error_code removeError;
        const auto verbatimDir = NativeVerbatimPath_nid_no_patch(*it, removeError);
        Require(!removeError && RemoveDirectoryW(verbatimDir.c_str()) != 0);
    }
#else
    Require(std::filesystem::remove(deepHost / "deep.txt"));
    for (auto it = createdDirs.rbegin(); it != createdDirs.rend(); ++it)
        Require(std::filesystem::remove(*it));
#endif

    RemovePathAlias_nid_no_patch("/anyps5-fstatvfs-test");
    Require(std::filesystem::remove(filePath));
    Require(std::filesystem::remove(root));
}
