#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include "prx/libSceNet/libSceNet.hpp"
#include "prx/libkernel/Socket/include/SocketPoll.hpp"


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
            const int socket = sceNetSocket("boolean-readback", family, type, type == 1 ? 6 : 17);
            Require(socket >= 0);
            std::shared_ptr<void> owner;
            std::uintptr_t native = 0;
            Require(KernelFindSocketNative_nid_no_patch(socket, &owner, &native));
            for (const int option : {4, 32}) {
                if (option == 32 && (family != 2 || type != 2)) continue;
                for (const int setting : {1, -1, 0, 4, 0}) {
                    Require(sceNetSetsockopt(socket, 0xffff, option, &setting, sizeof(setting)) == 0);
                    for (const std::uint32_t capacity : {0u, 1u, 3u, 4u, 8u}) {
                        std::array<std::uint8_t, 8> bytes;
                        bytes.fill(0xa5);
                        std::uint32_t size = capacity;
                        const int expected = setting != 0 ? option : 0;
                        Require(sceNetGetsockopt(socket, 0xffff, option, bytes.data(), &size) == 0);
                        Require(size == std::min(capacity, 4u));
                        Require(std::memcmp(bytes.data(), &expected, size) == 0);
                        Require(std::all_of(bytes.begin() + size, bytes.end(), [](auto byte) { return byte == 0xa5; }));
                    }
                }
                const int enabled = 1;
                Require(::setsockopt(native, SOL_SOCKET, option == 4 ? SO_REUSEADDR : SO_BROADCAST,
                    reinterpret_cast<const char*>(&enabled), sizeof(enabled)) == 0);
                int value = 0;
                std::uint32_t size = sizeof(value);
                Require(sceNetGetsockopt(socket, 0xffff, option, &value, &size) == 0 && value == option);
                Require(sceNetGetsockopt(socket, 0xffff, option, nullptr, &size) == static_cast<int>(0x80410116u));
                Require(*sceNetErrnoLoc() == 22);
                Require(sceNetGetsockopt(socket, 0xffff, option, &value, nullptr) == static_cast<int>(0x80410116u));
            }
            Require(sceNetSocketClose(socket) == 0);
            int value = 0;
            std::uint32_t size = sizeof(value);
            Require(sceNetGetsockopt(socket, 0xffff, 4, &value, &size) == static_cast<int>(0x80410109u));
            Require(*sceNetErrnoLoc() == 9);
        }
    }
}
