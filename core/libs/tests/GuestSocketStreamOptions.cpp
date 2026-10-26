#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <utility>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "socket stream option check failed at line %d, errno %d\n", line, *__error_nid_postfix());
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

constexpr int SocketLevel = 0xffff;
constexpr int TcpLevel = 6;
constexpr int TcpNoDelay = 1;
constexpr int TcpMaximumSegment = 2;
constexpr int Ipv6Level = 41;
constexpr int Ipv6UnicastHops = 4;
constexpr int Ipv6Only = 27;
constexpr int NoSigpipe = 0x800;
constexpr int NonBlockingOption = 0x1200;
constexpr int NoSignalFlag = 0x20000;
constexpr int GetStatusFlags = 3;
constexpr int NonBlockingFlag = 4;
constexpr int Invalid = 22;
constexpr int BrokenPipe = 32;
constexpr int Again = 35;
constexpr int NoProtocolOption = 42;

#ifndef _WIN32
static volatile std::sig_atomic_t brokenPipes = 0;
static void CountBrokenPipe(int) {
    brokenPipes = brokenPipes + 1;
}
#endif

static int BrokenPipes() {
#ifdef _WIN32
    return 0;
#else
    return brokenPipes;
#endif
}

static bool Fails(std::int64_t result, int error) {
    return result == -1 && *__error_nid_postfix() == error;
}

static bool Set(int descriptor, int level, int option, int value) {
    return setsockopt_nid_postfix(descriptor, level, option, &value, sizeof(value)) == 0;
}

static int Get(int descriptor, int level, int option) {
    int value = -1;
    std::uint32_t length = sizeof(value);
    Require(getsockopt_nid_postfix(descriptor, level, option, &value, &length) == 0 && length == sizeof(value));
    return value;
}

