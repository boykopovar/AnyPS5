#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "prx/libSceNet/libSceNet.hpp"
#include "prx/libkernel/libkernel.hpp"
#include "prx/libkernel/Socket/include/SocketPoll.hpp"


static void Require(bool condition) {
    if (!condition) std::abort();
}

static bool Failed(std::int64_t result, int error) {
    return result == static_cast<int>(0x80410100u | static_cast<unsigned>(error)) && *sceNetErrnoLoc() == error;
}

static void CheckKeepalive() {
    for (const int family : {2, 28}) {
        for (const int type : {1, 2}) {
            const int socket = sceNetSocket("keepalive", family, type, type == 1 ? 6 : 17);
            Require(socket >= 0);
            int value = -1;
            std::uint32_t size = sizeof(value);
            Require(sceNetGetsockopt(socket, 0xffff, 8, &value, &size) == 0 && value == 0 && size == sizeof(value));
            for (const int setting : {1, -1, 0, 8, 0}) {
                Require(sceNetSetsockopt(socket, 0xffff, 8, &setting, sizeof(setting)) == 0);
                size = sizeof(value);
                Require(sceNetGetsockopt(socket, 0xffff, 8, &value, &size) == 0 && value == (setting ? 8 : 0));
                if (type == 1) {
                    std::shared_ptr<void> owner;
                    std::uintptr_t native = 0;
                    Require(KernelFindSocketNative_nid_no_patch(socket, &owner, &native));
                    int enabled = 0;
#ifdef _WIN32
                    int length = sizeof(enabled);
#else
                    socklen_t length = sizeof(enabled);
#endif
                    Require(::getsockopt(native, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<char*>(&enabled), &length) == 0);
                    Require((enabled != 0) == (setting != 0));
                }
            }
            const int enabled = 1;
            Require(Failed(sceNetSetsockopt(socket, 0xffff, 8, &enabled, sizeof(enabled) - 1), 22));
            Require(Failed(sceNetSetsockopt(socket, 0xffff, 8, nullptr, sizeof(enabled)), 22));
            Require(setsockopt_nid_postfix(socket, 0xffff, 8, &enabled, sizeof(enabled)) == 0);
            const int other = sceNetSocket("keepalive-other", family, type, type == 1 ? 6 : 17);
            Require(other >= 0);
            size = sizeof(value);
            Require(sceNetGetsockopt(other, 0xffff, 8, &value, &size) == 0 && value == 0);
            Require(sceNetSocketClose(other) == 0);
            Require(Failed(sceNetGetsockopt(socket, 0xffff, 8, nullptr, &size), 22));
            Require(Failed(sceNetGetsockopt(socket, 0xffff, 8, &value, nullptr), 22));
            for (const std::uint32_t length : {0u, 1u, 3u, 4u, 8u}) {
                std::array<std::uint8_t, 8> bytes;
                bytes.fill(0xa5);
                size = length;
                const int expected = 8;
                Require(sceNetGetsockopt(socket, 0xffff, 8, bytes.data(), &size) == 0 && size == std::min(length, 4u));
                Require(std::memcmp(bytes.data(), &expected, size) == 0);
                Require(std::all_of(bytes.begin() + size, bytes.end(), [](auto byte) { return byte == 0xa5; }));
            }
            Require(sceNetSocketClose(socket) == 0);
            size = sizeof(value);
            Require(Failed(sceNetGetsockopt(socket, 0xffff, 8, &value, &size), 9));
            Require(Failed(sceNetSetsockopt(socket, 0xffff, 8, &enabled, sizeof(enabled)), 9));
        }
    }
}

int main() {
    Require(sceNetInit_nid_postfix() == 0);
    CheckKeepalive();
    const auto readFlag = [](int socket) {
        int value = -1;
        std::uint32_t size = sizeof(value);
        Require(sceNetGetsockopt(socket, 0xffff, 8, &value, &size) == 0 && size == sizeof(value));
        return value;
    };
    for (const int family : {2, 28}) {
        const int listener = sceNetSocket("keepalive-listener", family, 1, 6);
        const int client = sceNetSocket("keepalive-client", family, 1, 6);
        Require(listener >= 0 && client >= 0);
        const int on = 1;
        const int off = 0;
        Require(sceNetSetsockopt(listener, 0xffff, 8, &on, sizeof(on)) == 0);
        std::array<std::uint8_t, 28> address{};
        std::uint32_t size = family == 2 ? 16 : 28;
        address[0] = size;
        address[1] = family;
        if (family == 2) {
            address[4] = 127;
            address[7] = 1;
        } else {
            address[23] = 1;
        }
        Require(sceNetBind_nid_postfix(listener, address.data(), size) == 0);
        Require(sceNetListen(listener, 1) == 0);
        Require(sceNetGetsockname(listener, address.data(), &size) == 0);
        Require(sceNetConnect(client, address.data(), size) == 0);
        const int accepted = sceNetAccept(listener, nullptr, nullptr);
        Require(accepted >= 0 && readFlag(accepted) == 8 && readFlag(client) == 0);
        Require(sceNetSetsockopt(listener, 0xffff, 8, &off, sizeof(off)) == 0);
        Require(readFlag(listener) == 0 && readFlag(accepted) == 8);
        for (const int socket : {accepted, client, listener}) Require(sceNetSocketClose(socket) == 0);
    }
}
