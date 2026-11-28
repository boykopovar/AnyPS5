#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceNet/include/Net.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/Socket/include/SocketPoll.hpp"

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "check failed at line %u\n", location.line());
        std::abort();
    }
}

static std::size_t UnreadablePageSize() {
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    return info.dwPageSize;
#else
    const long size = sysconf(_SC_PAGESIZE);
    Require(size > 0);
    return static_cast<std::size_t>(size);
#endif
}

static void* UnreadablePage() {
    const std::size_t size = UnreadablePageSize();
#ifdef _WIN32
    void* page = VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS);
    Require(page != nullptr);
#else
    void* page = mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED);
#endif
    return page;
}

static void ReleaseUnreadablePage(void* page) {
#ifdef _WIN32
    Require(VirtualFree(page, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(page, UnreadablePageSize()) == 0);
#endif
}

int main() {
    Require(sceNetInit_nid_postfix() == 0);
    void* unreadablePage = UnreadablePage();
    for (const int family : {2, 28}) {
        for (const int type : {1, 2}) {
            const int socket = sceNetSocket("buffer-validation", family, type, type == 1 ? 6 : 17);
            Require(socket >= 0);
            std::shared_ptr<void> owner;
            std::uintptr_t native = 0;
            Require(KernelFindSocketNative_nid_no_patch(socket, &owner, &native));
            for (const int option : {0x1001, 0x1002}) {
                const auto nativeBuffer = [&] {
                    int value = 0;
#ifdef _WIN32
                    int size = sizeof(value);
#else
                    socklen_t size = sizeof(value);
#endif
                    Require(::getsockopt(native, SOL_SOCKET, option == 0x1001 ? SO_SNDBUF : SO_RCVBUF,
                        reinterpret_cast<char*>(&value), &size) == 0);
                    return value;
                };
                const int initial = 32768;
                Require(sceNetSetsockopt(socket, 0xffff, option, &initial, sizeof(initial)) == 0);
                const int originalNative = nativeBuffer();
                for (const int invalid : {0, -1, -4096, INT_MIN}) {
                    Require(sceNetSetsockopt(socket, 0xffff, option, &invalid, sizeof(invalid)) == static_cast<int>(0x80410116u));
                    Require(*sceNetErrnoLoc() == 22);
                    Require(setsockopt_nid_postfix(socket, 0xffff, option, &invalid, sizeof(invalid)) == -1);
                    Require(*__error_nid_postfix() == 22);
                    int value = 0;
                    std::uint32_t size = sizeof(value);
                    Require(sceNetGetsockopt(socket, 0xffff, option, &value, &size) == 0 && value == initial);
                    Require(nativeBuffer() == originalNative);
                }
                Require(sceNetSetsockopt(socket, 0xffff, option, nullptr, sizeof(initial)) == static_cast<int>(0x80410116u));
                Require(sceNetSetsockopt(socket, 0xffff, option, &initial, 3) == static_cast<int>(0x80410116u));
                const int valid = 8192;
                Require(sceNetSetsockopt(socket, 0xffff, option, &valid, sizeof(valid)) == 0);
                int value = 0;
                std::uint32_t size = sizeof(value);
                Require(sceNetGetsockopt(socket, 0xffff, option, &value, &size) == 0 && value == valid);
            }
            const std::uint64_t timeout[2]{};
            for (const int option : {0x1005, 0x1006}) {
                for (const void* unreadable : {reinterpret_cast<const void*>(0x10), static_cast<const void*>(unreadablePage)}) {
                    Require(sceNetSetsockopt(socket, 0xffff, option, unreadable, sizeof(timeout)) ==
                            static_cast<int>(0x8041010eu));
                    Require(*sceNetErrnoLoc() == 14);
                    Require(setsockopt_nid_postfix(socket, 0xffff, option, unreadable, sizeof(timeout)) == -1);
                    Require(*__error_nid_postfix() == 14);
                }
                Require(sceNetSetsockopt(socket, 0xffff, option, timeout, sizeof(timeout)) == 0);
                Require(setsockopt_nid_postfix(socket, 0xffff, option, timeout, sizeof(timeout)) == 0);
            }
            Require(sceNetSocketClose(socket) == 0);
        }
    }
    ReleaseUnreadablePage(unreadablePage);
    const int zero = 0;
    Require(sceNetSetsockopt(-1, 0xffff, 0x1001, &zero, sizeof(zero)) == static_cast<int>(0x80410109u));
    Require(*sceNetErrnoLoc() == 9);
}
