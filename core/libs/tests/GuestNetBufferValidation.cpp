#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceNet/include/Net.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/Socket/include/SocketPoll.hpp"

struct GuestTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

struct GuestLinger {
    int onoff;
    int linger;
};

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "check failed at line %u\n", location.line());
        std::abort();
    }
}

static void CheckNetFault(int result) {
    Require(result == static_cast<int>(0x8041010eu));
    Require(*sceNetErrnoLoc() == 14);
    Require(*__error_nid_postfix() == 14);
}

static void CheckKernelFault(int result) {
    Require(result == -1);
    Require(*__error_nid_postfix() == 14);
    Require(*sceNetErrnoLoc() == 14);
}

int main() {
    Require(sceNetInit_nid_postfix() == 0);
    constexpr std::size_t page = 16384;
    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 2, 0, 0, &physical) == 0);
    void* mapping = nullptr;
    Require(sceKernelMapDirectMemory(&mapping, page * 2, 3, 0, physical, 0) == 0);
    auto* boundary = static_cast<std::uint8_t*>(mapping) + page;
    Require(sceKernelMprotect(boundary, page, 0) == 0);
    const std::array<void*, 3> badPointers{
        reinterpret_cast<void*>(std::uintptr_t{0x10}),
        boundary,
        boundary - 2,
    };
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
            for (void* bad : badPointers) {
                for (const int option : {0x1001, 0x1002}) {
                    CheckNetFault(sceNetSetsockopt(socket, 0xffff, option, bad, sizeof(int)));
                }
                for (const int timeoutOption : {0x1005, 0x1006}) {
                    CheckNetFault(sceNetSetsockopt(socket, 0xffff, timeoutOption, bad, sizeof(GuestTimeval)));
                    CheckKernelFault(setsockopt_nid_postfix(socket, 0xffff, timeoutOption, bad, sizeof(GuestTimeval)));
                }
                if (type == 1) {
                    CheckNetFault(sceNetSetsockopt(socket, 6, 1, bad, sizeof(int)));
                    CheckNetFault(sceNetSetsockopt(socket, 6, 0x100, bad, sizeof(int)));
                }
                if (family == 28) {
                    CheckNetFault(sceNetSetsockopt(socket, 41, 27, bad, sizeof(int)));
                    int v6only = 0;
                    std::uint32_t v6size = sizeof(v6only);
                    CheckNetFault(sceNetGetsockopt(socket, 41, 27, &v6only, static_cast<std::uint32_t*>(bad)));
                    CheckNetFault(sceNetGetsockopt(socket, 41, 27, bad, &v6size));
                    Require(v6size == sizeof(v6only));
                }
                CheckNetFault(sceNetSetsockopt(socket, 0xffff, 0x80, bad, sizeof(GuestLinger)));
                {
                    GuestLinger linger{};
                    std::uint32_t lingerSize = sizeof(linger);
                    CheckNetFault(sceNetGetsockopt(socket, 0xffff, 0x80, &linger, static_cast<std::uint32_t*>(bad)));
                    CheckNetFault(sceNetGetsockopt(socket, 0xffff, 0x80, bad, &lingerSize));
                    Require(lingerSize == sizeof(linger));
                }
                for (const int option : {0x1008, 0x1001}) {
                    int value = 0;
                    std::uint32_t size = sizeof(value);
                    CheckNetFault(sceNetGetsockopt(socket, 0xffff, option, &value, static_cast<std::uint32_t*>(bad)));
                    CheckNetFault(sceNetGetsockopt(socket, 0xffff, option, bad, &size));
                    Require(size == sizeof(value));
                }
            }
            Require(sceNetSocketClose(socket) == 0);
        }
    }
    Require(sceKernelMprotect(boundary, page, 3) == 0);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(mapping), page * 2) == 0);
    Require(sceKernelReleaseDirectMemory(physical, page * 2) == 0);
    const int zero = 0;
    Require(sceNetSetsockopt(-1, 0xffff, 0x1001, &zero, sizeof(zero)) == static_cast<int>(0x80410109u));
    Require(*sceNetErrnoLoc() == 9);
}