int main() {
    const int listener = socket_nid_postfix(2, 1, 0);
    Require(listener >= 0);
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0);
    Require(listen_nid_postfix(listener, 4) == 0);
    std::uint32_t addressSize = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &addressSize) == 0);
    const int client = socket_nid_postfix(2, 1, 0);
    Require(client >= 0 && connect_nid_postfix(client, address.data(), address.size()) == 0);
    const int server = accept_nid_postfix(listener, nullptr, nullptr);
    Require(server >= 0);
    Require(Set(listener, SocketLevel, NoSigpipe, 1) && Set(listener, TcpLevel, TcpNoDelay, 1) && Set(listener, TcpLevel, 0x100, 60) && Set(listener, TcpLevel, 4, 1) && Set(listener, TcpLevel, 8, 1));
    const int secondClient = socket_nid_postfix(2, 1, 0);
    Require(secondClient >= 0 && connect_nid_postfix(secondClient, address.data(), address.size()) == 0);
    const int inheriting = accept_nid_postfix(listener, nullptr, nullptr);
    Require(inheriting >= 0);
    Require(Get(inheriting, SocketLevel, NoSigpipe) == NoSigpipe && Get(inheriting, TcpLevel, TcpNoDelay) == 4);
    Require(Get(inheriting, TcpLevel, 0x100) == 60 && Get(server, TcpLevel, 0x100) == 7200);
    Require(Get(inheriting, TcpLevel, 4) == 0x1000 && Get(inheriting, TcpLevel, 8) == 0);
    Require(Get(server, SocketLevel, NoSigpipe) == 0 && Get(server, TcpLevel, TcpNoDelay) == 0);
    Require(close_nid_postfix(inheriting) == 0 && close_nid_postfix(secondClient) == 0);
    const int datagram = socket_nid_postfix(2, 2, 0);
    std::array<std::uint8_t, 16> datagramAddress{16, 2, 0, 0, 127, 0, 0, 1};
    Require(datagram >= 0 && bind_nid_postfix(datagram, datagramAddress.data(), datagramAddress.size()) == 0);
    int local[2]{-1, -1};
    Require(socketpair_nid_postfix(1, 1, 0, local) == 0);

    int one = 1;
    Require(Get(client, TcpLevel, TcpNoDelay) == 0);
    Require(Set(client, TcpLevel, TcpNoDelay, 7) && Get(client, TcpLevel, TcpNoDelay) == 4);
    unsigned char byte = 0x7f;
    std::uint32_t length = sizeof(byte);
    Require(getsockopt_nid_postfix(client, TcpLevel, TcpNoDelay, &byte, &length) == 0 && length == sizeof(byte) && byte == 4);
    int value = 0;
    Require(Set(client, TcpLevel, TcpNoDelay, 0) && Get(client, TcpLevel, TcpNoDelay) == 0);
    Require(Fails(setsockopt_nid_postfix(client, TcpLevel, TcpNoDelay, &one, sizeof(one) - 1), Invalid));
    Require(Fails(setsockopt_nid_postfix(client, TcpLevel, TcpMaximumSegment, &one, sizeof(one)), Invalid));
    const int fresh = socket_nid_postfix(2, 1, 0);
    Require(fresh >= 0 && Get(fresh, TcpLevel, TcpMaximumSegment) == 536);
    for (const int segment : {1000, 175, 0, -1}) {
        Require(Fails(setsockopt_nid_postfix(fresh, TcpLevel, TcpMaximumSegment, &segment, sizeof(segment)), Invalid));
    }
    Require(Set(fresh, TcpLevel, TcpMaximumSegment, 300) && Get(fresh, TcpLevel, TcpMaximumSegment) == 300);
    Require(Get(fresh, TcpLevel, 0x100) == 7200 && Get(fresh, TcpLevel, 0x200) == 75 && Get(fresh, TcpLevel, 0x400) == 8);
    Require(Set(fresh, TcpLevel, 0x100, 60) && Get(fresh, TcpLevel, 0x100) == 60);
    Require(Set(fresh, TcpLevel, 0x100, 0) && Get(fresh, TcpLevel, 0x100) == 7200);
    Require(Set(fresh, TcpLevel, 0x200, 10) && Get(fresh, TcpLevel, 0x200) == 10);
    Require(Set(fresh, TcpLevel, 0x400, 3) && Get(fresh, TcpLevel, 0x400) == 3);
    Require(Set(fresh, TcpLevel, 0x400, -1) && Get(fresh, TcpLevel, 0x400) == -1);
    Require(Fails(setsockopt_nid_postfix(fresh, TcpLevel, 0x7777, &one, sizeof(one)), NoProtocolOption));
    for (const auto& [option, bit] : {std::pair{4, 0x1000}, std::pair{8, 0x8}, std::pair{0x80, 0x10000}}) {
        Require(Get(fresh, TcpLevel, option) == 0);
        Require(Set(fresh, TcpLevel, option, 5) && Get(fresh, TcpLevel, option) == bit);
        Require(Set(fresh, TcpLevel, option, -1) && Get(fresh, TcpLevel, option) == bit);
        Require(Set(fresh, TcpLevel, option, 0) && Get(fresh, TcpLevel, option) == 0);
    }
    Require(close_nid_postfix(fresh) == 0);
    length = sizeof(value);
    Require(Fails(setsockopt_nid_postfix(datagram, TcpLevel, TcpNoDelay, &one, sizeof(one)), Invalid));
    Require(Fails(getsockopt_nid_postfix(datagram, TcpLevel, TcpNoDelay, &value, &length), Invalid));
    Require(Fails(setsockopt_nid_postfix(local[0], TcpLevel, TcpNoDelay, &one, sizeof(one)), Invalid));
    Require(Fails(getsockopt_nid_postfix(local[0], TcpLevel, TcpNoDelay, &value, &length), Invalid));

    char received = 0;
    Require(Get(server, SocketLevel, NonBlockingOption) == 0);
    Require((fcntl_nid_postfix(server, GetStatusFlags) & NonBlockingFlag) == 0);
    Require(Set(server, SocketLevel, NonBlockingOption, 5) && Get(server, SocketLevel, NonBlockingOption) == 0x100);
    Require((fcntl_nid_postfix(server, GetStatusFlags) & NonBlockingFlag) != 0);
    Require(Fails(recv_nid_postfix(server, &received, sizeof(received), 0), Again));
    Require(Set(server, SocketLevel, NonBlockingOption, 0) && Get(server, SocketLevel, NonBlockingOption) == 0);
    Require((fcntl_nid_postfix(server, GetStatusFlags) & NonBlockingFlag) == 0);
    Require(Fails(setsockopt_nid_postfix(server, SocketLevel, NonBlockingOption, &one, sizeof(one) - 1), Invalid));
    Require(Set(datagram, SocketLevel, NonBlockingOption, 1) && Get(datagram, SocketLevel, NonBlockingOption) == 0x100);
    Require(Fails(recv_nid_postfix(datagram, &received, sizeof(received), 0), Again));

