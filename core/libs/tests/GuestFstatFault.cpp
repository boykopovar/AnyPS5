#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "fstat check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static constexpr int GuestEbadf = 9;
static constexpr int GuestEfault = 14;

static void RequireFailure(int descriptor, int error) {
    *__error_nid_postfix() = 0;
    Require(fstat_nid_disambig1_nid_postfix(descriptor, nullptr) == -1);
    Require(*__error_nid_postfix() == error);
    Require(sceKernelFstat(descriptor, nullptr) == static_cast<int>(0x80020000u | static_cast<unsigned>(error)));
}

int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-fstat-fault-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    {
        std::ofstream file(root / "file.txt", std::ios::binary);
        file << "fstat";
        Require(static_cast<bool>(file));
    }

    AddPathAlias_nid_no_patch("/anyps5-fstat-fault-test", root.string().c_str());
    const int file = sceKernelOpen("/anyps5-fstat-fault-test/file.txt", SCE_KERNEL_O_RDONLY, 0);
    Require(file >= 0);
    FileStat status{};
    Require(fstat_nid_disambig1_nid_postfix(file, &status) == 0 && status.st_size == 5);
    RequireFailure(file, GuestEfault);

    int sockets[2];
    Require(socketpair_nid_postfix(1, 1, 0, sockets) == 0);
    RequireFailure(sockets[0], GuestEfault);
    Require(close_nid_postfix(sockets[0]) == 0);
    Require(close_nid_postfix(sockets[1]) == 0);
    Require(close_nid_postfix(file) == 0);
    RequireFailure(file, GuestEbadf);
    RequireFailure(-1, GuestEbadf);

    std::filesystem::remove_all(root);
}
