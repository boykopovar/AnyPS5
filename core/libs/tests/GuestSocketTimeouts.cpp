#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

struct Timeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

struct Iovec {
    void* base;
    std::uint64_t length;
};

struct Msghdr {
    void* name;
    std::uint32_t nameLength;
    Iovec* iov;
    int iovLength;
    void* control;
    std::uint32_t controlLength;
    int flags;
};

extern "C" {
int APS5_VABI socket_nid_postfix(int, int, int);
int APS5_VABI bind_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI listen_nid_postfix(int, int);
int APS5_VABI getsockname_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI connect_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI accept_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI setsockopt_nid_postfix(int, int, int, const void*, std::uint32_t);
int APS5_VABI getsockopt_nid_postfix(int, int, int, void*, std::uint32_t*);
int APS5_VABI close_nid_postfix(int);
std::int64_t APS5_VABI send_nid_postfix(int, const void*, std::uint64_t, int);
std::int64_t APS5_VABI sendto_nid_postfix(int, const void*, std::uint64_t, int, const void*, std::uint32_t);
std::int64_t APS5_VABI recv_nid_postfix(int, void*, std::uint64_t, int);
std::int64_t APS5_VABI recvfrom_nid_postfix(int, void*, std::uint64_t, int, void*, std::uint32_t*);
std::int64_t APS5_VABI recvmsg_nid_postfix(int, Msghdr*, int);
std::int64_t APS5_VABI read_nid_postfix(int, void*, std::uint64_t);
int* APS5_VABI __error_nid_postfix();
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "socket timeout check failed at line %d, errno %d\n", line, *__error_nid_postfix());
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

constexpr int SocketLevel = 0xffff;
constexpr int TcpLevel = 6;
constexpr int SendBuffer = 0x1001;
constexpr int ReceiveBuffer = 0x1002;
constexpr int SendTimeout = 0x1005;
constexpr int ReceiveTimeout = 0x1006;
constexpr int DontWait = 0x80;
constexpr int Invalid = 22;
constexpr int Domain = 33;
constexpr int WouldBlock = 35;
constexpr int NoProtocolOption = 42;

using Clock = std::chrono::steady_clock;

static bool Fails(std::int64_t result, int error) {
    return result == -1 && *__error_nid_postfix() == error;
}

static int Set(int descriptor, int option, std::int64_t seconds, std::int64_t microseconds) {
    const Timeval time{seconds, microseconds};
    return setsockopt_nid_postfix(descriptor, SocketLevel, option, &time, sizeof(time));
}

static bool Reads(int descriptor, int option, std::int64_t seconds, std::int64_t microseconds) {
    Timeval time{-1, -1};
    std::uint32_t length = sizeof(time);
    return getsockopt_nid_postfix(descriptor, SocketLevel, option, &time, &length) == 0 && length == sizeof(time) &&
        time.seconds == seconds && time.microseconds == microseconds;
}

