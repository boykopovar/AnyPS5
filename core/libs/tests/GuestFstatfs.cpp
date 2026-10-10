#include "prx/libc/include/General.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

struct GuestStatfs {
    std::uint32_t f_version;
    std::uint32_t f_type;
    std::uint64_t f_flags;
    std::uint64_t f_bsize;
    std::uint64_t f_iosize;
    std::uint64_t f_blocks;
    std::uint64_t f_bfree;
    std::int64_t f_bavail;
    std::uint64_t f_files;
    std::int64_t f_ffree;
    std::uint64_t f_syncwrites;
    std::uint64_t f_asyncwrites;
    std::uint64_t f_syncreads;
    std::uint64_t f_asyncreads;
    std::uint64_t f_spare[10];
    std::uint32_t f_namemax;
    std::uint32_t f_owner;
    std::int32_t f_fsid[2];
    char f_charspare[80];
    char f_fstypename[16];
    char f_mntfromname[88];
    char f_mntonname[88];
};
static_assert(sizeof(GuestStatfs) == 472);

extern "C" {
int APS5_VABI _fstatfs_nid_postfix(int, GuestStatfs*);
int APS5_VABI sceKernelOpen(const char*, int, std::uint16_t);
int APS5_VABI close_nid_postfix(int);
int APS5_VABI pipe_nid_postfix(int*);
int APS5_VABI socketpair_nid_postfix(int, int, int, int*);
int* APS5_VABI __error_nid_postfix();
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "fstatfs check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static constexpr std::uint32_t StatfsVersion = 0x20030518;
static constexpr int GuestEbadf = 9;
static constexpr int GuestEfault = 14;
static constexpr int GuestEinval = 22;
static constexpr int FirstSocket = 0x10000000;

static void RequireFailure(int descriptor, int error) {
    GuestStatfs fs;
    std::memset(&fs, 0x5a, sizeof(fs));
    *__error_nid_postfix() = 0;
    Require(_fstatfs_nid_postfix(descriptor, &fs) == -1);
    Require(*__error_nid_postfix() == error);
    for (std::size_t i = 0; i < sizeof(fs); ++i) Require(reinterpret_cast<const unsigned char*>(&fs)[i] == 0x5a);
}

static void RequireVolume(int descriptor, const std::filesystem::path& path) {
    GuestStatfs fs;
    std::memset(&fs, 0x5a, sizeof(fs));
    Require(_fstatfs_nid_postfix(descriptor, &fs) == 0);
    const auto space = std::filesystem::space(path);
    Require(fs.f_version == StatfsVersion);
    Require(fs.f_bsize > 0 && fs.f_iosize > 0);
    Require(fs.f_blocks > 0 && fs.f_bfree <= fs.f_blocks);
    Require(fs.f_bavail >= 0 && static_cast<std::uint64_t>(fs.f_bavail) <= fs.f_bfree);
    Require(fs.f_blocks * fs.f_bsize <= space.capacity && space.capacity - fs.f_blocks * fs.f_bsize < fs.f_bsize);
    Require((fs.f_flags & 0x1) == 0);
    Require(fs.f_namemax >= 255);
    Require(fs.f_fstypename[0] == '\0' && fs.f_mntfromname[0] == '\0' && fs.f_mntonname[0] == '\0');
}

int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-fstatfs-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto filePath = root / "file.txt";
    {
        std::ofstream file(filePath, std::ios::binary);
        file << "fstatfs";
        Require(static_cast<bool>(file));
    }

    AddPathAlias_nid_no_patch("/anyps5-fstatfs-test", root.string().c_str());
    const int file = sceKernelOpen("/anyps5-fstatfs-test/file.txt", SCE_KERNEL_O_RDONLY, 0);
    Require(file >= 0);
    RequireVolume(file, filePath);
    *__error_nid_postfix() = 0;
    Require(_fstatfs_nid_postfix(file, nullptr) == -1);
    Require(*__error_nid_postfix() == GuestEfault);

    const int directory = sceKernelOpen("/anyps5-fstatfs-test", SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(directory >= 0);
    RequireVolume(directory, root);
    Require(close_nid_postfix(directory) == 0);

    int pipe[2];
    Require(pipe_nid_postfix(pipe) == 0);
    RequireFailure(pipe[0], GuestEinval);
    RequireFailure(pipe[1], GuestEinval);
    Require(close_nid_postfix(pipe[0]) == 0);
    Require(close_nid_postfix(pipe[1]) == 0);

    int sockets[2];
    Require(socketpair_nid_postfix(1, 1, 0, sockets) == 0);
    Require(sockets[0] >= FirstSocket);
    RequireFailure(sockets[0], GuestEinval);
    Require(close_nid_postfix(sockets[0]) == 0);
    Require(close_nid_postfix(sockets[1]) == 0);
    RequireFailure(sockets[0], GuestEbadf);

    RequireFailure(-1, GuestEbadf);
    Require(close_nid_postfix(file) == 0);
#ifndef _WIN32
    RequireFailure(file, GuestEbadf);
#endif

    std::filesystem::remove_all(root);
}
