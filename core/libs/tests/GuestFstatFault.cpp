#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#ifdef __linux__
#include <errno.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#endif

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

static void RequireBadPointerAt(int descriptor, const char* path, void* address) {
    {
        auto* bad = static_cast<FileStat*>(address);
        *__error_nid_postfix() = 0;
        Require(fstat_nid_disambig1_nid_postfix(descriptor, bad) == -1 && *__error_nid_postfix() == GuestEfault);
        Require(sceKernelFstat(descriptor, bad) == static_cast<int>(0x80020000u | GuestEfault));
        *__error_nid_postfix() = 0;
        Require(stat_nid_postfix(path, bad) == -1 && *__error_nid_postfix() == GuestEfault);
        *__error_nid_postfix() = 0;
        Require(lstat_nid_postfix(path, bad) == -1 && *__error_nid_postfix() == GuestEfault);
        Require(sceKernelStat(path, bad) == static_cast<int>(0x80020000u | GuestEfault));
        *__error_nid_postfix() = 0;
        Require(_fstatfs_nid_postfix(descriptor, reinterpret_cast<KernelStatfs*>(bad)) == -1 && *__error_nid_postfix() == GuestEfault);
    }
}

static void RequireUnwritablePage(int descriptor, const char* path) {
#ifdef _WIN32
    void* page = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READONLY);
    Require(page != nullptr);
    void* none = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_NOACCESS);
    Require(none != nullptr);
    RequireBadPointerAt(descriptor, path, page);
    RequireBadPointerAt(descriptor, path, none);
    VirtualFree(page, 0, MEM_RELEASE);
    VirtualFree(none, 0, MEM_RELEASE);
#else
    void* page = mmap(nullptr, 4096, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED);
    void* none = mmap(nullptr, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(none != MAP_FAILED);
    RequireBadPointerAt(descriptor, path, page);
    RequireBadPointerAt(descriptor, path, none);
    munmap(page, 4096);
    munmap(none, 4096);
#endif
}

#ifdef __linux__
static void RequireNonEfaultFailureThrows() {
    const pid_t child = fork();
    Require(child >= 0);
    if (child == 0) {
        sock_filter filter[] = {
            BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr)),
            BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_process_vm_writev, 0, 1),
            BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
            BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
        };
        sock_fprog program{static_cast<unsigned short>(sizeof(filter) / sizeof(filter[0])), filter};
        if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0 || prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &program) != 0) _exit(2);
        char source[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        char destination[8] = {};
        try {
            GuestCopyOut_nid_no_patch(destination, source, sizeof(source));
        } catch (const std::runtime_error&) {
            _exit(destination[0] == 0 ? 0 : 3);
        }
        _exit(1);
    }
    int status = 0;
    Require(waitpid(child, &status, 0) == child);
    Require(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
#endif

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
    for (const std::uintptr_t address : {std::uintptr_t{1}, ~std::uintptr_t{0xfff}})
        RequireBadPointerAt(file, "/anyps5-fstat-fault-test/file.txt", reinterpret_cast<void*>(address));
    RequireUnwritablePage(file, "/anyps5-fstat-fault-test/file.txt");
#ifdef __linux__
    RequireNonEfaultFailureThrows();
#endif

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