int main() {
    const int datagram = socket_nid_postfix(2, 2, 0);
    std::array<std::uint8_t, 16> own{16, 2, 0, 0, 127, 0, 0, 1};
    Require(datagram >= 0 && bind_nid_postfix(datagram, own.data(), own.size()) == 0);
    std::uint32_t ownSize = own.size();
    Require(getsockname_nid_postfix(datagram, own.data(), &ownSize) == 0);

    Require(Reads(datagram, ReceiveTimeout, 0, 0) && Reads(datagram, SendTimeout, 0, 0));
    Require(Set(datagram, ReceiveTimeout, 2, 0) == 0 && Reads(datagram, ReceiveTimeout, 2, 0));
    Require(Reads(datagram, SendTimeout, 0, 0));
    Require(Set(datagram, SendTimeout, 1, 500000) == 0 && Reads(datagram, SendTimeout, 1, 499999));
    Require(Set(datagram, SendTimeout, 0, 1) == 0 && Reads(datagram, SendTimeout, 0, 0));
    Require(Set(datagram, SendTimeout, std::int64_t{1} << 31, 7) == 0 && Reads(datagram, SendTimeout, 0x7fffffff, 999999));
    Require(Set(datagram, SendTimeout, 0, 0) == 0 && Reads(datagram, SendTimeout, 0, 0));
    Require(Fails(Set(datagram, ReceiveTimeout, -1, 0), Domain));
    Require(Fails(Set(datagram, ReceiveTimeout, 0, -1), Domain));
    Require(Fails(Set(datagram, ReceiveTimeout, 0, 1000000), Domain));
    Timeval time{0, 0};
    Require(Fails(setsockopt_nid_postfix(datagram, SocketLevel, ReceiveTimeout, &time, sizeof(time) - 1), Invalid));
    Require(Reads(datagram, ReceiveTimeout, 2, 0));
    std::int64_t seconds = -1;
    std::uint32_t length = sizeof(seconds);
    Require(getsockopt_nid_postfix(datagram, SocketLevel, ReceiveTimeout, &seconds, &length) == 0 && length == sizeof(seconds) && seconds == 2);
    length = sizeof(time);
    Require(Fails(getsockopt_nid_postfix(datagram, TcpLevel, ReceiveTimeout, &time, &length), Invalid));

    char byte = 0;
    Require(Set(datagram, ReceiveTimeout, 0, 60000) == 0);
    auto started = Clock::now();
    Require(Fails(recv_nid_postfix(datagram, &byte, sizeof(byte), 0), WouldBlock));
    Require(Clock::now() - started >= std::chrono::milliseconds(59));
    Require(Fails(recvfrom_nid_postfix(datagram, &byte, sizeof(byte), 0, nullptr, nullptr), WouldBlock));
    Iovec part{&byte, sizeof(byte)};
    Msghdr message{nullptr, 0, &part, 1, nullptr, 0, 0};
    Require(Fails(recvmsg_nid_postfix(datagram, &message, 0), WouldBlock));
    Require(Fails(read_nid_postfix(datagram, &byte, sizeof(byte)), WouldBlock));
    Require(Set(datagram, ReceiveTimeout, 60, 0) == 0);
    Require(Fails(recv_nid_postfix(datagram, &byte, sizeof(byte), DontWait), WouldBlock));
    Require(Fails(recvfrom_nid_postfix(datagram, &byte, sizeof(byte), DontWait, nullptr, nullptr), WouldBlock));
    Require(Fails(recvmsg_nid_postfix(datagram, &message, DontWait), WouldBlock));
    Require(Set(datagram, ReceiveTimeout, 0, 60000) == 0);
    Require(sendto_nid_postfix(datagram, "d", 1, 0, own.data(), own.size()) == 1);
    Require(recv_nid_postfix(datagram, &byte, sizeof(byte), 0) == 1 && byte == 'd');

    const int listener = socket_nid_postfix(2, 1, 0);
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(listener >= 0 && bind_nid_postfix(listener, address.data(), address.size()) == 0);
    int small = 4096;
    Require(setsockopt_nid_postfix(listener, SocketLevel, ReceiveBuffer, &small, sizeof(small)) == 0);
    Require(Set(listener, ReceiveTimeout, 3, 0) == 0);
    Require(listen_nid_postfix(listener, 4) == 0);
    std::uint32_t addressSize = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &addressSize) == 0);
    const int client = socket_nid_postfix(2, 1, 0);
    Require(client >= 0 && setsockopt_nid_postfix(client, SocketLevel, SendBuffer, &small, sizeof(small)) == 0);
    Require(connect_nid_postfix(client, address.data(), address.size()) == 0);
    const int server = accept_nid_postfix(listener, nullptr, nullptr);
    Require(server >= 0 && Reads(server, ReceiveTimeout, 3, 0) && Reads(server, SendTimeout, 0, 0));

    Require(Set(client, ReceiveTimeout, 0, 60000) == 0);
    started = Clock::now();
    Require(Fails(recv_nid_postfix(client, &byte, sizeof(byte), 0), WouldBlock));
    Require(Clock::now() - started >= std::chrono::milliseconds(59));

    Require(Set(client, SendTimeout, 0, 60000) == 0);
    const std::vector<char> chunk(1024, 'x');
    std::int64_t sent = 0;
    bool stopped = false;
    for (int round = 0; round < 200000 && !stopped; ++round) {
        const auto result = send_nid_postfix(client, chunk.data(), chunk.size(), 0);
        if (result > 0) sent += result;
        else stopped = Fails(result, WouldBlock);
        Require(result > 0 || stopped);
    }
    Require(stopped && sent > 0);

    Require(close_nid_postfix(server) == 0 && close_nid_postfix(client) == 0);
    Require(close_nid_postfix(listener) == 0 && close_nid_postfix(datagram) == 0);
    return 0;
}
