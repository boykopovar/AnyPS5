#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"
#include "prx/libc/include/Api.hpp"
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <utility>

enum class NativeResult { Disconnected, Connected, UnknownPeer };
static int injectedDescriptor = -1;
static int injectedError = 0;
static int peerError = 0;
static NativeResult injectedResult = NativeResult::Disconnected;

extern "C" int connect(int descriptor, const sockaddr* address, socklen_t size) {
    if (descriptor != injectedDescriptor || address->sa_family != AF_UNSPEC)
        return static_cast<int>(::syscall(SYS_connect, descriptor, address, size));
    if (injectedResult != NativeResult::Connected) {
        const int result = static_cast<int>(::syscall(SYS_connect, descriptor, address, size));
        if (result != 0) return result;
    }
    if (injectedResult == NativeResult::UnknownPeer) peerError = EACCES;
    errno = injectedError;
    return -1;
}

extern "C" int getpeername(int descriptor, sockaddr* address, socklen_t* size) {
    if (descriptor == injectedDescriptor && peerError != 0) {
        errno = std::exchange(peerError, 0);
        return -1;
    }
    return static_cast<int>(::syscall(SYS_getpeername, descriptor, address, size));
}

static void Require(bool value) { if (!value) std::abort(); }

static void CheckResult(int family, int binding, int error, NativeResult result, bool portZero) {
    const std::uint32_t addressSize = family == 2 ? 16 : 28;
    const int peer = socket_nid_postfix(family, 2, 0);
    if (peer < 0 && family == 28 && *__error_nid_postfix() == 47) return;
    Require(peer >= 0);
    std::array<unsigned char, 28> destination{};
    destination[0] = static_cast<unsigned char>(addressSize);
    destination[1] = static_cast<unsigned char>(family);
    if (family == 2) { destination[4] = 127; destination[7] = 1; }
    else destination[23] = 1;
    const int bound = bind_nid_postfix(peer, destination.data(), addressSize);
    if (bound < 0 && family == 28 && *__error_nid_postfix() == 49) {
        Require(close_nid_postfix(peer) == 0);
        return;
    }
    Require(bound == 0);
    std::uint32_t size = destination.size();
    Require(getsockname_nid_postfix(peer, destination.data(), &size) == 0 && size == addressSize);
    int client = socket_nid_postfix(family, 2, 0);
    Require(client >= 0);
    if (binding != 0) {
        auto local = destination;
        local[2] = local[3] = 0;
        Require(bind_nid_postfix(client, local.data(), addressSize) == 0);
        if (binding == 2) {
            size = local.size();
            Require(getsockname_nid_postfix(client, local.data(), &size) == 0);
            Require(close_nid_postfix(client) == 0);
            client = socket_nid_postfix(family, 2, 0);
            Require(client >= 0 && bind_nid_postfix(client, local.data(), addressSize) == 0);
        }
    }
    Require(connect_nid_postfix(client, destination.data(), addressSize) == 0);
    std::array<unsigned char, 28> before{};
    size = before.size();
    Require(getsockname_nid_postfix(client, before.data(), &size) == 0 && size == addressSize);
    Require(before[2] || before[3]);
    const auto native = GuestSockets::NativeDescriptor(client);
    Require(native >= 0);
    injectedDescriptor = static_cast<int>(native);
    injectedError = error;
    injectedResult = result;
    auto disconnect = destination;
    if (portZero) disconnect[2] = disconnect[3] = 0;
    else disconnect[1] = 0;
    const int expected = result == NativeResult::Disconnected ? (portZero ? 49 : 47) : (error == EINVAL ? 22 : 47);
    Require(connect_nid_postfix(client, disconnect.data(), addressSize) == -1 && *__error_nid_postfix() == expected);
    injectedDescriptor = -1;
    Require(peerError == 0);
    std::array<unsigned char, 28> name{};
    size = name.size();
    if (result == NativeResult::Connected) {
        Require(getpeername_nid_postfix(client, name.data(), &size) == 0 && name == destination);
        size = name.size();
        Require(getsockname_nid_postfix(client, name.data(), &size) == 0 && name == before);
    } else {
        Require(getpeername_nid_postfix(client, name.data(), &size) == -1 && *__error_nid_postfix() == 57);
        if (result == NativeResult::Disconnected) {
            size = name.size();
            Require(getsockname_nid_postfix(client, name.data(), &size) == 0 && size == addressSize);
            Require(name[2] == before[2] && name[3] == before[3]);
            const auto begin = family == 2 ? 4 : 8;
            const auto end = family == 2 ? 8 : 28;
            Require(std::all_of(name.begin() + begin, name.begin() + end, [](auto byte) { return byte == 0; }));
        }
    }
    Require(close_nid_postfix(client) == 0 && close_nid_postfix(peer) == 0);
}

int main() {
    for (const int family : {2, 28})
        for (int binding = 0; binding < 3; ++binding)
            for (const int error : {EAFNOSUPPORT, EINVAL})
                for (const auto result : {NativeResult::Disconnected, NativeResult::Connected, NativeResult::UnknownPeer})
                    for (const bool portZero : {false, true}) CheckResult(family, binding, error, result, portZero);
}
