#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Apr/include/AprCommandBuffer.hpp"
#include "prx/libSceAmpr/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/RemoveTree.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <xmmintrin.h>
#include <filesystem>
#include <fstream>
#include <source_location>
#include <stdexcept>
#include <set>
#include <string>
#include <thread>
#include <tuple>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2
#endif
#endif
#ifndef _WIN32
#include <cerrno>
#include <sys/mman.h>
#include <unistd.h>
#endif

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Kernel sys check failed at line %u (guest errno %d)\n", location.line(), *__error_nid_postfix());
        std::abort();
    }
}

int Error() {
    return *__error_nid_postfix();
}

bool ChrootThrows(const std::string& path) {
    try {
        chroot_nid_postfix(path.c_str());

    } catch (const std::runtime_error& error) {
        return std::strstr(error.what(), path.c_str()) != nullptr;
    }
    return false;
}

template <typename Call>
bool ThrowsNaming(Call call, const char* text) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        return std::strstr(error.what(), text) != nullptr;
    }
    return false;
}

void OwnershipAndLinkTests(const std::filesystem::path& root) {
    constexpr std::uint32_t Unchanged = 0xFFFFFFFFu;
    const auto file = (root / "owned.txt").string();
    std::ofstream(file).put('x');
    const auto missing = (root / "missing.txt").string();
    const auto throughFile = file + "/inner";
    const auto linked = (root / "linked.txt").string();
    const auto other = (root / "other.txt").string();
    Require(chown_nid_postfix(file.c_str(), 0, 0) == 0);
    Require(chown_nid_postfix(file.c_str(), Unchanged, Unchanged) == 0);
    Require(lchown_nid_postfix(file.c_str(), 0, Unchanged) == 0);
    Require(fchownat_nid_postfix(-100, file.c_str(), Unchanged, 0, 0) == 0);
    Require(fchownat_nid_postfix(-100, file.c_str(), 0, 0, 0x200) == 0);
    for (const auto call : {chown_nid_postfix, lchown_nid_postfix}) {
        Require(call(missing.c_str(), 0, 0) == -1 && Error() == 2);
        Require(call(throughFile.c_str(), 0, 0) == -1 && Error() == 20);
        Require(call("", 0, 0) == -1 && Error() == 2);
        Require(call(nullptr, 0, 0) == -1 && Error() == 14);
    }
    Require(fchownat_nid_postfix(-100, missing.c_str(), 0, 0, 0) == -1 && Error() == 2);
    Require(fchownat_nid_postfix(-100, file.c_str(), 0, 0, 0x400) == -1 && Error() == 22);
    Require(chown_nid_postfix(missing.c_str(), 1000, 0) == -1 && Error() == 2);
    Require(chown_nid_postfix(nullptr, 1000, 0) == -1 && Error() == 14);
    Require(lchown_nid_postfix(throughFile.c_str(), 0, 1000) == -1 && Error() == 20);
    Require(fchownat_nid_postfix(-100, missing.c_str(), 1000, 0, 0) == -1 && Error() == 2);
    Require(fchownat_nid_postfix(4321, "owned.txt", 1000, 0, 0) == -1 && Error() == 9);
    Require(fchownat_nid_postfix(-100, file.c_str(), 1000, 0, 0x400) == -1 && Error() == 22);
    const int directory = sceKernelOpen(root.string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(directory >= 0);
    Require(fchownat_nid_postfix(directory, "owned.txt", 0, 0, 0) == 0);
    Require(fchownat_nid_postfix(directory, "missing.txt", 0, 0, 0) == -1 && Error() == 2);
    Require(fchownat_nid_postfix(directory, "missing.txt", 1000, 0, 0) == -1 && Error() == 2);
    Require(linkat_nid_postfix(-100, file.c_str(), -100, linked.c_str(), 0) == 0);
    Require(std::filesystem::hard_link_count(file) == 2);
    Require(linkat_nid_postfix(-100, file.c_str(), -100, linked.c_str(), 0x400) == -1 && Error() == 17);
    Require(linkat_nid_postfix(-100, missing.c_str(), -100, other.c_str(), 0x400) == -1 && Error() == 2);
    Require(linkat_nid_postfix(-100, file.c_str(), -100, other.c_str(), 0x200) == -1 && Error() == 22);
    Require(linkat_nid_postfix(-100, nullptr, -100, other.c_str(), 0) == -1 && Error() == 14);
    Require(linkat_nid_postfix(-100, "", -100, other.c_str(), 0) == -1 && Error() == 2);
    Require(linkat_nid_postfix(directory, "owned.txt", -100, other.c_str(), 0) == 0);
    Require(std::filesystem::hard_link_count(file) == 3 && std::filesystem::remove(other));
    Require(linkat_nid_postfix(-100, file.c_str(), directory, "other.txt", 0) == 0);
    Require(std::filesystem::hard_link_count(file) == 3);
    Require(linkat_nid_postfix(directory, "owned.txt", directory, "other.txt", 0) == -1 && Error() == 17);
    Require(linkat_nid_postfix(directory, "missing.txt", directory, "absent.txt", 0) == -1 && Error() == 2);
    Require(linkat_nid_postfix(4321, "owned.txt", directory, "absent.txt", 0) == -1 && Error() == 9);
    Require(linkat_nid_postfix(directory, "owned.txt", 4321, "absent.txt", 0) == -1 && Error() == 9);
    Require(linkat_nid_postfix(directory, "missing.txt", 4321, "absent.txt", 0) == -1 && Error() == 2);
    Require(linkat_nid_postfix(directory, "missing.txt", -100, nullptr, 0) == -1 && Error() == 2);
    Require(linkat_nid_postfix(-100, root.string().c_str(), 4321, "absent.txt", 0) == -1 && Error() == 1);
    Require(linkat_nid_postfix(-100, root.string().c_str(), -100, "", 0) == -1 && Error() == 1);
    Require(linkat_nid_postfix(directory, "owned.txt", -100, nullptr, 0) == -1 && Error() == 14);
    Require(linkat_nid_postfix(directory, "owned.txt", -100, "", 0) == -1 && Error() == 2);
    const int plain = sceKernelOpen(file.c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(plain >= 0);
    Require(linkat_nid_postfix(plain, "owned.txt", directory, "absent.txt", 0) == -1 && Error() == 20);
    Require(linkat_nid_postfix(directory, "owned.txt", plain, "absent.txt", 0) == -1 && Error() == 20);
    Require(sceKernelClose(plain) == 0 && std::filesystem::remove(other));
    Require(!std::filesystem::exists(root / "absent.txt"));
    const auto symbolic = (root / "owned.link").string();
    const auto dangling = (root / "dangling.link").string();
    if (symlink_nid_postfix("owned.txt", symbolic.c_str()) == 0) {
        Require(symlink_nid_postfix("missing.txt", dangling.c_str()) == 0);
        Require(chown_nid_postfix(symbolic.c_str(), 0, 0) == 0 && lchown_nid_postfix(symbolic.c_str(), 0, 0) == 0);
        Require(chown_nid_postfix(dangling.c_str(), 0, 0) == -1 && Error() == 2);
        Require(lchown_nid_postfix(dangling.c_str(), 0, 0) == 0);
        Require(fchownat_nid_postfix(-100, dangling.c_str(), 0, 0, 0) == -1 && Error() == 2);
        Require(fchownat_nid_postfix(-100, dangling.c_str(), 0, 0, 0x200) == 0);
        Require(ThrowsNaming([&] { linkat_nid_postfix(-100, symbolic.c_str(), -100, other.c_str(), 0); }, "symbolic link itself"));
        Require(linkat_nid_postfix(-100, symbolic.c_str(), -100, other.c_str(), 0x400) == 0);
#ifdef _WIN32
        Require((GetFileAttributesW(std::filesystem::path(other).c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) == 0);
#else
        Require(!std::filesystem::is_symlink(std::filesystem::symlink_status(other)));
#endif
        Require(std::filesystem::hard_link_count(file) == 3);
        Require(std::filesystem::remove(other) && std::filesystem::remove(symbolic) && std::filesystem::remove(dangling));
    }
    Require(sceKernelClose(directory) == 0);
    Require(std::filesystem::remove(linked) && std::filesystem::remove(file));
}

bool SymlinkTargetThrows(const char* target, const std::string& created) {
    try {
        symlink_nid_postfix(target, created.c_str());

    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

bool RemoveSymlink(const std::string& link) {
#ifdef _WIN32
    // std::filesystem::remove follows the link, reports a dangling one as
    // missing and clears the error, so remove the reparse point itself. A
    // directory reparse point, such as the one the "..." target creates, is
    // refused by DeleteFileW and needs RemoveDirectoryW instead.
    const auto wide = std::filesystem::path(link).wstring();
    return DeleteFileW(wide.c_str()) != FALSE || RemoveDirectoryW(wide.c_str()) != FALSE;
#else
    return std::filesystem::remove(link);
#endif
}

class GuardedPaths {
public:
    GuardedPaths() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        page = info.dwPageSize;
        memory = static_cast<char*>(VirtualAlloc(nullptr, 2 * page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(memory != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(memory + page, page, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        page = static_cast<std::size_t>(size);
        memory = static_cast<char*>(mmap(nullptr, 2 * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(memory != MAP_FAILED);
        Require(mprotect(memory + page, page, PROT_NONE) == 0);
#endif
    }

    ~GuardedPaths() {
#ifdef _WIN32
        Require(VirtualFree(memory, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(memory, 2 * page) == 0);
#endif
    }

    char* Unreadable() const { return memory + page; }
    char* Crossing() const { return memory + page - 3; }
    char* Boundary() const { return memory + page - 2; }

private:
    char* memory = nullptr;
    std::size_t page = 0;
};

void CheckUnreadablePaths(const std::string& renameSource, int directory) {
    constexpr int Fault = 14;
    constexpr int KernelFault = static_cast<int>(0x8002000Eu);
    GuardedPaths paths;
    std::memcpy(paths.Crossing(), "abc", 3);
    std::string copy;
    Require(CopyGuestPath_nid_no_patch(paths.Unreadable(), copy) == Fault);
    Require(CopyGuestPath_nid_no_patch(paths.Crossing(), copy) == Fault);
    paths.Boundary()[0] = '.';
    paths.Boundary()[1] = '\0';
    Require(CopyGuestPath_nid_no_patch(paths.Boundary(), copy) == 0 && copy == ".");
    const std::string maximum(1023, 'p');
    std::array<char, 1024> unterminated{};
    unterminated.fill('p');
    Require(CopyGuestPath_nid_no_patch(maximum.c_str(), copy) == 0 && copy == maximum);
    Require(CopyGuestPath_nid_no_patch(unterminated.data(), copy) == 63);
    const auto posixFault = [&](auto call) {
        *__error_nid_postfix() = 0;
        Require(call() == -1 && Error() == Fault);
    };
    FileStat status{};
    char linkBuffer[16]{};
    posixFault([&] { return open_nid_postfix(paths.Unreadable(), SCE_KERNEL_O_RDONLY, 0); });
    posixFault([&] { return chmod_nid_postfix(paths.Unreadable(), 0); });
    posixFault([&] { return mkdir_nid_postfix(paths.Unreadable(), 0755); });
    posixFault([&] { return rmdir_nid_postfix(paths.Unreadable()); });
    posixFault([&] { return unlink_nid_postfix(paths.Unreadable()); });
    posixFault([&] { return stat_nid_postfix(paths.Unreadable(), &status); });
    posixFault([&] { return lstat_nid_postfix(paths.Unreadable(), &status); });
    posixFault([&] { return truncate_nid_postfix(paths.Unreadable(), 0); });
    posixFault([&] { return utimes_nid_postfix(paths.Unreadable(), nullptr); });
    posixFault([&] { return lutimes_nid_postfix(paths.Unreadable(), nullptr); });
    posixFault([&] { return readlink_nid_postfix(paths.Unreadable(), linkBuffer, sizeof(linkBuffer)); });
    posixFault([&] { return pathconf_nid_postfix(paths.Unreadable(), 4); });
    posixFault([&] { return link_nid_postfix(paths.Unreadable(), "unused-link"); });
    posixFault([&] { return link_nid_postfix(renameSource.c_str(), paths.Unreadable()); });
    posixFault([&] { return symlink_nid_postfix(paths.Unreadable(), "unused-symlink"); });
    posixFault([&] { return symlink_nid_postfix("target", paths.Unreadable()); });
    posixFault([&] { return openat_nid_postfix(-100, paths.Unreadable(), SCE_KERNEL_O_RDONLY); });
    posixFault([&] { return mkdirat_nid_postfix(-100, paths.Unreadable(), 0755); });
    posixFault([&] { return fchmodat_nid_postfix(-100, paths.Unreadable(), 0, 0); });
    posixFault([&] { return lchmod_nid_postfix(paths.Unreadable(), 0); });
    posixFault([&] { return fstatat_nid_postfix(-100, paths.Unreadable(), &status, 0); });
    posixFault([&] { return unlinkat_nid_postfix(-100, paths.Unreadable(), 0); });
    posixFault([&] { return readlinkat_nid_postfix(-100, paths.Unreadable(), linkBuffer, sizeof(linkBuffer)); });
    posixFault([&] { return renameat_nid_postfix(-100, paths.Unreadable(), -100, "unused-rename"); });
    posixFault([&] { return renameat_nid_postfix(-100, renameSource.c_str(), -100, paths.Unreadable()); });
    posixFault([&] { return openat_nid_postfix(directory, paths.Unreadable(), SCE_KERNEL_O_RDONLY); });
    posixFault([&] { return mkdirat_nid_postfix(directory, paths.Unreadable(), 0755); });
    posixFault([&] { return fchmodat_nid_postfix(directory, paths.Unreadable(), 0, 0); });
    posixFault([&] { return fstatat_nid_postfix(directory, paths.Unreadable(), &status, 0); });
    posixFault([&] { return unlinkat_nid_postfix(directory, paths.Unreadable(), 0); });
    posixFault([&] { return readlinkat_nid_postfix(directory, paths.Unreadable(), linkBuffer, sizeof(linkBuffer)); });
    posixFault([&] { return renameat_nid_postfix(directory, paths.Unreadable(), directory, "unused-rename"); });
    posixFault([&] { return renameat_nid_postfix(directory, "file.txt", directory, paths.Unreadable()); });
    *__error_nid_postfix() = 0;
    Require(renameat_nid_postfix(directory, "missing", directory, paths.Unreadable()) == -1 && Error() == 2);
    *__error_nid_postfix() = 0;
    Require(link_nid_postfix("missing", paths.Unreadable()) == -1 && Error() == 2);
    posixFault([&] { return shm_open_nid_postfix(paths.Unreadable(), SCE_KERNEL_O_RDWR, 0600); });
    posixFault([&] { return shm_unlink_nid_postfix(paths.Unreadable()); });
    posixFault([&] { return kldfind_nid_postfix(paths.Unreadable()); });
    Require(sceKernelOpen(paths.Unreadable(), SCE_KERNEL_O_RDONLY, 0) == KernelFault);
    Require(sceKernelStat(paths.Unreadable(), &status) == KernelFault);
    Require(sceKernelMkdir(paths.Unreadable(), 0755) == KernelFault);
    Require(sceKernelRmdir(paths.Unreadable()) == KernelFault);
    Require(sceKernelUnlink(paths.Unreadable()) == KernelFault);
    Require(sceKernelChmod_nid_postfix(paths.Unreadable(), 0) == KernelFault);
    Require(sceKernelTruncate_nid_postfix(paths.Unreadable(), 0) == KernelFault);
    Require(sceKernelUtimes_nid_postfix(paths.Unreadable(), nullptr) == KernelFault);
    Require(sceKernelCheckReachability(paths.Unreadable()) == KernelFault);
    Require(sceKernelRename(paths.Unreadable(), ".") == KernelFault);
    Require(sceKernelRename(renameSource.c_str(), paths.Unreadable()) == KernelFault);
    Require(!std::filesystem::exists("unused-link") && !std::filesystem::exists("unused-symlink") && !std::filesystem::exists("unused-rename"));
}

void CheckSymlinkTargetRestrictions(const std::filesystem::path& root) {
    const auto created = (root / "rejected").string();
    for (const auto& target : {std::string(".."), std::string("../x"), std::string("a/../b"), std::string("a/../../x"), std::string("/abs")})
        Require(SymlinkTargetThrows(target.c_str(), created));
#ifdef _WIN32
    for (const auto& target : {std::string("C:/"), std::string("\\\\server\\share")})
        Require(SymlinkTargetThrows(target.c_str(), created));
#endif
    const auto links = [](const std::string& created, const char* target) {
        char buffer[16];
        const auto length = std::strlen(target);
        return readlink_nid_postfix(created.c_str(), buffer, sizeof(buffer)) == static_cast<std::int64_t>(length) &&
            std::memcmp(buffer, target, length) == 0;
    };
    const auto nested = (root / "accepted-nested").string();
    const int link = symlink_nid_postfix("sub/dir", nested.c_str());
    if (link == 0) {
        Require(links(nested, "sub/dir"));
        const auto dots = (root / "accepted-dots").string();
        Require(symlink_nid_postfix("a..b", dots.c_str()) == 0 && links(dots, "a..b"));
        const auto ellipsis = (root / "accepted-ellipsis").string();
        Require(symlink_nid_postfix("...", ellipsis.c_str()) == 0 && links(ellipsis, "..."));
        Require(RemoveSymlink(nested) && RemoveSymlink(dots) && RemoveSymlink(ellipsis));
    } else {
        Require(link == -1 && Error() == 1);
        std::fprintf(stderr, "symbolic links unavailable: the symlink target success cases not tested\n");
    }
}

UserUcontext sigreturnContext{};
UserUcontext sigreturnResume{};
volatile int sigreturnPasses = 0;
volatile std::uint32_t sigreturnMxcsr = 0;

std::uint32_t RoundingAndMasks(std::uint32_t mxcsr) {
    return mxcsr & ~0x3fu;
}

void SetContextMxcsr(UserUcontext& context, std::uint32_t mxcsr) {
    std::memcpy(reinterpret_cast<std::uint8_t*>(context.mcontext.fpstate) + 24, &mxcsr, sizeof(mxcsr));
}

void CheckSigreturnRefusals(const UserUcontext& valid) {
    UserUcontext bad = valid;
    bad.mcontext.flags = 0x8;
    Require(sigreturn_nid_postfix(&bad) == -1 && Error() == 22);
    for (const std::uint64_t privileged : {0x200ull, 0x3000ull, 0x20000ull, 0x80000ull}) {
        bad = valid;
        bad.mcontext.rflags ^= privileged;
        Require(sigreturn_nid_postfix(&bad) == -1 && Error() == 22);
    }
    bad = valid;
    bad.mcontext.fpformat = 0x10001;
    Require(sigreturn_nid_postfix(&bad) == -1 && Error() == 22);
    bad = valid;
    bad.mcontext.ownedfp = 0x20003;
    Require(sigreturn_nid_postfix(&bad) == -1 && Error() == 22);
    bad = valid;
    bad.mcontext.cs &= ~std::uint64_t{3};
    Require(ThrowsNaming([&] { sigreturn_nid_postfix(&bad); }, "not a user selector"));
}

void CheckSigreturn() {
    Require(sigreturn_nid_postfix(nullptr) == -1 && Error() == 14);
    const std::uint32_t savedMxcsr = _mm_getcsr();
    sigreturnPasses = 0;
    Require(getcontext_nid_postfix(&sigreturnContext) == 0);
    sigreturnMxcsr = _mm_getcsr();
    sigreturnPasses = sigreturnPasses + 1;
    std::uint32_t mask[4] = {};
    Require(sigprocmask_nid_postfix(1, nullptr, mask) == 0);
    const bool blocked = (mask[0] & (1u << 29)) != 0;
    if (sigreturnPasses == 1) {
        CheckSigreturnRefusals(sigreturnContext);
        sigreturnResume = sigreturnContext;
        sigreturnResume.mcontext.len = 0;
        sigreturnResume.sigmask[0] |= 1u << 29;
        SetContextMxcsr(sigreturnResume, 0x7f80);
        Require(setcontext_nid_postfix(&sigreturnResume) == -1 && Error() == 22);
        sigreturn_nid_postfix(&sigreturnResume);
        Require(false);
    }
    if (sigreturnPasses == 2) {
        Require(RoundingAndMasks(sigreturnMxcsr) == 0x7f80 && blocked);
        sigreturnResume = sigreturnContext;
        sigreturnResume.mcontext.fpformat = 0x10000;
        SetContextMxcsr(sigreturnResume, 0x5f80);
        _mm_setcsr(0x3f80);
        sigreturn_nid_postfix(&sigreturnResume);
        Require(false);
    }
    if (sigreturnPasses == 3) {
        Require(RoundingAndMasks(sigreturnMxcsr) == 0x3f80 && !blocked);
        sigreturnResume = sigreturnContext;
        sigreturnResume.mcontext.ownedfp = 0x20000;
        SetContextMxcsr(sigreturnResume, 0x5f80);
        sigreturn_nid_postfix(&sigreturnResume);
        Require(false);
    }
    Require(sigreturnPasses == 4 && RoundingAndMasks(sigreturnMxcsr) == 0x1f80 && !blocked);
    _mm_setcsr(savedMxcsr);
}

void CheckSchedSetscheduler() {
    const Pthread self = scePthreadSelf();
    int savedPolicy = 0;
    KernelSchedParam saved{};
    Require(scePthreadGetschedparam(self, &savedPolicy, &saved) == 0);
    KernelSchedParam param{700};
    Require(sched_setscheduler_nid_postfix(0, 1, nullptr) == -1 && Error() == 14);
    Require(sched_setscheduler_nid_postfix(getpid_nid_postfix() + 1, 1, &param) == -1 && Error() == 3);
    Require(sched_setscheduler_nid_postfix(-1, 1, &param) == -1 && Error() == 3);
    for (const int policy : {-1, 0, 4, 7}) Require(sched_setscheduler_nid_postfix(0, policy, &param) == -1 && Error() == 22);
    int policy = 0;
    KernelSchedParam current{};
    Require(sched_setscheduler_nid_postfix(0, 1, &param) == 0);
    Require(scePthreadGetschedparam(self, &policy, &current) == 0 && policy == 1 && current.sched_priority == 700);
    param.sched_priority = 300;
    Require(sched_setscheduler_nid_postfix(0, 3, &param) == 0);
    Require(scePthreadGetschedparam(self, &policy, &current) == 0 && policy == 3 && current.sched_priority == 300);
    Require(scePthreadSetschedparam(self, savedPolicy, &saved) == 0);
}

void CheckCondattrClock() {
    constexpr KernelClockid Realtime = 0;
    constexpr KernelClockid Monotonic = 4;
    PthreadCondattr attr = nullptr;
    KernelClockid clock = -1;
    Require(scePthreadCondattrGetclock(nullptr, &clock) == SCE_KERNEL_ERROR_EINVAL && clock == -1);
    Require(scePthreadCondattrGetclock(nullptr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadCondattrGetclock(&attr, &clock) == SCE_KERNEL_ERROR_EINVAL && clock == -1);
    Require(scePthreadCondattrInit(&attr) == 0 && attr != nullptr);
    Require(scePthreadCondattrGetclock(&attr, &clock) == 0 && clock == Realtime);
    Require(scePthreadCondattrSetclock(&attr, Monotonic) == 0);
    Require(scePthreadCondattrGetclock(&attr, &clock) == 0 && clock == Monotonic);
    bool getclockThrew = false;
    try { scePthreadCondattrGetclock(&attr, nullptr); } catch (const std::runtime_error&) { getclockThrew = true; }
    Require(getclockThrew);
    Require(scePthreadCondattrDestroy(&attr) == 0 && attr == nullptr);
    clock = -1;
    Require(scePthreadCondattrGetclock(&attr, &clock) == SCE_KERNEL_ERROR_EINVAL && clock == -1);
}

template<class TRecord, class TWrite>
void CheckAmmWriter(TRecord record, TWrite write, int expected) {
    std::array<std::uint8_t, 64> recorded{};
    Apr::CommandBufferObject buffer{Apr::BufferType::Generic, 0, 0, 0, static_cast<std::uint32_t>(recorded.size()), recorded.data()};
    Require(record(&buffer) == expected);
    std::uint64_t measured = ~std::uint64_t{0};
    Require(write(nullptr, &measured) == expected);
    std::array<std::uint8_t, 64> written{};
    written.fill(0xCC);
    std::uint64_t reported = ~std::uint64_t{0};
    Require(write(written.data(), &reported) == expected);
    if (expected != 0) {
        Require(buffer.offset == 0 && measured == ~std::uint64_t{0} && reported == ~std::uint64_t{0});
        Require(std::all_of(written.begin(), written.end(), [](std::uint8_t byte) { return byte == 0xCC; }));
        return;
    }
    Require(buffer.offset != 0 && measured == buffer.offset && reported == buffer.offset);
    Require(std::memcmp(written.data(), recorded.data(), buffer.offset) == 0 && written[buffer.offset] == 0xCC);
    written.fill(0xCC);
    Require(write(written.data(), nullptr) == 0 && std::memcmp(written.data(), recorded.data(), buffer.offset) == 0);
}

void CheckAmmWriteCommands() {
    constexpr std::uint64_t page = 0x4000;
    constexpr std::uint64_t address = 0x10000000;
    constexpr std::uint64_t source = 0x20000000;
    constexpr std::uint64_t directOffset = 0x300000;
    constexpr std::uint64_t size = 2 * page;
    constexpr std::int32_t type = 3;
    constexpr std::int32_t protection = 0x33;
    constexpr std::int32_t mask = 0x13;
    constexpr std::uint8_t gpuMaskId = 5;
    constexpr std::int32_t badProtection = 0x04;
    for (const auto [mapped, offset, protect, expected] : {std::tuple{address, directOffset, protection, 0}, std::tuple{address + 8, directOffset, protection, SCE_KERNEL_ERROR_EINVAL}, std::tuple{address, directOffset + 8, protection, SCE_KERNEL_ERROR_EINVAL}, std::tuple{address, directOffset, badProtection, SCE_KERNEL_ERROR_EINVAL}}) {
        const bool alignedOffset = offset == directOffset;
        if (alignedOffset) {
            CheckAmmWriter([&](Apr::CommandBufferObject* buffer) { return sceAmprAmmCommandBufferMap(buffer, mapped, size, type, protect); }, [&](void* dst, std::uint64_t* outSize) { return sceKernelWriteMapCommand(dst, mapped, size, type, static_cast<std::uint32_t>(protect), outSize); }, expected);
            CheckAmmWriter([&](Apr::CommandBufferObject* buffer) { return sceAmprAmmCommandBufferMapWithGpuMaskId(buffer, mapped, size, type, protect, gpuMaskId); }, [&](void* dst, std::uint64_t* outSize) { return sceKernelWriteMapWithGpuMaskIdCommand(dst, mapped, size, type, static_cast<std::uint32_t>(protect), gpuMaskId, outSize); }, expected);
        }
        CheckAmmWriter([&](Apr::CommandBufferObject* buffer) { return sceAmprAmmCommandBufferMapDirect(buffer, mapped, offset, size, type, protect); }, [&](void* dst, std::uint64_t* outSize) { return sceKernelWriteMapDirectCommand(dst, mapped, offset, size, type, static_cast<std::uint32_t>(protect), outSize); }, expected);
        CheckAmmWriter([&](Apr::CommandBufferObject* buffer) { return sceAmprAmmCommandBufferMapDirectWithGpuMaskId(buffer, mapped, offset, size, type, protect, gpuMaskId); }, [&](void* dst, std::uint64_t* outSize) { return sceKernelWriteMapDirectWithGpuMaskIdCommand(dst, mapped, offset, size, type, static_cast<std::uint32_t>(protect), gpuMaskId, outSize); }, expected);
    }
    for (const auto [mapped, length, expected] : {std::tuple{address, size, 0}, std::tuple{address + 8, size, SCE_KERNEL_ERROR_EINVAL}, std::tuple{address, std::uint64_t{0}, SCE_KERNEL_ERROR_EINVAL}}) {
        CheckAmmWriter([&](Apr::CommandBufferObject* buffer) { return sceAmprAmmCommandBufferUnmap(buffer, mapped, length); }, [&](void* dst, std::uint64_t* outSize) { return sceKernelWriteUnmapCommand(dst, mapped, length, outSize); }, expected);
    }
    for (const auto [from, protect, expected] : {std::tuple{source, protection, 0}, std::tuple{source + 8, protection, SCE_KERNEL_ERROR_EINVAL}, std::tuple{source, badProtection, SCE_KERNEL_ERROR_EINVAL}}) {
        CheckAmmWriter([&](Apr::CommandBufferObject* buffer) { return sceAmprAmmCommandBufferRemap(buffer, address, from, size, protect); }, [&](void* dst, std::uint64_t* outSize) { return sceKernelWriteRemapCommand(dst, address, from, size, static_cast<std::uint32_t>(protect), outSize); }, expected);
        CheckAmmWriter([&](Apr::CommandBufferObject* buffer) { return sceAmprAmmCommandBufferRemapWithGpuMaskId(buffer, address, from, size, protect, gpuMaskId); }, [&](void* dst, std::uint64_t* outSize) { return sceKernelWriteRemapWithGpuMaskIdCommand(dst, address, from, size, static_cast<std::uint32_t>(protect), gpuMaskId, outSize); }, expected);
        CheckAmmWriter([&](Apr::CommandBufferObject* buffer) { return sceAmprAmmCommandBufferMultiMap(buffer, address, from, size, protect); }, [&](void* dst, std::uint64_t* outSize) { return sceKernelWriteMultiMapCommand(dst, address, from, size, static_cast<std::uint32_t>(protect), outSize); }, expected);
        CheckAmmWriter([&](Apr::CommandBufferObject* buffer) { return sceAmprAmmCommandBufferMultiMapWithGpuMaskId(buffer, address, from, size, protect, gpuMaskId); }, [&](void* dst, std::uint64_t* outSize) { return sceKernelWriteMultiMapWithGpuMaskIdCommand(dst, address, from, size, static_cast<std::uint32_t>(protect), gpuMaskId, outSize); }, expected);
    }
    for (const auto [protect, masked, expected] : {std::tuple{protection, mask, 0}, std::tuple{badProtection, mask, SCE_KERNEL_ERROR_EINVAL}, std::tuple{protection, badProtection, SCE_KERNEL_ERROR_EINVAL}}) {
        CheckAmmWriter([&](Apr::CommandBufferObject* buffer) { return sceAmprAmmCommandBufferModifyProtect(buffer, address, size, protect, masked); }, [&](void* dst, std::uint64_t* outSize) { return sceKernelWriteModifyProtectCommand(dst, address, size, static_cast<std::uint32_t>(protect), static_cast<std::uint32_t>(masked), outSize); }, expected);
        CheckAmmWriter([&](Apr::CommandBufferObject* buffer) { return sceAmprAmmCommandBufferModifyProtectWithGpuMaskId(buffer, address, size, protect, masked, gpuMaskId); }, [&](void* dst, std::uint64_t* outSize) { return sceKernelWriteModifyProtectWithGpuMaskIdCommand(dst, address, size, static_cast<std::uint32_t>(protect), static_cast<std::uint32_t>(masked), gpuMaskId, outSize); }, expected);
        CheckAmmWriter([&](Apr::CommandBufferObject* buffer) { return sceAmprAmmCommandBufferModifyMtypeProtectWithGpuMaskId(buffer, address, size, type, protect, masked, gpuMaskId); }, [&](void* dst, std::uint64_t* outSize) { return sceKernelWriteModifyMtypeProtectWithGpuMaskIdCommand(dst, address, size, type, static_cast<std::uint32_t>(protect), static_cast<std::uint32_t>(masked), gpuMaskId, outSize); }, expected);
    }
    std::uint64_t untouched = ~std::uint64_t{0};
    constexpr std::uint64_t wide = std::uint64_t{1} << 32 | 0x33;
    Require(sceKernelWriteMapCommand(nullptr, address, size, type, wide, &untouched) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelWriteRemapCommand(nullptr, address, source, size, wide, &untouched) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelWriteModifyProtectCommand(nullptr, address, size, protection, wide, &untouched) == SCE_KERNEL_ERROR_EINVAL);
    Require(untouched == ~std::uint64_t{0});
    constexpr std::uint64_t wideType = std::uint64_t{1} << 32 | static_cast<std::uint64_t>(type);
    CheckAmmWriter([&](Apr::CommandBufferObject* buffer) { return sceAmprAmmCommandBufferMap(buffer, address, size, type, protection); }, [&](void* dst, std::uint64_t* outSize) { return sceKernelWriteMapCommand(dst, address, size, wideType, static_cast<std::uint32_t>(protection), outSize); }, 0);
    CheckAmmWriter([&](Apr::CommandBufferObject* buffer) { return sceAmprAmmCommandBufferMapWithGpuMaskId(buffer, address, size, type, protection, gpuMaskId); }, [&](void* dst, std::uint64_t* outSize) { return sceKernelWriteMapWithGpuMaskIdCommand(dst, address, size, wideType, static_cast<std::uint32_t>(protection), gpuMaskId, outSize); }, 0);
    CheckAmmWriter([&](Apr::CommandBufferObject* buffer) { return sceAmprAmmCommandBufferMapDirect(buffer, address, directOffset, size, type, protection); }, [&](void* dst, std::uint64_t* outSize) { return sceKernelWriteMapDirectCommand(dst, address, directOffset, size, wideType, static_cast<std::uint32_t>(protection), outSize); }, 0);
    CheckAmmWriter([&](Apr::CommandBufferObject* buffer) { return sceAmprAmmCommandBufferMapDirectWithGpuMaskId(buffer, address, directOffset, size, type, protection, gpuMaskId); }, [&](void* dst, std::uint64_t* outSize) { return sceKernelWriteMapDirectWithGpuMaskIdCommand(dst, address, directOffset, size, wideType, static_cast<std::uint32_t>(protection), gpuMaskId, outSize); }, 0);
    CheckAmmWriter([&](Apr::CommandBufferObject* buffer) { return sceAmprAmmCommandBufferModifyMtypeProtectWithGpuMaskId(buffer, address, size, type, protection, mask, gpuMaskId); }, [&](void* dst, std::uint64_t* outSize) { return sceKernelWriteModifyMtypeProtectWithGpuMaskIdCommand(dst, address, size, wideType, static_cast<std::uint32_t>(protection), static_cast<std::uint32_t>(mask), gpuMaskId, outSize); }, 0);
}

void CheckAmmWaitCompletion() {
    constexpr std::uint64_t page = 0x4000;
    constexpr std::int32_t cpuReadWrite = 0x03;
    std::uint64_t start = 0;
    std::uint64_t end = 0;
    std::uint64_t multimapStart = 0;
    std::uint64_t multimapEnd = 0;
    Require(sceAmprAmmGetVirtualAddressRanges(&start, &end, &multimapStart, &multimapEnd) == 0);
    std::int64_t pool = -1;
    Require(sceAmprAmmGiveDirectMemory(0, static_cast<std::int64_t>(sceKernelGetDirectMemorySize()), 2 * page, page, 1, &pool) == 0);
    const std::uint64_t target = start + 0x400000;
    std::array<std::uint8_t, 64> commands{};
    std::uint64_t bytes = 0;
    Require(sceKernelWriteMapCommand(commands.data(), target, page, 0, cpuReadWrite, &bytes) == 0);
    std::uint32_t id = 0;
    Require(sceAmprAmmSubmitCommandBuffer3(commands.data(), static_cast<std::uint32_t>(bytes), 0, &id) == 0 && id != 0);
    Require(sceKernelWaitCommandBufferCompletion(id) == 0);
    auto* mapped = reinterpret_cast<volatile std::uint8_t*>(target);
    *mapped = 0x5A;
    Require(*mapped == 0x5A);
    Require(sceKernelWaitCommandBufferCompletion(id) == 0);
    Require(sceKernelWaitCommandBufferCompletion(0) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelWaitCommandBufferCompletion(id + 1) == SCE_KERNEL_ERROR_ESRCH && sceAmprAmmWaitCommandBufferCompletion(id + 1) == SCE_KERNEL_ERROR_ESRCH);
}

void CheckAmmWaitConcurrentSubmits() {
    constexpr int submitters = 4;
    constexpr int submissions = 4000;
    std::array<std::uint8_t, 16> empty{};
    std::atomic<std::uint32_t> newest{0};
    std::atomic<int> running{submitters};
    std::atomic<int> rejected{0};
    std::vector<std::thread> threads;
    for (int thread = 0; thread < submitters; ++thread) {
        threads.emplace_back([&] {
            for (int index = 0; index < submissions; ++index) {
                std::uint32_t id = 0;
                if (sceAmprAmmSubmitCommandBuffer3(empty.data(), 0, 0, &id) != 0 || id == 0) ++rejected;
                auto seen = newest.load();
                while (seen < id && !newest.compare_exchange_weak(seen, id)) {}
            }
            --running;
        });
    }
    std::thread waiter([&] {
        while (running.load() != 0) {
            const auto id = newest.load();
            if (id != 0 && (sceKernelWaitCommandBufferCompletion(id) != 0 || sceAmprAmmWaitCommandBufferCompletion(id) != 0)) ++rejected;
        }
    });
    for (auto& thread : threads) thread.join();
    waiter.join();
    Require(rejected.load() == 0);
    Require(sceKernelWaitCommandBufferCompletion(newest.load()) == 0);
}

void CheckPathconf(const std::filesystem::path& root, const std::string& file) {
    const auto directory = root.generic_string();
    for (const auto& path : {file, directory}) {
        Require(pathconf_nid_postfix(path.c_str(), 1) == 32767);
        Require(pathconf_nid_postfix(path.c_str(), 4) == 255);
        Require(pathconf_nid_postfix(path.c_str(), 5) == 1024);
        Require(pathconf_nid_postfix(path.c_str(), 6) == 512);
        Require(pathconf_nid_postfix(path.c_str(), 7) == 1);
        Require(pathconf_nid_postfix(path.c_str(), 8) == 1);
        Require(pathconf_nid_postfix(path.c_str(), 12) == 64);
        *__error_nid_postfix() = 0;
        Require(pathconf_nid_postfix(path.c_str(), 15) == -1 && Error() == 0);
        Require(pathconf_nid_postfix(path.c_str(), 18) == 1024);
        Require(pathconf_nid_postfix(path.c_str(), 54) == 0);
        Require(pathconf_nid_postfix(path.c_str(), 55) == 0);
        for (const int name : {10, 14, 16, 21}) Require(pathconf_nid_postfix(path.c_str(), name) == 65536);
        Require(pathconf_nid_postfix(path.c_str(), 17) == 16384);
        Require(pathconf_nid_postfix(path.c_str(), 60) == 3);
        for (const int name : {59, 63, 64}) Require(pathconf_nid_postfix(path.c_str(), name) == 0);
    }
    for (const int name : {0, 2, 3, 9, 11, 53, 61, 62, 65, -1})
        Require(pathconf_nid_postfix(file.c_str(), name) == -1 && Error() == 22);
    const auto missing = (root / "missing").string();
    Require(pathconf_nid_postfix(missing.c_str(), 4) == -1 && Error() == 2);
    Require(pathconf_nid_postfix(missing.c_str(), 0) == -1 && Error() == 2);
    Require(pathconf_nid_postfix((root / "missing" / "file").string().c_str(), 4) == -1 && Error() == 2);
    Require(pathconf_nid_postfix("", 4) == -1 && Error() == 2);
    Require(pathconf_nid_postfix(nullptr, 4) == -1 && Error() == 14);
    const auto longName = directory + "/" + std::string(256, 'n');
    Require(pathconf_nid_postfix(longName.c_str(), 4) == -1 && Error() == 63);
    Require(pathconf_nid_postfix((directory + "/missing/" + std::string(256, 'n')).c_str(), 4) == -1 && Error() == 2);
    const auto longPath = directory + "/" + std::string(1024, 'p');
    Require(pathconf_nid_postfix(longPath.c_str(), 4) == -1 && Error() == 63);
}

void CheckSettimeofday() {
    KernelTimezone zone{-1, -1};
    Require(settimeofday_nid_postfix(nullptr, nullptr) == 0);
    Require(gettimeofday_nid_postfix(nullptr, &zone) == 0 && zone.tz_minuteswest == 0 && zone.tz_dsttime == 0);
    const KernelTimezone west{60, 1};
    const auto unreadableTime = reinterpret_cast<const KernelTimeval*>(1);
    const auto unreadableZone = reinterpret_cast<const KernelTimezone*>(1);
    Require(settimeofday_nid_postfix(unreadableTime, &west) == -1 && Error() == 14);
    Require(settimeofday_nid_postfix(unreadableTime, unreadableZone) == -1 && Error() == 14);
    Require(settimeofday_nid_postfix(nullptr, unreadableZone) == -1 && Error() == 14);
    for (const KernelTimeval invalid : {KernelTimeval{0, 1000000}, KernelTimeval{0, -1}, KernelTimeval{-1, 0}}) {
        Require(settimeofday_nid_postfix(&invalid, &west) == -1 && Error() == 22);
        Require(settimeofday_nid_postfix(&invalid, unreadableZone) == -1 && Error() == 14);
    }
    Require(gettimeofday_nid_postfix(nullptr, &zone) == 0 && zone.tz_minuteswest == 0 && zone.tz_dsttime == 0);
    const KernelTimeval valid{1000000000, 0};
    Require(settimeofday_nid_postfix(&valid, unreadableZone) == -1 && Error() == 14);
    bool rejected = false;
    try {
        settimeofday_nid_postfix(&valid, &west);
    } catch (const std::runtime_error& error) {
        rejected = std::strstr(error.what(), "settimeofday: setting the time to 1000000000.000000") != nullptr;
    }
    Require(rejected);
    Require(gettimeofday_nid_postfix(nullptr, &zone) == 0 && zone.tz_minuteswest == 0 && zone.tz_dsttime == 0);
    Require(settimeofday_nid_postfix(nullptr, &west) == 0);
    KernelTimeval now{};
    Require(gettimeofday_nid_postfix(&now, &zone) == 0 && zone.tz_minuteswest == 60 && zone.tz_dsttime == 1 && now.tv_sec > 0);
    const KernelTimezone zero{0, 0};
    Require(settimeofday_nid_postfix(nullptr, &zero) == 0);
    Require(gettimeofday_nid_postfix(nullptr, &zone) == 0 && zone.tz_minuteswest == 0 && zone.tz_dsttime == 0);
}

void DirectoryRenameTests(const std::filesystem::path& parent) {
    const auto fixture = parent / "directory-identity";
    const auto original = fixture / "original";
    const auto moved = fixture / "moved";
    const auto twice = fixture / "twice";
    Require(std::filesystem::create_directory(fixture));
    Require(std::filesystem::create_directory(original));
    { std::ofstream(original / "marker").put('o'); }
    { std::ofstream(original / "old-only").put('a'); }
    const auto readMarker = [](int directory) {
        const int file = openat_nid_postfix(directory, "marker", SCE_KERNEL_O_RDONLY);
        Require(file >= 0);
        char marker = 0;
        Require(sceKernelRead(file, &marker, 1) == 1);
        Require(sceKernelClose(file) == 0);
        return marker;
    };
    const auto names = [](int directory) {
        std::set<std::string> result;
        std::array<char, 4096> buffer{};
        for (;;) {
            const int bytes = sceKernelGetdents(directory, buffer.data(), buffer.size());
            Require(bytes >= 0);
            if (bytes == 0) return result;
            for (int offset = 0; offset < bytes;) {
                std::uint16_t length = 0;
                std::memcpy(&length, buffer.data() + offset + 4, sizeof(length));
                Require(length >= 12 && offset + length <= bytes);
                result.emplace(buffer.data() + offset + 8);
                offset += length;
            }
        }
    };
    const int first = sceKernelOpen(original.string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(first >= 0 && readMarker(first) == 'o');
    FileStat before{};
    Require(sceKernelFstat(first, &before) == 0 && (before.st_mode & 0170000) == 0040000);
    const int duplicate = dup_nid_postfix(first);
    const int target = sceKernelOpen((original / "marker").string().c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(duplicate >= 0 && target >= 0);
    Require(dup2_nid_postfix(first, target) == target);
    Require(sceKernelClose(first) == 0);
    Require(sceKernelRename(original.string().c_str(), moved.string().c_str()) == 0);
    Require(std::filesystem::create_directory(original));
    { std::ofstream(original / "marker").put('r'); }
    { std::ofstream(original / "new-only").put('b'); }
    const char marker = readMarker(duplicate);
    std::fprintf(stderr, "directory marker after rename/replacement: %c, expected o\n", marker);
    Require(marker == 'o' && readMarker(target) == 'o');
    const std::set<std::string> expected{".", "..", "marker", "old-only"};
    Require(names(duplicate) == expected);
    Require(names(target).empty());
    FileStat after{};
    Require(sceKernelFstat(target, &after) == 0 && after.st_dev == before.st_dev && after.st_ino == before.st_ino);
    Require(sceKernelLseek(target, 0, 0) == 0);
    char directoryByte = 0;
    Require(sceKernelRead(target, &directoryByte, 1) == static_cast<int>(0x80020015u));
    Require(sceKernelPread(target, &directoryByte, 1, 0) == static_cast<int>(0x80020015u));
    char previous[4096];
    Require(getcwd_nid_postfix(previous, sizeof(previous)) == previous);
    Require(fchdir_nid_postfix(target) == 0 && readMarker(-100) == 'o');
    Require(chdir_nid_postfix(previous) == 0);
    Require(mkdirat_nid_postfix(target, "created", 0755) == 0);
    Require(std::filesystem::is_directory(moved / "created") && !std::filesystem::exists(original / "created"));
    Require(unlinkat_nid_postfix(duplicate, "created", 0x800) == 0);
    Require(sceKernelRename(moved.string().c_str(), twice.string().c_str()) == 0);
    Require(std::filesystem::create_directory(moved));
    { std::ofstream(moved / "different").put('c'); }
    Require(sceKernelLseek(target, 0, 0) == 0 && names(target) == expected);
    Require(sceKernelClose(duplicate) == 0);
    Require(readMarker(target) == 'o');
    Require(unlinkat_nid_postfix(target, "marker", 0) == 0);
    Require(unlinkat_nid_postfix(target, "old-only", 0) == 0);
    Require(std::filesystem::remove(twice));
    Require(sceKernelFstat(target, &after) == 0 && after.st_dev == before.st_dev && after.st_ino == before.st_ino && (after.st_mode & 0170000) == 0040000);
    Require(sceKernelClose(target) == 0);
    Require(std::filesystem::remove(original / "marker") && std::filesystem::remove(original / "new-only"));
    Require(std::filesystem::remove(moved / "different"));
    Require(std::filesystem::remove(original) && std::filesystem::remove(moved) && std::filesystem::remove(fixture));
}

static void CheckProcessQueries() {
    KernelTitleWorkaround workaround{1, 0, {(1ull << 3) | (1ull << 0x39), ~0ull}};
    std::int32_t enabled = -1;
    Require(sceKernelTitleWorkaroundIsEnabled(nullptr, 3, &enabled) == static_cast<int>(0x8002000E));
    Require(sceKernelTitleWorkaroundIsEnabled(&workaround, 3, nullptr) == static_cast<int>(0x8002000E));
    Require(sceKernelTitleWorkaroundIsEnabled(&workaround, 0x3A, &enabled) == static_cast<int>(0x80020016) && enabled == -1);
    Require(sceKernelTitleWorkaroundIsEnabled(&workaround, -1, &enabled) == static_cast<int>(0x80020016) && enabled == -1);
    Require(sceKernelTitleWorkaroundIsEnabled(&workaround, 3, &enabled) == 0 && enabled == 1);
    Require(sceKernelTitleWorkaroundIsEnabled(&workaround, 4, &enabled) == 0 && enabled == 0);
    Require(sceKernelTitleWorkaroundIsEnabled(&workaround, 63, &enabled) == static_cast<int>(0x80020016));
    Require(sceKernelTitleWorkaroundIsEnabled(&workaround, 0x39, &enabled) == 0 && enabled == 1);
    Require(sceKernelTitleWorkaroundIsEnabled(&workaround, 0x38, &enabled) == 0 && enabled == 0);
    Require(sceKernelGetProcessType(getpid_nid_postfix()) == 0);
    Require(sceKernelGetProcessType(-1) == 0 && sceKernelGetProcessType(0) == 0);
    Require(sceKernelGetProcessType(getpid_nid_postfix() + 1) == static_cast<int>(0x80020003));
}

template <typename F>
void CheckDescriptorPathconf(F fn, int descriptor) {
    Require(fn(descriptor, 1) == 32767);
    Require(fn(descriptor, 4) == 255);
    Require(fn(descriptor, 5) == 1024);
    Require(fn(descriptor, 6) == 512);
    Require(fn(descriptor, 7) == 1);
    Require(fn(descriptor, 8) == 1);
    Require(fn(descriptor, 12) == 64);
    *__error_nid_postfix() = 0;
    Require(fn(descriptor, 15) == -1 && Error() == 0);
    Require(fn(descriptor, 18) == 1024);
    Require(fn(descriptor, 54) == 0);
    Require(fn(descriptor, 55) == 0);
    for (const int name : {10, 14, 16, 21}) Require(fn(descriptor, name) == 65536);
    Require(fn(descriptor, 17) == 16384);
    Require(fn(descriptor, 60) == 3);
    for (const int name : {59, 63, 64}) Require(fn(descriptor, name) == 0);
    for (const int name : {0, 2, 3, 9, 11, 61, 62, 65, -1})
        Require(fn(descriptor, name) == -1 && Error() == 22);
    Require(fn(descriptor, 53) == 200112);
    Require(fn(-1, 4) == -1 && Error() == 9);
    Require(fn(descriptor + 1000, 4) == -1 && Error() == 9);
}

template <typename F>
void CheckPipePathconf(F fn) {
    int ends[2]{-1, -1};
    Require(pipe_nid_postfix(ends) == 0);
    for (const int end : ends) {
        Require(fn(end, 6) == 512);
        Require(fn(end, 53) == 200112);
        for (const int name : {0, 1, 4, 5, 7, 8, 12, 18, 54, 55, 61, -1})
            Require(fn(end, name) == -1 && Error() == 22);
    }
    Require(close_nid_postfix(ends[0]) == 0 && close_nid_postfix(ends[1]) == 0);
}

int main() {
#ifdef _WIN32
    auto hostDirectorySpelling = std::filesystem::current_path().native();
    CharUpperBuffW(hostDirectorySpelling.data(), static_cast<DWORD>(hostDirectorySpelling.size()));
    Require(SetCurrentDirectoryW(hostDirectorySpelling.c_str()) != 0);
#endif
    CheckProcessQueries();
    Require(setsid_nid_postfix() == -1 && Error() == 1);
    Require(profil_nid_postfix(nullptr, 0, 0, (1u << 16) + 1) == -1 && Error() == 22);
    Require(profil_nid_postfix(nullptr, 0, 0, 0xFFFFFFFFu) == -1 && Error() == 22);
    Require(profil_nid_postfix(reinterpret_cast<void*>(0x10), 4096, 0, 0) == 0);
    Require(profil_nid_postfix(nullptr, 0, 0, 0) == 0);
    Require(umask_nid_postfix(0) == 022);
    Require(umask_nid_postfix(07777) == 0);
    Require(umask_nid_postfix(022) == 07777);
    CheckCondattrClock();
    CheckAmmWriteCommands();
    CheckAmmWaitCompletion();
    CheckAmmWaitConcurrentSubmits();
    CheckSchedSetscheduler();
    CheckSigreturn();
    const auto root = std::filesystem::path("anyps5-kernel-sys-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    DirectoryRenameTests(root);
    OwnershipAndLinkTests(root);
    const auto file = (root / "file.txt").string();
    std::ofstream(file).put('x');
    CheckPathconf(root, file);
    const int descriptor = open_nid_postfix(file.c_str(), 0, 0);
    Require(descriptor >= 0);
    KernelStatfs publicStats{};
    auto* viaPublic = reinterpret_cast<unsigned char*>(&publicStats);
    KernelStatfs syscallStats{};
    auto* viaSyscall = reinterpret_cast<unsigned char*>(&syscallStats);
    Require(fstatfs_nid_postfix(descriptor, reinterpret_cast<KernelStatfs*>(viaPublic)) == 0 && _fstatfs_nid_postfix(descriptor, reinterpret_cast<KernelStatfs*>(viaSyscall)) == 0);
    Require(std::memcmp(viaPublic, viaSyscall, 4) == 0 && std::memcmp(viaPublic + 16, viaSyscall + 16, 8) == 0);
    Require(std::memcmp(viaPublic + 280, viaSyscall + 280, 16) == 0);
    Require(fstatfs_nid_postfix(descriptor, nullptr) == -1 && Error() == 14);
    FileStat viaFstat{};
    FileStat viaUnderscore{};
    Require(fstat_nid_postfix(descriptor, &viaFstat) == 0 && _fstat_nid_postfix(descriptor, &viaUnderscore) == 0);
    Require(std::memcmp(&viaFstat, &viaUnderscore, sizeof(FileStat)) == 0 && viaFstat.st_size == 1);
    Require(fstat_nid_postfix(descriptor, nullptr) == -1 && Error() == 14);
    CheckDescriptorPathconf(fpathconf_nid_postfix, descriptor);
    CheckDescriptorPathconf(_fpathconf_nid_postfix, descriptor);
    CheckPipePathconf(fpathconf_nid_postfix);
    CheckPipePathconf(_fpathconf_nid_postfix);
    Require(close_nid_postfix(descriptor) == 0);
    Require(fstatfs_nid_postfix(-1, reinterpret_cast<KernelStatfs*>(viaPublic)) == -1 && Error() == 9);
    Require(fstat_nid_postfix(descriptor, &viaFstat) == -1 && Error() == 9);
    Require(fstat_nid_postfix(-1, &viaFstat) == -1 && Error() == 9);
    Require(fpathconf_nid_postfix(descriptor, 4) == -1 && Error() == 9);
    Require(_fpathconf_nid_postfix(descriptor, 4) == -1 && Error() == 9);
    const int dirDescriptor = open_nid_postfix(root.generic_string().c_str(), 0, 0);
    Require(dirDescriptor >= 0);
    CheckDescriptorPathconf(fpathconf_nid_postfix, dirDescriptor);
    CheckDescriptorPathconf(_fpathconf_nid_postfix, dirDescriptor);
    Require(close_nid_postfix(dirDescriptor) == 0);
    char buffer[64];
    for (const auto& component : {std::string("missing"), std::string("file.txt")}) {
        const auto prefix = root.generic_string() + "/" + component + "/../";
        const auto invalid = prefix + "file.txt";
        const auto created = prefix + "new.txt";
        const int expected = component == "missing" ? 2 : 20;
        Require(readlink_nid_postfix(invalid.c_str(), buffer, sizeof(buffer)) == -1 && Error() == expected);
        Require(link_nid_postfix(invalid.c_str(), created.c_str()) == -1 && Error() == expected);
        Require(link_nid_postfix(file.c_str(), created.c_str()) == -1 && Error() == expected);
        Require(symlink_nid_postfix("file.txt", created.c_str()) == -1 && Error() == expected);
        Require(std::filesystem::hard_link_count(file) == 1 && !std::filesystem::exists(root / "new.txt"));
    }
    for (const std::size_t length : {std::size_t{1024}, std::size_t{1100}}) {
        const auto tooLong = std::string(length, 'a');
        const auto created = (root / "target-too-long").string();
        Require(symlink_nid_postfix(tooLong.c_str(), created.c_str()) == -1 && Error() == 63);
        Require(!std::filesystem::exists(std::filesystem::symlink_status(root / "target-too-long")));
    }
    {
        const auto longest = std::string(1023, 'a');
        const auto created = (root / "target-longest").string();
        const int made = symlink_nid_postfix(longest.c_str(), created.c_str());
        Require(made == 0 || Error() != 63);
        if (made == 0) Require(RemoveSymlink(created));
    }
    Require(readlink_nid_postfix(file.c_str(), buffer, sizeof(buffer)) == -1 && Error() == 22);
    Require(readlink_nid_postfix(root.string().c_str(), buffer, sizeof(buffer)) == -1 && Error() == 22);
    Require(readlink_nid_postfix((root / "missing").string().c_str(), buffer, sizeof(buffer)) == -1 && Error() == 2);
    Require(readlink_nid_postfix((root / "file.txt" / "child").string().c_str(), buffer, sizeof(buffer)) == -1 && Error() == 20);
    Require(readlink_nid_postfix("", buffer, sizeof(buffer)) == -1 && Error() == 2);
    Require(readlink_nid_postfix(nullptr, buffer, sizeof(buffer)) == -1 && Error() == 14);
    constexpr auto hugeSize = static_cast<std::size_t>(-1);
    Require(readlink_nid_postfix(nullptr, buffer, hugeSize) == -1 && Error() == 22);
    Require(readlink_nid_postfix("", buffer, hugeSize) == -1 && Error() == 22);
    Require(readlink_nid_postfix((root / "missing").string().c_str(), buffer, hugeSize) == -1 && Error() == 22);
    Require(readlinkat_nid_postfix(-100, nullptr, buffer, hugeSize) == -1 && Error() == 22);
    Require(readlinkat_nid_postfix(-100, "", buffer, hugeSize) == -1 && Error() == 22);
    Require(readlinkat_nid_postfix(-100, nullptr, buffer, sizeof(buffer)) == -1 && Error() == 14);
#ifndef _WIN32
    Require(readlink_nid_postfix((root / std::string(300, 'n')).string().c_str(), buffer, sizeof(buffer)) == -1 && Error() == 63);
#else
    const auto setReparseTag = [](const std::filesystem::path& path, DWORD tag) {
        const HANDLE handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
            FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        Require(handle != INVALID_HANDLE_VALUE);
        std::array<std::byte, REPARSE_GUID_DATA_BUFFER_HEADER_SIZE + 4> reparseData{};
        REPARSE_GUID_DATA_BUFFER reparseHeader{};
        reparseHeader.ReparseTag = tag;
        reparseHeader.ReparseDataLength = 4;
        reparseHeader.ReparseGuid = {0x6a1e3d0b, 0x1f6c, 0x4c1e, {0x9b, 0x52, 0x3d, 0x8e, 0x41, 0x07, 0x5c, 0x2a}};
        std::memcpy(reparseData.data(), &reparseHeader, REPARSE_GUID_DATA_BUFFER_HEADER_SIZE);
        DWORD returned = 0;
        const bool set = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, reparseData.data(), static_cast<DWORD>(reparseData.size()),
            nullptr, 0, &returned, nullptr) != 0;
        if (!set) Require(GetLastError() == ERROR_PRIVILEGE_NOT_HELD || GetLastError() == ERROR_ACCESS_DENIED);
        Require(CloseHandle(handle) != 0);
        if (!set) {
            Require(std::filesystem::remove(path));
            std::fprintf(stderr, "Windows reparse point checks skipped: setting a reparse point on a file needs rights this user lacks\n");
            return false;
        }
        Require((GetFileAttributesW(path.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) != 0);
        return true;
    };
    const auto clearReparseTag = [](const std::filesystem::path& path, DWORD tag) {
        const HANDLE handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
            FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        Require(handle != INVALID_HANDLE_VALUE);
        REPARSE_GUID_DATA_BUFFER reparseHeader{};
        reparseHeader.ReparseTag = tag;
        reparseHeader.ReparseGuid = {0x6a1e3d0b, 0x1f6c, 0x4c1e, {0x9b, 0x52, 0x3d, 0x8e, 0x41, 0x07, 0x5c, 0x2a}};
        DWORD returned = 0;
        Require(DeviceIoControl(handle, FSCTL_DELETE_REPARSE_POINT, &reparseHeader, REPARSE_GUID_DATA_BUFFER_HEADER_SIZE,
            nullptr, 0, &returned, nullptr) != 0);
        Require(CloseHandle(handle) != 0 && std::filesystem::remove(path));
    };
    const auto tagged = root / "tagged.bin";
    std::ofstream(tagged, std::ios::binary) << "abc";
    if (setReparseTag(tagged, 0x00000ABC)) {
        Require(readlink_nid_postfix(tagged.string().c_str(), buffer, sizeof(buffer)) == -1 && Error() == 22);
        clearReparseTag(tagged, 0x00000ABC);
    }
    const auto surrogate = root / "surrogate.bin";
    std::ofstream(surrogate, std::ios::binary) << "abc";
    if (setReparseTag(surrogate, 0x20000ABC)) {
        Require(readlink_nid_postfix(surrogate.string().c_str(), buffer, sizeof(buffer)) == -1 && Error() == 22);
        FileStat surrogateStatus{};
        Require(lstat_nid_postfix(surrogate.string().c_str(), &surrogateStatus) == 0);
        Require(surrogateStatus.st_mode == 0100666 && surrogateStatus.st_nlink == 1);
        clearReparseTag(surrogate, 0x20000ABC);
    }
#endif
    std::error_code error;
    const auto link = (root / "link").string();
#ifdef _WIN32
    if (!CreateSymbolicLinkW(std::filesystem::path(link).c_str(), L"sub\\target.txt", SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)) {
        error = std::error_code(GetLastError(), std::system_category());
    }
#else
    std::filesystem::create_symlink("sub/target.txt", link, error);
#endif
    if (!error) {
        std::memset(buffer, '#', sizeof(buffer));
        Require(readlink_nid_postfix(link.c_str(), buffer, sizeof(buffer)) == 14);
        Require(std::memcmp(buffer, "sub/target.txt", 14) == 0 && buffer[14] == '#');
        std::memset(buffer, '#', sizeof(buffer));
        Require(readlink_nid_postfix(link.c_str(), buffer, 3) == 3);
        Require(std::memcmp(buffer, "sub", 3) == 0 && buffer[3] == '#');
        Require(readlink_nid_postfix(link.c_str(), nullptr, 0) == 0);
        Require(readlink_nid_postfix(link.c_str(), nullptr, sizeof(buffer)) == -1 && Error() == 14);
        Require(readlink_nid_postfix((root / "LINK").string().c_str(), buffer, sizeof(buffer)) == 14);
        const int linkDir = sceKernelOpen(root.string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
        Require(linkDir >= 0);
        std::memset(buffer, '#', sizeof(buffer));
        Require(readlinkat_nid_postfix(linkDir, "link", buffer, sizeof(buffer)) == 14);
        Require(std::memcmp(buffer, "sub/target.txt", 14) == 0 && buffer[14] == '#');
        Require(readlinkat_nid_postfix(linkDir, "link", buffer, 3) == 3 && std::memcmp(buffer, "sub", 3) == 0);
        Require(readlinkat_nid_postfix(linkDir, "link", nullptr, 0) == 0);
        Require(readlinkat_nid_postfix(linkDir, "link", nullptr, sizeof(buffer)) == -1 && Error() == 14);
        const auto guestLink = "/" + root.filename().string() + "/link";
        Require(readlinkat_nid_postfix(-1, guestLink.c_str(), buffer, sizeof(buffer)) == 14 && std::memcmp(buffer, "sub/target.txt", 14) == 0);
        Require(readlinkat_nid_postfix(-100, link.c_str(), buffer, sizeof(buffer)) == 14 && std::memcmp(buffer, "sub/target.txt", 14) == 0);
        Require(sceKernelClose(linkDir) == 0);
#ifndef _WIN32
        std::filesystem::create_symlink("loopB", root / "loopA");
        std::filesystem::create_symlink("loopA", root / "loopB");
        Require(readlink_nid_postfix((root / "loopA" / "inner").string().c_str(), buffer, sizeof(buffer)) == -1 && Error() == 62);
#endif
    } else {
        std::fprintf(stderr, "symbolic links unavailable: the link cases not tested\n");
    }
    const auto original = (root / "original.bin").string();
    const auto linked = (root / "linked.bin").string();
    std::ofstream(original) << "abc";
    Require(link_nid_postfix(original.c_str(), linked.c_str()) == 0);
    Require(std::filesystem::hard_link_count(original) == 2 && std::filesystem::file_size(linked) == 3);
    std::ofstream(linked, std::ios::trunc).close();
    Require(std::filesystem::file_size(original) == 0);
    Require(link_nid_postfix(original.c_str(), linked.c_str()) == -1 && Error() == 17);
    Require(link_nid_postfix((root / "missing").string().c_str(), (root / "other").string().c_str()) == -1 && Error() == 2);
    Require(link_nid_postfix(original.c_str(), (root / "missing" / "linked.bin").string().c_str()) == -1 && Error() == 2);
    Require(link_nid_postfix(root.string().c_str(), (root / "directory").string().c_str()) == -1 && Error() == 1);
    Require(link_nid_postfix(nullptr, linked.c_str()) == -1 && Error() == 14);
    Require(link_nid_postfix(original.c_str(), "") == -1 && Error() == 2);
    Require(std::filesystem::remove(linked) && std::filesystem::hard_link_count(original) == 1);
    CheckSettimeofday();
    const int directory = sceKernelOpen(root.string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(directory >= 0);
    const int fileDescriptor = sceKernelOpen(file.c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(fileDescriptor >= 0);
    Require(fchdir_nid_postfix(-1) == -1 && Error() == 9);
    Require(fchdir_nid_postfix(fileDescriptor) == -1 && Error() == 20);
    int pipeEnds[2] = {-1, -1};
    Require(pipe_nid_postfix(pipeEnds) == 0);
    Require(fchdir_nid_postfix(pipeEnds[0]) == -1 && Error() == 22);
    Require(fchdir_nid_postfix(pipeEnds[1]) == -1 && Error() == 22);
    Require(close_nid_postfix(pipeEnds[0]) == 0 && close_nid_postfix(pipeEnds[1]) == 0);
    const int characterDevice = open_nid_postfix("/dev/random", 0, 0);
    Require(characterDevice >= 0);
    Require(fchdir_nid_postfix(characterDevice) == -1 && Error() == 20);
    Require(close_nid_postfix(characterDevice) == 0);
    const int socketDescriptor = socket_nid_postfix(2, 2, 0);
    Require(socketDescriptor >= 0);
    Require(fchdir_nid_postfix(socketDescriptor) == -1 && Error() == 22);
    Require(close_nid_postfix(socketDescriptor) == 0);
    Require(fchdir_nid_postfix(directory) == 0);
    char directoryPath[1024];
    Require(getcwd_nid_postfix(directoryPath, sizeof(directoryPath)) == directoryPath);
    Require(std::strcmp(directoryPath, ("/" + root.filename().string()).c_str()) == 0);
#ifndef _WIN32
    Require(chdir_nid_postfix("/") == 0);
    const auto originalPermissions = std::filesystem::status(root).permissions();
    std::filesystem::permissions(root, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
    const bool searchDenied = ::access(root.c_str(), X_OK) == -1 && errno == EACCES;
    const int deniedResult = searchDenied ? fchdir_nid_postfix(directory) : 0;
    const int deniedError = Error();
    const bool cwdUnchanged = getcwd_nid_postfix(directoryPath, sizeof(directoryPath)) == directoryPath && std::strcmp(directoryPath, "/") == 0;
    std::filesystem::permissions(root, std::filesystem::perms::owner_exec);
    const int searchOnlyResult = fchdir_nid_postfix(directory);
    std::filesystem::permissions(root, originalPermissions);
    if (searchDenied) Require(deniedResult == -1 && deniedError == EACCES && cwdUnchanged);
    Require(searchOnlyResult == 0);
#endif
    Require(sceKernelClose(fileDescriptor) == 0);
    Require(sceKernelClose(directory) == 0);
    Require(fchdir_nid_postfix(directory) == -1 && Error() == 9);
    Require(chdir_nid_postfix("/") == 0);
    Require(chroot_nid_postfix(nullptr) == -1 && Error() == 14);
    Require(chroot_nid_postfix("") == -1 && Error() == 2);
    Require(chroot_nid_postfix((root / "missing").string().c_str()) == -1 && Error() == 2);
    Require(chroot_nid_postfix(file.c_str()) == -1 && Error() == 20);
    Require(chroot_nid_postfix((root / "file.txt" / "child").string().c_str()) == -1 && Error() == 20);
    Require(chroot_nid_postfix((file + "/").c_str()) == -1 && Error() == 20);
    Require(chroot_nid_postfix((root / "file.txt" / "..").string().c_str()) == -1 && Error() == 20);
    Require(chroot_nid_postfix((root / std::string(256, 'n')).string().c_str()) == -1 && Error() == 63);
    Require(chroot_nid_postfix(("/" + std::string(1024, 'p')).c_str()) == -1 && Error() == 63);
    Require(kldfind_nid_postfix(nullptr) == -1 && Error() == 14);
    Require(kldfind_nid_postfix(reinterpret_cast<const char*>(0x10)) == -1 && Error() == 14);
    Require(kldfind_nid_postfix(("/" + std::string(1024, 'p')).c_str()) == -1 && Error() == 63);
    Require(kldfind_nid_postfix("") == -1 && Error() == 2);
    Require(kldfind_nid_postfix("kernel") == -1 && Error() == 2);
    Require(kldfind_nid_postfix("missing.ko") == -1 && Error() == 2);
    Require(kldnext_nid_postfix(-1) == -1 && Error() == 2);
    Require(kldnext_nid_postfix(0) == 0);
    Require(kldnext_nid_postfix(1) == -1 && Error() == 2);
    Require(kldfirstmod_nid_postfix(-1) == -1 && Error() == 2);
    Require(kldfirstmod_nid_postfix(0) == -1 && Error() == 2);
    Require(kldfirstmod_nid_postfix(1) == -1 && Error() == 2);
    Require(kldunload_nid_postfix(-1) == -1 && Error() == 1);
    Require(kldunload_nid_postfix(0) == -1 && Error() == 1);
    Require(kldunload_nid_postfix(1) == -1 && Error() == 1);
    const std::string throughFile = (root / "file.txt" / "child").string();
    Require(pathconf_nid_postfix(throughFile.c_str(), 4) == -1 && Error() == 20);
    Require(fchmodat_nid_postfix(-100, throughFile.c_str(), 0600, 0) == -1 && Error() == 20);
    Require(symlink_nid_postfix("target", throughFile.c_str()) == -1 && Error() == 20);
    Require(link_nid_postfix(file.c_str(), throughFile.c_str()) == -1 && Error() == 20);
    Require(link_nid_postfix(throughFile.c_str(), (root / "linked").string().c_str()) == -1 && Error() == 20);
    Require(ChrootThrows(root.string()));
    Require(ChrootThrows("/" + root.filename().string() + "/."));
    const auto guestRoot = "/" + root.filename().string();
    const auto chrootFile = guestRoot + "/file.txt";
    const int top = sceKernelOpen("/", SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(top >= 0);
    Require(chdir_nid_postfix(guestRoot.c_str()) == 0);
    Require(fchdir_nid_postfix(top) == 0 && chroot_nid_postfix(".") == 0);
    Require(getcwd_nid_postfix(directoryPath, sizeof(directoryPath)) == directoryPath && std::strcmp(directoryPath, "/") == 0);
    Require(sceKernelClose(top) == 0);
    Require(chroot_nid_postfix("/") == 0 && chroot_nid_postfix("/..") == 0 && chroot_nid_postfix("//./") == 0);
    Require(chdir_nid_postfix(guestRoot.c_str()) == 0);
    Require(chroot_nid_postfix("..") == 0 && chroot_nid_postfix("../..") == 0);
    Require(getcwd_nid_postfix(directoryPath, sizeof(directoryPath)) == directoryPath && guestRoot == directoryPath);
    Require(ChrootThrows("."));
    for (const auto& path : {std::string("file.txt"), chrootFile, "/../../.." + chrootFile, "../.." + chrootFile}) {
        const int opened = sceKernelOpen(path.c_str(), SCE_KERNEL_O_RDONLY, 0);
        char byte = 0;
        Require(opened >= 0 && sceKernelRead(opened, &byte, 1) == 1 && byte == 'x');
        Require(sceKernelClose(opened) == 0);
    }
    Require(chdir_nid_postfix("/") == 0);
    Require(getcwd_nid_postfix(directoryPath, sizeof(directoryPath)) == directoryPath && std::strcmp(directoryPath, "/") == 0);
    Require(readlink_nid_postfix(file.c_str(), buffer, sizeof(buffer)) == -1 && Error() == 22);
    const auto symbolic = (root / "symbolic").string();
    const int created = symlink_nid_postfix("original.bin", symbolic.c_str());
    if (created == 0) {
        const auto dangling = (root / "dangling").string();
#ifdef _WIN32
        Require((GetFileAttributesW(std::filesystem::path(symbolic).wstring().c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) != 0);
        Require(symlink_nid_postfix("original.bin", symbolic.c_str()) == -1 && Error() == 17);
        Require(symlink_nid_postfix("missing.bin", dangling.c_str()) == 0);
        Require((GetFileAttributesW(std::filesystem::path(dangling).wstring().c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) != 0);
        Require(symlink_nid_postfix("missing.bin", dangling.c_str()) == -1 && Error() == 17);
        Require(readlink_nid_postfix(symbolic.c_str(), buffer, sizeof(buffer)) == 12 && std::memcmp(buffer, "original.bin", 12) == 0);
        Require(readlink_nid_postfix(dangling.c_str(), buffer, sizeof(buffer)) == 11 && std::memcmp(buffer, "missing.bin", 11) == 0);
        const auto nested = (root / "nested").string();
        Require(symlink_nid_postfix("sub/target.txt", nested.c_str()) == 0);
        std::memset(buffer, '#', sizeof(buffer));
        Require(readlink_nid_postfix(nested.c_str(), buffer, sizeof(buffer)) == 14);
        Require(std::memcmp(buffer, "sub/target.txt", 14) == 0 && buffer[14] == '#');
        Require(readlink_nid_postfix(nested.c_str(), buffer, 3) == 3 && std::memcmp(buffer, "sub", 3) == 0);
        Require(readlink_nid_postfix(nested.c_str(), nullptr, 0) == 0);
        Require(readlink_nid_postfix(nested.c_str(), nullptr, sizeof(buffer)) == -1 && Error() == 14);
        const auto accented = (root / "accented").string();
        Require(symlink_nid_postfix("a\xc3\xa7\xc3\xa3o.bin", accented.c_str()) == 0);
        Require(readlink_nid_postfix(accented.c_str(), buffer, sizeof(buffer)) == 10 && std::memcmp(buffer, "a\xc3\xa7\xc3\xa3o.bin", 10) == 0);
        Require(std::filesystem::create_directory(root / "sub"));
        const auto directoryLink = (root / "directory-link").string();
        Require(symlink_nid_postfix("sub", directoryLink.c_str()) == 0);
        Require(readlink_nid_postfix(directoryLink.c_str(), buffer, sizeof(buffer)) == 3 && std::memcmp(buffer, "sub", 3) == 0);
        const auto absolute = std::filesystem::absolute(root / "absolute");
        Require(CreateSymbolicLinkW(absolute.wstring().c_str(), std::filesystem::absolute(original).wstring().c_str(),
            SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != 0);
        char absoluteBuffer[512];
        std::memset(absoluteBuffer, '#', sizeof(absoluteBuffer));
        const auto absoluteLength = readlink_nid_postfix((root / "absolute").string().c_str(), absoluteBuffer, sizeof(absoluteBuffer));
        const bool absoluteFits = absoluteLength > 0 && static_cast<std::size_t>(absoluteLength) < sizeof(absoluteBuffer) &&
            absoluteBuffer[absoluteLength] == '#';
        const std::string absoluteTarget = absoluteFits ? std::string(absoluteBuffer, static_cast<std::size_t>(absoluteLength)) : std::string();
        std::error_code absoluteError;
        const bool absoluteMatches = absoluteFits && absoluteTarget.find('?') == std::string::npos &&
            std::filesystem::equivalent(absoluteTarget, original, absoluteError) && !absoluteError;
        if (!absoluteMatches)
            std::fprintf(stderr, "absolute link returned %lld bytes\n", static_cast<long long>(absoluteLength));
        Require(absoluteMatches);
#else
        Require(std::filesystem::is_symlink(std::filesystem::symlink_status(symbolic)));
        Require(readlink_nid_postfix(symbolic.c_str(), buffer, sizeof(buffer)) == 12 && std::memcmp(buffer, "original.bin", 12) == 0);
        Require(std::filesystem::file_size(symbolic) == std::filesystem::file_size(original));
        Require(symlink_nid_postfix("original.bin", symbolic.c_str()) == -1 && Error() == 17);
        Require(symlink_nid_postfix("missing.bin", dangling.c_str()) == 0);
        Require(readlink_nid_postfix(dangling.c_str(), buffer, sizeof(buffer)) == 11 && !std::filesystem::exists(dangling));
        Require(symlink_nid_postfix("missing.bin", dangling.c_str()) == -1 && Error() == 17);
#endif
    } else {
        Require(created == -1 && Error() == 1);
        std::fprintf(stderr, "symbolic links unavailable: the symlink success cases not tested\n");
    }
    Require(symlink_nid_postfix("original.bin", (root / "missing" / "symbolic").string().c_str()) == -1 && Error() == 2);
    Require(symlink_nid_postfix(nullptr, symbolic.c_str()) == -1 && Error() == 14);
    Require(symlink_nid_postfix("original.bin", "") == -1 && Error() == 2);
    CheckSymlinkTargetRestrictions(root);
    const int base = sceKernelOpen(root.string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(base >= 0);
    CheckUnreadablePaths(file, base);
    const int relative = openat_nid_postfix(base, "file.txt", SCE_KERNEL_O_RDONLY);
    char byte = 0;
    Require(relative >= 0 && sceKernelRead(relative, &byte, 1) == 1 && byte == 'x');
    Require(sceKernelClose(relative) == 0);
    const int made = openat_nid_postfix(base, "made.bin", SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL, 0644);
    Require(made >= 0 && std::filesystem::is_regular_file(root / "made.bin"));
    Require(sceKernelClose(made) == 0);
    Require(openat_nid_postfix(base, "made.bin", SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL, 0644) == -1 && Error() == 17);
    Require(openat_nid_postfix(base, "missing.bin", SCE_KERNEL_O_RDONLY) == -1 && Error() == 2);
    Require(openat_nid_postfix(base, "file.txt", SCE_KERNEL_O_ACCMODE) == -1 && Error() == 22);
    Require(openat_nid_postfix(base, "absent.bin", SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL | SCE_KERNEL_O_DIRECTORY, 0644) == -1 && Error() == 22);
    Require(openat_nid_postfix(base, nullptr, SCE_KERNEL_O_ACCMODE) == -1 && Error() == 22);
    Require(openat_nid_postfix(base, "", SCE_KERNEL_O_ACCMODE) == -1 && Error() == 22);
    Require(openat_nid_postfix(9999, "", SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL | SCE_KERNEL_O_DIRECTORY, 0644) == -1 && Error() == 22);
    Require(openat_nid_postfix(-100, nullptr, SCE_KERNEL_O_ACCMODE) == -1 && Error() == 22);
    const auto guestFile = "/" + root.filename().string() + "/file.txt";
    const int absolute = openat_nid_postfix(-1, guestFile.c_str(), SCE_KERNEL_O_RDONLY);
    Require(absolute >= 0 && sceKernelClose(absolute) == 0);
    const int current = openat_nid_postfix(-100, file.c_str(), SCE_KERNEL_O_RDONLY);
    Require(current >= 0 && sceKernelClose(current) == 0);
    const int plain = sceKernelOpen(file.c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(plain >= 0);
    Require(openat_nid_postfix(plain, "file.txt", SCE_KERNEL_O_RDONLY) == -1 && Error() == 20);
    const int aliased = _openat_nid_postfix(base, "file.txt", SCE_KERNEL_O_RDONLY);
    byte = 0;
    Require(aliased >= 0 && sceKernelRead(aliased, &byte, 1) == 1 && byte == 'x');
    Require(sceKernelClose(aliased) == 0);
    const int aliasMade = _openat_nid_postfix(base, "alias.bin", SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL, 0600);
    Require(aliasMade >= 0 && sceKernelClose(aliasMade) == 0 && std::filesystem::is_regular_file(root / "alias.bin"));
#ifndef _WIN32
    Require((std::filesystem::status(root / "alias.bin").permissions() & std::filesystem::perms::all) ==
        (std::filesystem::perms::owner_read | std::filesystem::perms::owner_write));
#endif
    const int aliasWritten = _openat_nid_postfix(base, "alias.bin", SCE_KERNEL_O_WRONLY);
    Require(aliasWritten >= 0 && sceKernelClose(aliasWritten) == 0);
    Require(_openat_nid_postfix(base, "alias.bin", SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL, 0600) == -1 && Error() == 17);
    Require(_openat_nid_postfix(base, "file.txt", SCE_KERNEL_O_ACCMODE) == -1 && Error() == 22);
    Require(_openat_nid_postfix(-1, "file.txt", SCE_KERNEL_O_RDONLY) == -1 && Error() == 9);
    Require(_openat_nid_postfix(plain, "file.txt", SCE_KERNEL_O_RDONLY) == -1 && Error() == 20);
    Require(readlinkat_nid_postfix(base, "file.txt", buffer, sizeof(buffer)) == -1 && Error() == 22);
    Require(readlinkat_nid_postfix(base, "missing.bin", buffer, sizeof(buffer)) == -1 && Error() == 2);
    Require(readlinkat_nid_postfix(base, "file.txt/child", buffer, sizeof(buffer)) == -1 && Error() == 20);
    Require(readlinkat_nid_postfix(plain, "file.txt", buffer, sizeof(buffer)) == -1 && Error() == 20);
    Require(readlinkat_nid_postfix(-1, "file.txt", buffer, sizeof(buffer)) == -1 && Error() == 9);
    Require(readlinkat_nid_postfix(base, nullptr, buffer, sizeof(buffer)) == -1 && Error() == 14);
    Require(readlinkat_nid_postfix(base, "", buffer, sizeof(buffer)) == -1 && Error() == 2);
    Require(readlinkat_nid_postfix(-1, guestFile.c_str(), buffer, sizeof(buffer)) == -1 && Error() == 22);
    Require(readlinkat_nid_postfix(-100, file.c_str(), buffer, sizeof(buffer)) == -1 && Error() == 22);
    Require(mkdirat_nid_postfix(base, "dir", 0700) == 0 && std::filesystem::is_directory(root / "dir"));
    Require(mkdirat_nid_postfix(base, "dir", 0700) == -1 && Error() == 17);
    Require(mkdirat_nid_postfix(base, "file.txt", 0700) == -1 && Error() == 17);
    Require(mkdirat_nid_postfix(base, "missing/child", 0700) == -1 && Error() == 2);
    Require(mkdirat_nid_postfix(base, "file.txt/child", 0700) == -1 && Error() == 20);
    Require(mkdirat_nid_postfix(base, "slash_dir/", 0700) == 0 && std::filesystem::is_directory(root / "slash_dir"));
    const auto guestSubdir = "/" + root.filename().string() + "/abs_dir";
    Require(mkdirat_nid_postfix(-1, guestSubdir.c_str(), 0700) == 0 && std::filesystem::is_directory(root / "abs_dir"));
    const auto cwdDir = (root / "cwd_dir").string();
    Require(mkdirat_nid_postfix(-100, cwdDir.c_str(), 0700) == 0 && std::filesystem::is_directory(root / "cwd_dir"));
    Require(mkdirat_nid_postfix(plain, "child", 0700) == -1 && Error() == 20);
    Require(mkdirat_nid_postfix(-1, "child", 0700) == -1 && Error() == 9);
    Require(mkdirat_nid_postfix(base, nullptr, 0700) == -1 && Error() == 14);
    Require(mkdirat_nid_postfix(base, "", 0700) == -1 && Error() == 2);
    const std::string longName(256, 'a');
    Require(mkdirat_nid_postfix(base, longName.c_str(), 0700) == -1 && Error() == 63);
    Require(mkdirat_nid_postfix(base, "dir/../dotdot_dir", 0700) == 0 && std::filesystem::is_directory(root / "dotdot_dir"));
#ifdef _WIN32
    for (const DWORD linkFlags : {DWORD(0), DWORD(SYMBOLIC_LINK_FLAG_DIRECTORY)}) {
        const auto loopLink = root / "mkdirat_loop";
        if (CreateSymbolicLinkW(loopLink.c_str(), L"mkdirat_loop", linkFlags | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) == 0) {
            Require(GetLastError() == ERROR_PRIVILEGE_NOT_HELD);
            std::fprintf(stderr, "symbolic links unavailable: the mkdirat loop case not tested\n");
            break;
        }
        Require(mkdirat_nid_postfix(base, "mkdirat_loop/child", 0700) == -1 && Error() == 62);
        Require(mkdirat_nid_postfix(base, "mkdirat_loop", 0700) == -1 && Error() == 17);
        Require((linkFlags != 0 ? RemoveDirectoryW(loopLink.c_str()) : DeleteFileW(loopLink.c_str())) != 0);
    }
#endif
    Require(unlinkat_nid_postfix(plain, "file.txt", 0) == -1 && Error() == 20);
    Require(unlinkat_nid_postfix(-1, "file.txt", 0) == -1 && Error() == 9);
    Require(sceKernelClose(plain) == 0);
    Require(unlinkat_nid_postfix(base, "file.txt", 1) == -1 && Error() == 22);
    Require(unlinkat_nid_postfix(base, nullptr, 1) == -1 && Error() == 22);
    Require(unlinkat_nid_postfix(base, nullptr, 0) == -1 && Error() == 14);
    Require(unlinkat_nid_postfix(base, "", 0) == -1 && Error() == 2);
    Require(unlinkat_nid_postfix(base, "missing.bin", 0) == -1 && Error() == 2);
    Require(unlinkat_nid_postfix(base, "missing.bin", 0x800) == -1 && Error() == 2);
    Require(unlinkat_nid_postfix(base, "missing/child", 0) == -1 && Error() == 2);
    Require(unlinkat_nid_postfix(base, "file.txt/child", 0) == -1 && Error() == 20);
    Require(unlinkat_nid_postfix(base, "file.txt/child", 0x800) == -1 && Error() == 20);
    Require(unlinkat_nid_postfix(base, "file.txt/", 0) == -1 && Error() == 20);
    Require(unlinkat_nid_postfix(base, longName.c_str(), 0) == -1 && Error() == 63);
    Require(unlinkat_nid_postfix(base, "file.txt", 0x800) == -1 && Error() == 20);
    Require(unlinkat_nid_postfix(base, ".", 0x800) == -1 && Error() == 22);
    Require(unlinkat_nid_postfix(base, "..", 0x800) == -1 && Error() == 22);
    Require(unlinkat_nid_postfix(base, ".", 0) == -1 && Error() == 22);
    Require(unlinkat_nid_postfix(base, "..", 0) == -1 && Error() == 22);
    std::filesystem::create_directory(root / "usub");
    Require(unlinkat_nid_postfix(base, "usub", 0) == -1 && Error() == 1);
    std::filesystem::create_directory(root / "nonempty");
    { std::ofstream out(root / "nonempty" / "f.bin"); out << 'x'; }
    Require(unlinkat_nid_postfix(base, "nonempty", 0x800) == -1 && Error() == 66);
#ifdef _WIN32
    const auto fullJunctionTarget = root / "junction-full-target";
    Require(std::filesystem::create_directory(fullJunctionTarget));
    { std::ofstream out(fullJunctionTarget / "inside.txt"); out << 'x'; }
    const auto fullJunctionCommand = L"cmd /c mklink /J \"" + (root / "junction-full").wstring() + L"\" \"" + std::filesystem::absolute(fullJunctionTarget).wstring() + L"\"";
    Require(_wsystem(fullJunctionCommand.c_str()) == 0);
    Require(unlinkat_nid_postfix(base, "junction-full", 0x800) == -1 && Error() == 20);
    Require(std::filesystem::is_regular_file(fullJunctionTarget / "inside.txt"));
    Require(unlinkat_nid_postfix(base, "junction-full", 0) == 0);
    Require(!std::filesystem::exists(std::filesystem::symlink_status(root / "junction-full")));
    Require(std::filesystem::is_regular_file(fullJunctionTarget / "inside.txt"));
    Require(std::filesystem::remove(fullJunctionTarget / "inside.txt") && std::filesystem::remove(fullJunctionTarget));
    { std::ofstream out(root / "symlink-target.bin"); out << 'x'; }
    const int madeLink = symlink_nid_postfix("symlink-target.bin", (root / "symlink-file").string().c_str());
    if (madeLink == 0) {
        Require(unlinkat_nid_postfix(base, "symlink-file", 0) == 0);
        Require(!std::filesystem::exists(std::filesystem::symlink_status(root / "symlink-file")));
        Require(std::filesystem::is_regular_file(root / "symlink-target.bin"));
        Require(std::filesystem::remove(root / "symlink-target.bin"));
    } else {
        Require(std::filesystem::remove(root / "symlink-target.bin"));
    }
#endif
    Require(unlinkat_nid_postfix(base, "nonempty/f.bin", 0) == 0 && !std::filesystem::exists(root / "nonempty" / "f.bin"));
    Require(unlinkat_nid_postfix(base, "nonempty", 0x800) == 0 && !std::filesystem::exists(root / "nonempty"));
    { std::ofstream out(root / "to_unlink.bin"); out << 'x'; }
    Require(unlinkat_nid_postfix(base, "to_unlink.bin", 0) == 0 && !std::filesystem::exists(root / "to_unlink.bin"));
    { std::ofstream out(root / "dotdot.bin"); out << 'x'; }
    Require(unlinkat_nid_postfix(base, "usub/../dotdot.bin", 0) == 0 && !std::filesystem::exists(root / "dotdot.bin"));
    std::filesystem::create_directory(root / "uslash_sub");
    Require(unlinkat_nid_postfix(base, "uslash_sub/", 0x800) == 0 && !std::filesystem::exists(root / "uslash_sub"));
    Require(unlinkat_nid_postfix(base, "usub", 0x800) == 0 && !std::filesystem::exists(root / "usub"));
    { std::ofstream out(root / "abs.bin"); out << 'x'; }
    const auto guestAbsFile = "/" + root.filename().string() + "/abs.bin";
    Require(unlinkat_nid_postfix(-1, guestAbsFile.c_str(), 0) == 0 && !std::filesystem::exists(root / "abs.bin"));
    std::filesystem::create_directory(root / "unlink_abs_dir");
    const auto guestAbsDir = "/" + root.filename().string() + "/unlink_abs_dir";
    Require(unlinkat_nid_postfix(-1, guestAbsDir.c_str(), 0x800) == 0 && !std::filesystem::exists(root / "unlink_abs_dir"));
    { std::ofstream out(root / "cwd.bin"); out << 'x'; }
    Require(unlinkat_nid_postfix(-100, (root / "cwd.bin").string().c_str(), 0) == 0 && !std::filesystem::exists(root / "cwd.bin"));
    std::filesystem::create_directory(root / "unlink_cwd_dir");
    Require(unlinkat_nid_postfix(-100, (root / "unlink_cwd_dir").string().c_str(), 0x800) == 0 && !std::filesystem::exists(root / "unlink_cwd_dir"));
    Require(openat_nid_postfix(-1, "file.txt", SCE_KERNEL_O_RDONLY) == -1 && Error() == 9);
    Require(openat_nid_postfix(base, nullptr, SCE_KERNEL_O_RDONLY) == -1 && Error() == 14);
    Require(openat_nid_postfix(base, "", SCE_KERNEL_O_RDONLY) == -1 && Error() == 2);
    Require(renameat_nid_postfix(base, nullptr, base, "renamed.txt") == -1 && Error() == 14);
    Require(renameat_nid_postfix(base, "made.bin", base, nullptr) == -1 && Error() == 14);
    Require(renameat_nid_postfix(base, "", base, "renamed.txt") == -1 && Error() == 2);
    Require(renameat_nid_postfix(base, "made.bin", base, "") == -1 && Error() == 2);
    const int bad = sceKernelOpen(file.c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(bad >= 0);
    Require(renameat_nid_postfix(-1, "made.bin", base, "renamed.txt") == -1 && Error() == 9);
    Require(renameat_nid_postfix(base, "made.bin", -1, "renamed.txt") == -1 && Error() == 9);
    Require(renameat_nid_postfix(bad, "made.bin", base, "renamed.txt") == -1 && Error() == 20);
    Require(renameat_nid_postfix(base, "made.bin", bad, "renamed.txt") == -1 && Error() == 20);
    Require(sceKernelClose(bad) == 0);
    const auto longPart = std::string(256, 'y');
    Require(renameat_nid_postfix(base, longPart.c_str(), base, "renamed.txt") == -1 && Error() == 63);
    Require(renameat_nid_postfix(base, "made.bin", base, longPart.c_str()) == -1 && Error() == 63);
    Require(renameat_nid_postfix(base, ".", base, "renamed.txt") == -1 && Error() == 22);
    Require(renameat_nid_postfix(base, "..", base, "renamed.txt") == -1 && Error() == 22);
    Require(renameat_nid_postfix(base, "made.bin", base, ".") == -1 && Error() == 22);
    Require(renameat_nid_postfix(base, "made.bin", base, "..") == -1 && Error() == 22);
    Require(renameat_nid_postfix(base, "missing.bin", base, "renamed.txt") == -1 && Error() == 2);
    Require(renameat_nid_postfix(base, "made.bin", base, "renamed.bin") == 0);
    Require(!std::filesystem::exists(root / "made.bin"));
    Require(std::filesystem::is_regular_file(root / "renamed.bin"));
    Require(renameat_nid_postfix(base, "renamed.bin", base, "renamed.bin") == 0);
    const auto subPath = root / "sub_dir";
    std::filesystem::create_directory(subPath);
    Require(renameat_nid_postfix(base, "renamed.bin", base, "sub_dir") == -1 && Error() == 21);
    Require(renameat_nid_postfix(base, "sub_dir", base, "renamed.bin") == -1 && Error() == 20);
    Require(renameat_nid_postfix(base, "sub_dir", base, "sub_renamed") == 0);
    Require(!std::filesystem::exists(subPath));
    Require(std::filesystem::is_directory(root / "sub_renamed"));
    const int subFd = sceKernelOpen((root / "sub_renamed").string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(subFd >= 0);
    Require(renameat_nid_postfix(base, "renamed.bin", subFd, "nested.bin") == 0);
    Require(!std::filesystem::exists(root / "renamed.bin"));
    Require(std::filesystem::is_regular_file(root / "sub_renamed" / "nested.bin"));
    Require(renameat_nid_postfix(base, "sub_renamed", subFd, "child") == -1 && Error() == 22);
    const auto otherDir = root / "other_dir";
    std::filesystem::create_directory(otherDir);
    { std::ofstream stream(otherDir / "item.txt"); }
    Require(renameat_nid_postfix(base, "sub_renamed", base, "other_dir") == -1 && Error() == 66);
    Require(renameat_nid_postfix(subFd, "nested.bin/", base, "back.bin") == -1 && Error() == 20);
    Require(renameat_nid_postfix(subFd, "nested.bin", base, "nonexistent/") == -1 && Error() == 20);
    Require(renameat_nid_postfix(subFd, "nested.bin", base, "made.bin") == 0);
    Require(std::filesystem::is_regular_file(root / "made.bin"));
    Require(sceKernelClose(subFd) == 0);
    Require(renameat_nid_postfix(-100, (root / "made.bin").string().c_str(), -100, (root / "final.bin").string().c_str()) == 0);
    Require(std::filesystem::is_regular_file(root / "final.bin"));
    Require(fchmodat_nid_postfix(base, "file.txt", 0600, 0x0200) == 0);
    Require(fchmodat_nid_postfix(-100, (root / "file.txt").string().c_str(), 0644, 0) == 0);
#ifndef _WIN32
    Require((std::filesystem::status(root / "file.txt").permissions() & std::filesystem::perms::all) ==
        (std::filesystem::perms::owner_read | std::filesystem::perms::owner_write | std::filesystem::perms::group_read | std::filesystem::perms::others_read));
#endif
    Require(fchmodat_nid_postfix(base, "missing.bin", 0600, 0) == -1 && Error() == 2);
    Require(fchmodat_nid_postfix(base, nullptr, 0600, 0) == -1 && Error() == 14);
    Require(fchmodat_nid_postfix(base, "", 0600, 0) == -1 && Error() == 2);
    Require(fchmodat_nid_postfix(base, "file.txt", 0600, 0x4000) == -1 && Error() == 22);
    Require(fchmodat_nid_postfix(-1, "file.txt", 0600, 0) == -1 && Error() == 9);
#ifndef _WIN32
    Require(symlink_nid_postfix("loop", (root / "loop").string().c_str()) == 0);
    Require(fchmodat_nid_postfix(base, "loop/x", 0600, 0) == -1 && Error() == 62);
    Require(fchmodat_nid_postfix(base, longName.c_str(), 0600, 0) == -1 && Error() == 63);
#endif
    const auto modeFile = (root / "file.txt").string();
#ifndef _WIN32
    const auto ownerOnly = std::filesystem::perms::owner_read | std::filesystem::perms::owner_write;
    const auto readable = ownerOnly | std::filesystem::perms::group_read | std::filesystem::perms::others_read;
#endif
    Require(lchmod_nid_postfix(modeFile.c_str(), 0600) == 0);
#ifndef _WIN32
    Require((std::filesystem::status(modeFile).permissions() & std::filesystem::perms::all) == ownerOnly);
#endif
    Require(lchmod_nid_postfix(guestFile.c_str(), 0100644) == 0);
#ifndef _WIN32
    Require((std::filesystem::status(modeFile).permissions() & std::filesystem::perms::all) == readable);
#endif
    Require(lchmod_nid_postfix((root / "missing.bin").string().c_str(), 0600) == -1 && Error() == 2);
    Require(lchmod_nid_postfix(nullptr, 0600) == -1 && Error() == 14);
    Require(lchmod_nid_postfix("", 0600) == -1 && Error() == 2);
#ifndef _WIN32
    Require(lchmod_nid_postfix((root / "loop" / "x").string().c_str(), 0600) == -1 && Error() == 62);
    Require(lchmod_nid_postfix((root / longName).string().c_str(), 0600) == -1 && Error() == 63);
    const auto modeLink = (root / "mode-link.txt").string();
    Require(symlink_nid_postfix("file.txt", modeLink.c_str()) == 0);
#ifdef __APPLE__
    Require(lchmod_nid_postfix(modeLink.c_str(), 0600) == 0);
    Require((std::filesystem::symlink_status(modeLink).permissions() & std::filesystem::perms::all) == ownerOnly);
#else
    bool lchmodThrew = false;
    try { lchmod_nid_postfix(modeLink.c_str(), 0600); } catch (const std::exception&) { lchmodThrew = true; }
    Require(lchmodThrew);
#endif
    Require((std::filesystem::status(modeFile).permissions() & std::filesystem::perms::all) == readable);
    Require(RemoveSymlink(modeLink));
#endif
    const auto chmodTarget = (root / "chmod-target.txt").string();
    { std::ofstream file(chmodTarget); file << "x"; }
    const auto chmodLink = (root / "chmod-link.txt").string();
    const auto chmodDangling = (root / "chmod-dangling.txt").string();
    const auto chmodLoopA = (root / "chmod-loop-a").string();
    const auto chmodLoopB = (root / "chmod-loop-b").string();
    if (symlink_nid_postfix("chmod-target.txt", chmodLink.c_str()) == 0) {
        Require(symlink_nid_postfix("chmod-missing.txt", chmodDangling.c_str()) == 0);
        Require(symlink_nid_postfix("chmod-loop-b", chmodLoopA.c_str()) == 0 && symlink_nid_postfix("chmod-loop-a", chmodLoopB.c_str()) == 0);
        const auto writable = [&] {
            FileStat linked{};
            Require(stat_nid_postfix(chmodTarget.c_str(), &linked) == 0);
            return (linked.st_mode & 0222) != 0;
        };
        Require(writable());
        Require(fchmodat_nid_postfix(base, "chmod-link.txt", 0400, 0) == 0 && !writable());
        Require(fchmodat_nid_postfix(-100, chmodLink.c_str(), 0600, 0) == 0 && writable());
        Require(fchmodat_nid_postfix(base, "chmod-dangling.txt", 0600, 0) == -1 && Error() == 2);
        Require(fchmodat_nid_postfix(-100, chmodDangling.c_str(), 0600, 0) == -1 && Error() == 2);
        Require(fchmodat_nid_postfix(base, "chmod-loop-a", 0600, 0) == -1 && Error() == 62);
        Require(fchmodat_nid_postfix(base, "chmod-link.txt/child", 0600, 0) == -1 && Error() == 20);
        Require(RemoveSymlink(chmodLink) && RemoveSymlink(chmodDangling) && RemoveSymlink(chmodLoopA) && RemoveSymlink(chmodLoopB));
    }
    Require(fchown_nid_postfix(base, 0, 0) == 0);
    Require(fchown_nid_postfix(base, 0xFFFFFFFFu, 0xFFFFFFFFu) == 0);
    Require(fchown_nid_postfix(base, 0, 0xFFFFFFFFu) == 0);
    Require(fchown_nid_postfix(base, 0xFFFFFFFFu, 0) == 0);
    Require(fchown_nid_postfix(-1, 0, 0) == -1 && Error() == 9);
    Require(fchown_nid_postfix(-1, 1, 0) == -1 && Error() == 9);
    Require(fchown_nid_postfix(-1, 0, 1) == -1 && Error() == 9);
    Require(fchown_nid_postfix(-1, 1, 1) == -1 && Error() == 9);
    bool fchownThrew = false;
    try { fchown_nid_postfix(base, 1, 0); } catch (const std::exception&) { fchownThrew = true; }
    Require(fchownThrew);
    fchownThrew = false;
    try { fchown_nid_postfix(base, 0, 1); } catch (const std::exception&) { fchownThrew = true; }
    Require(fchownThrew);
    FileStat stat{};
    Require(fstatat_nid_postfix(base, "file.txt", &stat, 0) == 0 && (stat.st_mode & 0170000) == 0100000 && stat.st_size == 1);
    Require(fstatat_nid_postfix(base, "other_dir", &stat, 0) == 0 && (stat.st_mode & 0170000) == 0040000);
    Require(fstatat_nid_postfix(base, "other_dir/", &stat, 0) == 0 && (stat.st_mode & 0170000) == 0040000);
    Require(fstatat_nid_postfix(base, "file.txt/", &stat, 0) == -1 && Error() == 20);
    Require(fstatat_nid_postfix(base, "file.txt", &stat, 0x0200) == 0 && (stat.st_mode & 0170000) == 0100000);
    Require(fstatat_nid_postfix(-100, file.c_str(), &stat, 0) == 0 && stat.st_size == 1);
    Require(fstatat_nid_postfix(-1, guestFile.c_str(), &stat, 0) == 0 && stat.st_size == 1);
    Require(fstatat_nid_postfix(base, "missing.bin", &stat, 0) == -1 && Error() == 2);
    Require(fstatat_nid_postfix(base, "missing.bin", &stat, 0x0200) == -1 && Error() == 2);
    Require(fstatat_nid_postfix(base, nullptr, &stat, 0) == -1 && Error() == 14);
    Require(fstatat_nid_postfix(base, "", &stat, 0) == -1 && Error() == 2);
    Require(fstatat_nid_postfix(base, "file.txt", &stat, 0x4000) == -1 && Error() == 22);
    Require(fstatat_nid_postfix(-1, "file.txt", &stat, 0) == -1 && Error() == 9);
    const int notDirectory = sceKernelOpen(file.c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(notDirectory >= 0);
    Require(fstatat_nid_postfix(notDirectory, "file.txt", &stat, 0) == -1 && Error() == 20);
    Require(sceKernelClose(notDirectory) == 0);
#ifndef _WIN32
    Require(symlink_nid_postfix("file.txt", (root / "link.txt").string().c_str()) == 0);
    Require(fstatat_nid_postfix(base, "link.txt", &stat, 0) == 0 && (stat.st_mode & 0170000) == 0100000 && stat.st_size == 1);
    Require(fstatat_nid_postfix(base, "link.txt", &stat, 0x0200) == 0 && (stat.st_mode & 0170000) == 0120000);
#endif
    Require(sceKernelClose(base) == 0);
#ifdef _WIN32
    const auto junctionTarget = root / "junction-target";
    const auto junction = root / "junction";
    Require(std::filesystem::create_directory(junctionTarget));
    const auto junctionCommand = L"cmd /c mklink /J \"" + junction.wstring() + L"\" \"" + std::filesystem::absolute(junctionTarget).wstring() + L"\"";
    Require(_wsystem(junctionCommand.c_str()) == 0);
    char junctionBuffer[512];
    std::memset(junctionBuffer, '#', sizeof(junctionBuffer));
    const auto junctionLength = readlink_nid_postfix(junction.string().c_str(), junctionBuffer, sizeof(junctionBuffer));
    Require(junctionLength > 0 && static_cast<std::size_t>(junctionLength) < sizeof(junctionBuffer) && junctionBuffer[junctionLength] == '#');
    const std::string junctionText(junctionBuffer, static_cast<std::size_t>(junctionLength));
    std::error_code junctionError;
    Require(junctionText.find('?') == std::string::npos && std::filesystem::equivalent(junctionText, junctionTarget, junctionError) && !junctionError);
    Require(std::filesystem::remove(junction) && std::filesystem::remove(junctionTarget));
#endif
    if (created == 0) {
        const auto throughLink = (root / "through-link.bin").string();
        Require(link_nid_postfix(symbolic.c_str(), throughLink.c_str()) == 0);
#ifdef _WIN32
        Require((GetFileAttributesW(std::filesystem::path(throughLink).c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) == 0);
#else
        Require(!std::filesystem::is_symlink(std::filesystem::symlink_status(throughLink)));
#endif
        Require(std::filesystem::equivalent(throughLink, original) && std::filesystem::hard_link_count(original) == 2);
        Require(std::filesystem::remove(throughLink) && std::filesystem::hard_link_count(original) == 1);
        Require(link_nid_postfix((root / "dangling").string().c_str(), throughLink.c_str()) == -1 && Error() == 2);
        const auto dotLink = (root / "dot-link").string();
        Require(symlink_nid_postfix(".", dotLink.c_str()) == 0);
        Require(link_nid_postfix(dotLink.c_str(), throughLink.c_str()) == -1 && Error() == 1);
#ifdef _WIN32
        Require(symlink_nid_postfix("loop-b", (root / "loop-a").string().c_str()) == 0);
        Require(symlink_nid_postfix("loop-a", (root / "loop-b").string().c_str()) == 0);
        Require(link_nid_postfix((root / "loop-a").string().c_str(), throughLink.c_str()) == -1 && Error() == 62);
#endif
        Require(!std::filesystem::exists(std::filesystem::symlink_status(throughLink)));
    }
    RemoveTree(root);
    return 0;
}