#ifndef _WIN32
    std::signal(SIGPIPE, CountBrokenPipe);
#endif
    Require(Get(client, SocketLevel, NoSigpipe) == 0);
    Require(Set(client, SocketLevel, NoSigpipe, 3) && Get(client, SocketLevel, NoSigpipe) == NoSigpipe);
    Require(Fails(setsockopt_nid_postfix(client, SocketLevel, NoSigpipe, &one, sizeof(one) - 1), Invalid));
    Require(shutdown_nid_postfix(client, 1) == 0);
    Require(Fails(send_nid_postfix(client, "x", 1, 0), BrokenPipe));
    Require(Fails(write_nid_postfix(client, "x", 1), BrokenPipe));
    Require(BrokenPipes() == 0);
    Require(Set(client, SocketLevel, NoSigpipe, 0) && Get(client, SocketLevel, NoSigpipe) == 0);
    Require(Fails(send_nid_postfix(client, "x", 1, NoSignalFlag), BrokenPipe));
    Require(BrokenPipes() == 0);
#ifndef _WIN32
    Require(Fails(send_nid_postfix(client, "x", 1, 0), BrokenPipe));
    Require(BrokenPipes() == 1);
#endif

    const int dualStack = socket_nid_postfix(28, 1, 0);
    Require(dualStack >= 0 && Get(dualStack, Ipv6Level, Ipv6Only) == 1);
    Require(Set(dualStack, Ipv6Level, Ipv6Only, 0) && Get(dualStack, Ipv6Level, Ipv6Only) == 0);
    std::array<std::uint8_t, 28> mapped{28, 28, address[2], address[3], 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 127, 0, 0, 1};
    Require(connect_nid_postfix(dualStack, mapped.data(), mapped.size()) == 0);
    const int mappedPeer = accept_nid_postfix(listener, nullptr, nullptr);
    Require(mappedPeer >= 0 && close_nid_postfix(mappedPeer) == 0);
    const int datagram6 = socket_nid_postfix(28, 2, 0);
    Require(datagram6 >= 0 && Get(datagram6, Ipv6Level, Ipv6Only) == 1);
    Require(Set(datagram6, Ipv6Level, Ipv6Only, 9) && Get(datagram6, Ipv6Level, Ipv6Only) == 1);
    Require(Fails(setsockopt_nid_postfix(datagram6, Ipv6Level, Ipv6Only, &one, sizeof(one) - 1), Invalid));
    Require(Fails(setsockopt_nid_postfix(datagram6, Ipv6Level, Ipv6UnicastHops, &one, sizeof(one)), NoProtocolOption));
    length = sizeof(value);
    Require(Fails(getsockopt_nid_postfix(datagram6, Ipv6Level, Ipv6UnicastHops, &value, &length), NoProtocolOption));
    std::array<std::uint8_t, 28> unspecified6{28, 28};
    Require(bind_nid_postfix(datagram6, unspecified6.data(), unspecified6.size()) == 0);
    Require(Fails(setsockopt_nid_postfix(datagram6, Ipv6Level, Ipv6Only, &one, sizeof(one)), Invalid));
    Require(Fails(setsockopt_nid_postfix(datagram, Ipv6Level, Ipv6Only, &one, sizeof(one)), Invalid));
    Require(Fails(getsockopt_nid_postfix(datagram, Ipv6Level, Ipv6Only, &value, &length), Invalid));
    Require(Fails(setsockopt_nid_postfix(local[0], Ipv6Level, Ipv6Only, &one, sizeof(one)), Invalid));
    Require(close_nid_postfix(datagram6) == 0 && close_nid_postfix(dualStack) == 0);

    Require(close_nid_postfix(local[0]) == 0 && close_nid_postfix(local[1]) == 0);
    Require(close_nid_postfix(datagram) == 0 && close_nid_postfix(server) == 0);
    Require(close_nid_postfix(client) == 0 && close_nid_postfix(listener) == 0);
    return 0;
}
