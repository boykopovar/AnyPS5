#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <source_location>
#include "prx/libSceNet/libSceNet.hpp"
#include "prx/libkernel/libkernel.hpp"
#include "prx/libkernel/Socket/include/SocketPoll.hpp"

namespace {

constexpr int SolSocket = 0xffff;
constexpr int SoLinger = 0x80;
constexpr int NetError = static_cast<int>(0x80410100u);
constexpr int Ebadf = 9;
constexpr int Einval = 22;
constexpr int Edom = 33;

struct Linger {
    std::int32_t enabled;
    std::int32_t seconds;
};

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "check failed at line %u\n", location.line());
        std::abort();
    }
}

bool Failed(int result, int error) {
    return result == (NetError | error) && *sceNetErrnoLoc() == error;
}

Linger NetLinger(int socket) {
    Linger value{-1, -1};
    std::uint32_t size = sizeof(value);
    Require(sceNetGetsockopt(socket, SolSocket, SoLinger, &value, &size) == 0);
    Require(size == sizeof(value));
    return value;
}

Linger KernelLinger(int socket) {
    Linger value{-1, -1};
    std::uint32_t size = sizeof(value);
    Require(getsockopt_nid_postfix(socket, SolSocket, SoLinger, &value, &size) == 0);
    Require(size == sizeof(value));
    return value;
}

Linger NativeLinger(int socket) {
    std::shared_ptr<void> owner;
    std::uintptr_t native = 0;
    Require(KernelFindSocketNative_nid_no_patch(socket, &owner, &native));
    linger value{};
#ifdef _WIN32
    int size = sizeof(value);
#else
    socklen_t size = sizeof(value);
#endif
#ifdef __APPLE__
    Require(::getsockopt(native, SOL_SOCKET, SO_LINGER_SEC, &value, &size) == 0);
#else
    Require(::getsockopt(native, SOL_SOCKET, SO_LINGER, reinterpret_cast<char*>(&value), &size) == 0);
#endif
    return {value.l_onoff != 0 ? 1 : 0, static_cast<std::int32_t>(value.l_linger)};
}

void Stream(int family) {
    const int socket = sceNetSocket("linger", family, 1, 6);
    Require(socket >= 0);
    const Linger on{1, 5};
    Require(sceNetSetsockopt(socket, SolSocket, SoLinger, &on, sizeof(on)) == 0);
    const auto net = NetLinger(socket);
    Require(net.enabled == SoLinger && net.seconds == 5);
    const auto kernel = KernelLinger(socket);
    Require(kernel.enabled == SoLinger && kernel.seconds == 5);
    const auto native = NativeLinger(socket);
    Require(native.enabled == 1 && native.seconds == 5);
    const Linger longest{1, 65535};
#ifdef __APPLE__
    const Linger longestHost{1, 327};
    Require(sceNetSetsockopt(socket, SolSocket, SoLinger, &longestHost, sizeof(longestHost)) == 0);
    Require(NetLinger(socket).seconds == 327 && NativeLinger(socket).seconds == 327);
    bool threw = false;
    try {
        sceNetSetsockopt(socket, SolSocket, SoLinger, &longest, sizeof(longest));
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw);
#else
    Require(sceNetSetsockopt(socket, SolSocket, SoLinger, &longest, sizeof(longest)) == 0);
    Require(NetLinger(socket).seconds == 65535);
#endif
    const Linger off{0, 0};
    Require(setsockopt_nid_postfix(socket, SolSocket, SoLinger, &off, sizeof(off)) == 0);
    Require(NetLinger(socket).enabled == 0);
    Require(NativeLinger(socket).enabled == 0);
    const Linger negative{1, -1};
    Require(Failed(sceNetSetsockopt(socket, SolSocket, SoLinger, &negative, sizeof(negative)), Edom));
    const Linger large{1, 65536};
    Require(Failed(sceNetSetsockopt(socket, SolSocket, SoLinger, &large, sizeof(large)), Edom));
    Require(Failed(sceNetSetsockopt(socket, SolSocket, SoLinger, &on, sizeof(std::int32_t)), Einval));
    Require(Failed(sceNetSetsockopt(socket, SolSocket, SoLinger, nullptr, sizeof(on)), 14));
    Require(NetLinger(socket).enabled == 0);
    Require(NativeLinger(socket).enabled == 0);
    Linger read{};
    std::uint32_t size = sizeof(read);
    Require(sceNetGetsockopt(socket, SolSocket, SoLinger, nullptr, &size) == 0 && size == 0);
    Require(Failed(sceNetGetsockopt(socket, SolSocket, SoLinger, &read, nullptr), 14));
    Require(sceNetSocketClose(socket) == 0);
    Require(Failed(sceNetSetsockopt(socket, SolSocket, SoLinger, &on, sizeof(on)), Ebadf));
    size = sizeof(read);
    Require(Failed(sceNetGetsockopt(socket, SolSocket, SoLinger, &read, &size), Ebadf));
}

}

int main() {
    Require(sceNetInit_nid_postfix() == 0);
    Stream(2);
}
