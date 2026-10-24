#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/Socket/include/SocketPoll.hpp"

extern "C" {
int APS5_VABI sceNetInit_nid_postfix();
int APS5_VABI sceNetSocket(const char*, int, int, int);
int APS5_VABI sceNetSocketClose(int);
int APS5_VABI sceNetSetsockopt(int, int, int, const void*, std::uint32_t);
int APS5_VABI sceNetGetsockopt(int, int, int, void*, std::uint32_t*);
int* APS5_VABI sceNetErrnoLoc();
int APS5_VABI setsockopt_nid_postfix(int, int, int, const void*, std::uint32_t);
int* APS5_VABI __error_nid_postfix();
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "check failed at line %u\n", location.line());
        std::abort();
    }
}

int main() {
    Require(sceNetInit_nid_postfix() == 0);
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
            Require(sceNetSocketClose(socket) == 0);
        }
    }
    const int zero = 0;
    Require(sceNetSetsockopt(-1, 0xffff, 0x1001, &zero, sizeof(zero)) == static_cast<int>(0x80410109u));
    Require(*sceNetErrnoLoc() == 9);
}
