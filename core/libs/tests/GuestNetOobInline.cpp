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

static void CheckFlag(int socket, int expected) {
    for (const std::uint32_t capacity : {0u, 1u, 3u, 4u, 8u}) {
        std::array<std::uint8_t, 8> bytes;
        bytes.fill(0xa5);
        std::uint32_t size = capacity;
        Require(sceNetGetsockopt(socket, 0xffff, 0x100, bytes.data(), &size) == 0 && size == std::min(capacity, 4u));
        Require(std::memcmp(bytes.data(), &expected, size) == 0);
        Require(std::all_of(bytes.begin() + size, bytes.end(), [](auto byte) { return byte == 0xa5; }));
    }
}

static void CheckInheritance(int family, int beforeListen, bool changeAfterListen) {
    std::fprintf(stderr, "inheritance family=%d beforeListen=%d changeAfterListen=%d\n",
        family, beforeListen, changeAfterListen);
    const int listener = sceNetSocket("inheritance-listener", family, 1, 6);
    const int client = sceNetSocket("inheritance-client", family, 1, 6);
    Require(listener >= 0 && client >= 0);
    Require(sceNetSetsockopt(listener, 0xffff, 0x100, &beforeListen, sizeof(beforeListen)) == 0);
    CheckFlag(listener, beforeListen != 0 ? 256 : 0);
    std::array<std::uint8_t, 28> address{};
    std::uint32_t size = family == 2 ? 16 : 28;
    address[0] = size;
    address[1] = family;
    if (family == 2) { address[4] = 127; address[7] = 1; }
    else address[23] = 1;
    Require(sceNetBind_nid_postfix(listener, address.data(), size) == 0);
    Require(sceNetListen(listener, 1) == 0);
    const int setting = changeAfterListen ? 1 - beforeListen : beforeListen;
    if (changeAfterListen)
        Require(sceNetSetsockopt(listener, 0xffff, 0x100, &setting, sizeof(setting)) == 0);
    CheckFlag(listener, setting != 0 ? 256 : 0);
    Require(sceNetGetsockname(listener, address.data(), &size) == 0);
    Require(sceNetConnect(client, address.data(), size) == 0);
    const int accepted = sceNetAccept(listener, nullptr, nullptr);
    Require(accepted >= 0);
    CheckFlag(accepted, setting != 0 ? 256 : 0);
    CheckFlag(client, 0);
    const int opposite = 1 - setting;
    Require(sceNetSetsockopt(listener, 0xffff, 0x100, &opposite, sizeof(opposite)) == 0);
    CheckFlag(listener, opposite != 0 ? 256 : 0);
    CheckFlag(accepted, setting != 0 ? 256 : 0);
    Require(sceNetSetsockopt(listener, 0xffff, 0x100, &setting, sizeof(setting)) == 0);
    Require(sceNetSetsockopt(accepted, 0xffff, 0x100, &opposite, sizeof(opposite)) == 0);
    CheckFlag(accepted, opposite != 0 ? 256 : 0);
    CheckFlag(listener, setting != 0 ? 256 : 0);
    CheckFlag(client, 0);
    for (const int socket : {accepted, client, listener}) Require(sceNetSocketClose(socket) == 0);
}

int main() {
    Require(sceNetInit_nid_postfix() == 0);
    for (const int family : {2, 28}) {
        for (const int beforeListen : {0, 1}) {
            for (const bool changeAfterListen : {false, true})
                CheckInheritance(family, beforeListen, changeAfterListen);
        }
        const int datagram = sceNetSocket("inline-datagram", family, 2, 17);
        const int listener = sceNetSocket("inline-listener", family, 1, 6);
        const int client = sceNetSocket("inline-client", family, 1, 6);
        Require(datagram >= 0 && listener >= 0 && client >= 0);
        for (const int socket : {datagram, listener}) {
            CheckFlag(socket, 0);
            for (const int setting : {1, -1, 0, 256, 0, 1}) {
                Require(sceNetSetsockopt(socket, 0xffff, 0x100, &setting, sizeof(setting)) == 0);
                CheckFlag(socket, setting != 0 ? 256 : 0);
            }
            const int on = 1;
            Require(sceNetSetsockopt(socket, 0xffff, 0x100, &on, 3) == static_cast<int>(0x80410116u));
            Require(*sceNetErrnoLoc() == 22);
            Require(sceNetSetsockopt(socket, 0xffff, 0x100, nullptr, 4) == static_cast<int>(0x80410116u));
        }
        CheckFlag(client, 0);
        std::array<std::uint8_t, 28> address{};
        std::uint32_t size = family == 2 ? 16 : 28;
        address[0] = size;
        address[1] = family;
        if (family == 2) { address[4] = 127; address[7] = 1; }
        else address[23] = 1;
        Require(sceNetBind_nid_postfix(listener, address.data(), size) == 0);
        Require(sceNetListen(listener, 1) == 0);
        Require(sceNetGetsockname(listener, address.data(), &size) == 0);
        Require(sceNetConnect(client, address.data(), size) == 0);
        const int accepted = sceNetAccept(listener, nullptr, nullptr);
        Require(accepted >= 0);
        CheckFlag(accepted, 256);
        const int timeout = 500000;
        Require(sceNetSetsockopt(accepted, 0xffff, 0x1106, &timeout, sizeof(timeout)) == 0);
        std::shared_ptr<void> owner;
        std::uintptr_t native = 0;
        Require(KernelFindSocketNative_nid_no_patch(client, &owner, &native));
        Require(::send(native, "A", 1, 0) == 1);
        Require(::send(native, "!", 1, MSG_OOB) == 1);
        Require(::send(native, "B", 1, 0) == 1);
        std::array<char, 3> received{};
        std::size_t count = 0;
        while (count < received.size()) {
            const auto result = sceNetRecv(accepted, received.data() + count, received.size() - count, 0);
            Require(result > 0);
            count += result;
        }
        Require(std::memcmp(received.data(), "A!B", 3) == 0);
        const int off = 0;
        Require(sceNetSetsockopt(listener, 0xffff, 0x100, &off, sizeof(off)) == 0);
        CheckFlag(listener, 0);
        CheckFlag(accepted, 256);
        for (const int socket : {accepted, client, listener, datagram}) Require(sceNetSocketClose(socket) == 0);
        int value = 0;
        size = sizeof(value);
        Require(sceNetGetsockopt(listener, 0xffff, 0x100, &value, &size) == static_cast<int>(0x80410109u));
    }
}
