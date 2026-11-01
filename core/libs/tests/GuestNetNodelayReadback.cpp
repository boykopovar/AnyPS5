#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
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


static void Require(bool condition, std::source_location where = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "check failed at line %u\n", where.line());
        std::abort();
    }
}

static void CheckValue(int socket, int expected) {
    for (const std::uint32_t capacity : {0u, 1u, 2u, 3u, 4u, 8u}) {
        std::array<std::uint8_t, 8> bytes;
        bytes.fill(0xa5);
        auto size = capacity;
        Require(sceNetGetsockopt(socket, 6, 1, bytes.data(), &size) == 0);
        Require(size == std::min(capacity, 4u));
        Require(std::memcmp(bytes.data(), &expected, size) == 0);
        Require(std::all_of(bytes.begin() + size, bytes.end(), [](auto value) { return value == 0xa5; }));
    }
}

int main() {
    Require(sceNetInit_nid_postfix() == 0);
    for (const int family : {2, 28}) {
        const int socket = sceNetSocket("nodelay", family, 1, 6);
        const int other = sceNetSocket("nodelay-other", family, 1, 0);
        const int datagram = sceNetSocket("nodelay-datagram", family, 2, 17);
        Require(socket >= 0 && other >= 0 && datagram >= 0);
        CheckValue(socket, 0);
        for (const int requested : {1, 0, -1, 256, 0}) {
            Require(sceNetSetsockopt(socket, 6, 1, &requested, sizeof(requested)) == 0);
            CheckValue(socket, requested != 0 ? 4 : 0);
            CheckValue(other, 0);
        }
        std::shared_ptr<void> owner;
        std::uintptr_t native = 0;
        Require(KernelFindSocketNative_nid_no_patch(socket, &owner, &native));
        const int enabled = 1;
        Require(::setsockopt(native, IPPROTO_TCP, TCP_NODELAY,
            reinterpret_cast<const char*>(&enabled), sizeof(enabled)) == 0);
        CheckValue(socket, 4);
        std::uint32_t size = 0;
        int value = 99;
        Require(sceNetGetsockopt(socket, 6, 1, nullptr, &size) < 0 && *sceNetErrnoLoc() == 22);
        Require(sceNetGetsockopt(socket, 6, 1, &value, nullptr) < 0 && *sceNetErrnoLoc() == 22);
        for (const std::uint32_t capacity : {0u, 3u, 4u}) {
            size = capacity;
            Require(sceNetGetsockopt(datagram, 6, 1, &value, &size) < 0 && *sceNetErrnoLoc() == 22);
            Require(size == capacity && value == 99);
            Require(sceNetGetsockopt(socket, 6, 0x7fffffff, &value, &size) < 0);
            Require(*sceNetErrnoLoc() == (capacity < 4 ? 22 : 42));
            Require(size == capacity && value == 99);
        }
        owner.reset();
        for (const int descriptor : {socket, other, datagram}) Require(sceNetSocketClose(descriptor) == 0);
        size = 0;
        Require(sceNetGetsockopt(socket, 6, 1, &value, &size) < 0 && *sceNetErrnoLoc() == 9);
        Require(size == 0 && value == 99);
    }
}
