#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
int APS5_VABI socket_nid_postfix(int, int, int);
int APS5_VABI bind_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI listen_nid_postfix(int, int);
int APS5_VABI getsockname_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI connect_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI accept_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI setsockopt_nid_postfix(int, int, int, const void*, std::uint32_t);
int APS5_VABI getsockopt_nid_postfix(int, int, int, void*, std::uint32_t*);
int APS5_VABI socketpair_nid_postfix(int, int, int, int*);
int APS5_VABI fcntl_nid_postfix(int, int, ...);
int APS5_VABI poll_nid_postfix(void*, std::uint32_t, int);
int APS5_VABI close_nid_postfix(int);
int* APS5_VABI __error_nid_postfix();
}

namespace {

constexpr int Stream = 1;
constexpr int Datagram = 2;
constexpr int SolSocket = 0xffff;
constexpr int SoAcceptConnection = 0x2;
constexpr int SoDebug = 0x1;
constexpr int SoKeepAlive = 0x8;
constexpr int SoBroadcast = 0x20;
constexpr int SoUseLoopback = 0x40;
constexpr int SoLinger = 0x80;
constexpr int SoReusePort = 0x200;
constexpr int SoTimestamp = 0x400;
constexpr int SoNoSigpipe = 0x800;
constexpr int SoSendBuffer = 0x1001;
constexpr int SoReceiveBuffer = 0x1002;
constexpr int SoSendLowWater = 0x1003;
constexpr int SoReceiveLowWater = 0x1004;
constexpr int SoError = 0x1007;
constexpr int SoType = 0x1008;
constexpr int ErrorInvalid = 22;
constexpr int ErrorDomain = 33;
constexpr int ErrorNoProtocolOption = 42;
constexpr int ErrorConnectionRefused = 61;
constexpr int ErrorBadDescriptor = 9;

struct Linger {
    std::int32_t enabled;
    std::int32_t seconds;
};

struct PollDescriptor {
    std::int32_t descriptor;
    std::int16_t events;
    std::int16_t revents;
};

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "%s (errno %d)\n", what, *__error_nid_postfix());
    std::abort();
}

int GetInt(int descriptor, int option) {
    int value = -12345;
    std::uint32_t size = sizeof(value);
    Require(getsockopt_nid_postfix(descriptor, SolSocket, option, &value, &size) == 0 && size == sizeof(value), "getsockopt of an integer option failed");
    return value;
}

void SetInt(int descriptor, int option, int value) {
    Require(setsockopt_nid_postfix(descriptor, SolSocket, option, &value, sizeof(value)) == 0, "setsockopt of an integer option failed");
}

bool SetFails(int descriptor, int level, int option, const void* value, std::uint32_t length, int error) {
    return setsockopt_nid_postfix(descriptor, level, option, value, length) == -1 && *__error_nid_postfix() == error;
}

std::array<std::uint8_t, 16> Loopback(std::uint16_t portNetworkOrder) {
    std::array<std::uint8_t, 16> address{16, 2};
    std::memcpy(address.data() + 2, &portNetworkOrder, 2);
    address[4] = 127;
    address[7] = 1;
    return address;
}

std::array<std::uint8_t, 16> Listen(int* listener) {
    *listener = socket_nid_postfix(2, Stream, 0);
    Require(*listener >= 0, "socket failed");
    auto address = Loopback(0);
    Require(bind_nid_postfix(*listener, address.data(), address.size()) == 0, "bind failed");
    Require(listen_nid_postfix(*listener, 4) == 0, "listen failed");
    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(*listener, address.data(), &size) == 0, "getsockname failed");
    return address;
}

void WaitUntilSettled(int descriptor) {
    PollDescriptor entry{descriptor, 0x1 | 0x4, 0};
    Require(poll_nid_postfix(&entry, 1, 6000) == 1, "the nonblocking connect did not settle within 6 s");
}

void ReportsTypeAndListening() {
    const int stream = socket_nid_postfix(2, Stream, 0);
    const int datagram = socket_nid_postfix(2, Datagram, 0);
    Require(stream >= 0 && datagram >= 0, "socket failed");
    Require(GetInt(stream, SoType) == Stream, "SO_TYPE of a stream socket must be SOCK_STREAM (1)");
    Require(GetInt(datagram, SoType) == Datagram, "SO_TYPE of a datagram socket must be SOCK_DGRAM (2)");
    Require(GetInt(stream, SoAcceptConnection) == 0, "SO_ACCEPTCONN must be 0 before listen");
    auto address = Loopback(0);
    Require(bind_nid_postfix(stream, address.data(), address.size()) == 0 && listen_nid_postfix(stream, 4) == 0, "bind or listen failed");
    Require(GetInt(stream, SoAcceptConnection) == SoAcceptConnection, "SO_ACCEPTCONN of a listener is the option bit (2), as FreeBSD returns so_options & SO_ACCEPTCONN");
    Require(close_nid_postfix(stream) == 0 && close_nid_postfix(datagram) == 0, "close failed");
}

void ReportsAndClearsTheConnectError() {
    int listener = -1;
    auto address = Listen(&listener);
    const int client = socket_nid_postfix(2, Stream, 0);
    Require(client >= 0 && fcntl_nid_postfix(client, 4, 4) == 0, "nonblocking socket setup failed");
    const int result = connect_nid_postfix(client, address.data(), address.size());
    Require(result == 0 || (result == -1 && *__error_nid_postfix() == 36), "nonblocking connect to a listener must succeed or be in progress");
    WaitUntilSettled(client);
    Require(GetInt(client, SoError) == 0, "SO_ERROR after a successful connect must be 0");
    const int accepted = accept_nid_postfix(listener, nullptr, nullptr);
    Require(accepted >= 0, "accept failed");
    for (const int descriptor : {client, accepted, listener}) Require(close_nid_postfix(descriptor) == 0, "close failed");

    int closed = -1;
    auto refused = Listen(&closed);
    Require(close_nid_postfix(closed) == 0, "close failed");
    const int failing = socket_nid_postfix(2, Stream, 0);
    Require(failing >= 0 && fcntl_nid_postfix(failing, 4, 4) == 0, "nonblocking socket setup failed");
    const int attempt = connect_nid_postfix(failing, refused.data(), refused.size());
    if (attempt == -1 && *__error_nid_postfix() != 36) {
        Require(*__error_nid_postfix() == ErrorConnectionRefused, "a refused connect must fail with ECONNREFUSED (61)");
    } else {
        WaitUntilSettled(failing);
        Require(GetInt(failing, SoError) == ErrorConnectionRefused, "SO_ERROR after a refused connect must be ECONNREFUSED (61)");
        Require(GetInt(failing, SoError) == 0, "reading SO_ERROR must clear it");
    }
    Require(close_nid_postfix(failing) == 0, "close failed");
}

void BooleanOptionsReportTheirBit() {
    const int stream = socket_nid_postfix(2, Stream, 0);
    const int datagram = socket_nid_postfix(2, Datagram, 0);
    Require(stream >= 0 && datagram >= 0, "socket failed");
    Require(GetInt(stream, SoKeepAlive) == 0, "SO_KEEPALIVE is off by default");
    SetInt(stream, SoKeepAlive, 1);
    Require(GetInt(stream, SoKeepAlive) == SoKeepAlive, "an enabled boolean option reads back as its own bit (FreeBSD returns so_options & option)");
    SetInt(stream, SoKeepAlive, 0);
    Require(GetInt(stream, SoKeepAlive) == 0, "SO_KEEPALIVE must turn off");
    SetInt(datagram, SoBroadcast, 1);
    Require(GetInt(datagram, SoBroadcast) == SoBroadcast, "SO_BROADCAST reads back as its bit");
    SetInt(stream, SoNoSigpipe, 1);
    Require(GetInt(stream, SoNoSigpipe) == SoNoSigpipe, "SO_NOSIGPIPE reads back as its bit");
    for (const int option : {SoReusePort, SoDebug, SoUseLoopback, SoTimestamp, 0x2000, 0x10000}) {
        Require(GetInt(stream, option) == 0, "a kept flag starts off");
        SetInt(stream, option, 5);
        Require(GetInt(stream, option) == option, "a kept flag reads back as its bit");
        SetInt(stream, option, 0);
        Require(GetInt(stream, option) == 0, "a kept flag turns off");
    }
    for (const int option : {0x4000, 0x8000}) {
        SetInt(stream, option, 1);
        int value = 0;
        std::uint32_t size = sizeof(value);
        Require(getsockopt_nid_postfix(stream, SolSocket, option, &value, &size) == -1 && *__error_nid_postfix() == ErrorNoProtocolOption, "SO_NO_OFFLOAD and SO_NO_DDP are accepted but cannot be read");
    }
    const unsigned char narrowFlag = 1;
    Require(SetFails(stream, SolSocket, SoDebug, &narrowFlag, 1, ErrorInvalid), "a flag shorter than an int must fail with EINVAL (22)");
    Require(close_nid_postfix(stream) == 0 && close_nid_postfix(datagram) == 0, "close failed");
}

void ReusePortSharesAPort() {
    const int first = socket_nid_postfix(2, Datagram, 0);
    const int second = socket_nid_postfix(2, Datagram, 0);
    const int third = socket_nid_postfix(2, Datagram, 0);
    Require(first >= 0 && second >= 0 && third >= 0, "socket failed");
    SetInt(first, SoReusePort, 1);
    SetInt(second, SoReusePort, 1);
    Require(GetInt(first, SoReusePort) == SoReusePort && GetInt(first, 0x4) == 0, "SO_REUSEPORT does not read back as SO_REUSEADDR");
    auto address = Loopback(0);
    Require(bind_nid_postfix(first, address.data(), address.size()) == 0, "bind failed");
    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(first, address.data(), &size) == 0, "getsockname failed");
    Require(bind_nid_postfix(second, address.data(), address.size()) == 0, "a second SO_REUSEPORT socket must bind the same port");
    Require(bind_nid_postfix(third, address.data(), address.size()) == -1 && *__error_nid_postfix() == 48, "a socket without SO_REUSEPORT must fail with EADDRINUSE (48)");
    Require(close_nid_postfix(first) == 0 && close_nid_postfix(second) == 0 && close_nid_postfix(third) == 0, "close failed");
}

void LingerFollowsFreeBsd() {
    const int stream = socket_nid_postfix(2, Stream, 0);
    Require(stream >= 0, "socket failed");
    Linger linger{1, 5};
    Require(setsockopt_nid_postfix(stream, SolSocket, SoLinger, &linger, sizeof(linger)) == 0, "SO_LINGER must be accepted");
    Linger read{};
    std::uint32_t size = sizeof(read);
    Require(getsockopt_nid_postfix(stream, SolSocket, SoLinger, &read, &size) == 0 && size == sizeof(read), "getsockopt SO_LINGER failed");
    Require(read.enabled == SoLinger && read.seconds == 5, "SO_LINGER reads back as {SO_LINGER bit, seconds}");
    linger = {0, 0};
    Require(setsockopt_nid_postfix(stream, SolSocket, SoLinger, &linger, sizeof(linger)) == 0, "SO_LINGER off must be accepted");
    size = sizeof(read);
    Require(getsockopt_nid_postfix(stream, SolSocket, SoLinger, &read, &size) == 0 && read.enabled == 0, "SO_LINGER must read back as off");
    linger = {1, -1};
    Require(SetFails(stream, SolSocket, SoLinger, &linger, sizeof(linger), ErrorDomain), "a negative linger time must fail with EDOM (33)");
    linger = {1, 65536};
    Require(SetFails(stream, SolSocket, SoLinger, &linger, sizeof(linger), ErrorDomain), "a linger time above 65535 must fail with EDOM (33)");
    Require(SetFails(stream, SolSocket, SoLinger, &linger, sizeof(std::int32_t), ErrorInvalid), "a linger value shorter than struct linger must fail with EINVAL (22)");
    Require(close_nid_postfix(stream) == 0, "close failed");
}

void LengthsFollowFreeBsd() {
    const int stream = socket_nid_postfix(2, Stream, 0);
    Require(stream >= 0, "socket failed");
    const int one = 1;
    Require(SetFails(stream, SolSocket, SoKeepAlive, &one, 2, ErrorInvalid), "an integer option shorter than an int must fail with EINVAL (22)");
    const std::array<std::int64_t, 1> wide{1};
    Require(setsockopt_nid_postfix(stream, SolSocket, SoKeepAlive, wide.data(), sizeof(wide)) == 0, "a value longer than an int is read from its first int");
    std::array<std::uint8_t, 2> narrow{0xaa, 0xaa};
    std::uint32_t size = narrow.size();
    Require(getsockopt_nid_postfix(stream, SolSocket, SoKeepAlive, narrow.data(), &size) == 0 && size == narrow.size(), "a short buffer gets a truncated value and its own length (sooptcopyout)");
    Require(narrow[0] == SoKeepAlive && narrow[1] == 0, "the truncated value is the low bytes of the int");
    const int zero = 0;
    Require(SetFails(stream, SolSocket, SoSendBuffer, &zero, sizeof(zero), ErrorInvalid), "SO_SNDBUF below 1 must fail with EINVAL (22)");
    const int negative = -5;
    Require(SetFails(stream, SolSocket, SoSendBuffer, &negative, sizeof(negative), ErrorInvalid), "a negative SO_SNDBUF must fail with EINVAL (22)");
    SetInt(stream, SoSendBuffer, 65536);
    Require(close_nid_postfix(stream) == 0, "close failed");
}

void BuffersFollowThePs5() {
    const int stream = socket_nid_postfix(2, Stream, 0);
    const int datagram = socket_nid_postfix(28, Datagram, 0);
    Require(stream >= 0 && datagram >= 0, "socket failed");
    Require(GetInt(stream, SoSendBuffer) == 32768 && GetInt(stream, SoReceiveBuffer) == 65536, "a stream socket starts with 32768 and 65536 bytes");
    Require(GetInt(stream, SoSendLowWater) == 2048 && GetInt(stream, SoReceiveLowWater) == 1, "the low-water marks start at 2048 and 1");
    Require(GetInt(datagram, SoSendBuffer) == 9216 && GetInt(datagram, SoReceiveBuffer) == 42080, "a datagram socket starts with 9216 and 42080 bytes");
    SetInt(stream, SoSendBuffer, 1000);
    Require(GetInt(stream, SoSendBuffer) == 1000 && GetInt(stream, SoSendLowWater) == 1000, "a smaller buffer is kept and lowers its low-water mark");
    SetInt(stream, SoReceiveBuffer, 524289);
    Require(GetInt(stream, SoReceiveBuffer) == 524288, "a buffer above 512 KiB is limited to 524288 bytes");
    SetInt(stream, SoSendBuffer, 5592405);
    Require(GetInt(stream, SoSendBuffer) == 524288, "the largest reservation is accepted and limited");
    const int tooLarge = 5592406;
    Require(SetFails(stream, SolSocket, SoSendBuffer, &tooLarge, sizeof(tooLarge), 55), "a reservation above sb_max must fail with ENOBUFS (55)");
    Require(GetInt(stream, SoSendBuffer) == 524288, "a failed reservation keeps the buffer");
    SetInt(stream, SoReceiveLowWater, 600000);
    Require(GetInt(stream, SoReceiveLowWater) == 524288, "a low-water mark above the buffer is limited to it");
    SetInt(stream, SoReceiveBuffer, 50);
    Require(GetInt(stream, SoReceiveLowWater) == 50, "a smaller buffer lowers the low-water mark");
    const int zero = 0;
    Require(SetFails(stream, SolSocket, SoReceiveLowWater, &zero, sizeof(zero), ErrorInvalid), "a low-water mark below 1 must fail with EINVAL (22)");
    int pair[2] = {-1, -1};
    Require(socketpair_nid_postfix(1, Stream, 0, pair) == 0, "socketpair failed");
    Require(GetInt(pair[0], SoSendBuffer) == 8192 && GetInt(pair[0], SoReceiveBuffer) == 8192, "an AF_UNIX stream socket starts with 8192 bytes each way");
    Require(close_nid_postfix(pair[0]) == 0 && close_nid_postfix(pair[1]) == 0, "close failed");
    Require(socketpair_nid_postfix(1, Datagram, 0, pair) == 0, "socketpair failed");
    Require(GetInt(pair[0], SoSendBuffer) == 2048 && GetInt(pair[0], SoReceiveBuffer) == 4096, "an AF_UNIX datagram socket starts with 2048 and 4096 bytes");
    Require(close_nid_postfix(pair[0]) == 0 && close_nid_postfix(pair[1]) == 0, "close failed");
    Require(close_nid_postfix(stream) == 0 && close_nid_postfix(datagram) == 0, "close failed");
    const int listener = socket_nid_postfix(2, Stream, 0);
    Require(listener >= 0, "socket failed");
    auto address = Loopback(0);
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0 && listen_nid_postfix(listener, 1) == 0, "listen failed");
    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &size) == 0, "getsockname failed");
    SetInt(listener, SoReceiveBuffer, 3000);
    SetInt(listener, SoSendLowWater, 700);
    const int client = socket_nid_postfix(2, Stream, 0);
    Require(client >= 0 && connect_nid_postfix(client, address.data(), address.size()) == 0, "connect failed");
    const int accepted = accept_nid_postfix(listener, nullptr, nullptr);
    Require(accepted >= 0, "accept failed");
    Require(GetInt(accepted, SoReceiveBuffer) == 3000 && GetInt(accepted, SoSendLowWater) == 700 && GetInt(accepted, SoSendBuffer) == 32768, "an accepted socket takes the listener's buffers");
    Require(close_nid_postfix(accepted) == 0 && close_nid_postfix(client) == 0 && close_nid_postfix(listener) == 0, "close failed");
}

void NullValues() {
    const int stream = socket_nid_postfix(2, Stream, 0);
    Require(stream >= 0, "socket failed");
    Require(SetFails(stream, SolSocket, SoKeepAlive, nullptr, sizeof(int), 14), "a null value with a length must fail with EFAULT (14)");
    Require(SetFails(-1, SolSocket, SoKeepAlive, nullptr, sizeof(int), 14), "the null value is checked before the descriptor");
    Require(SetFails(stream, SolSocket, 0x7fff, nullptr, sizeof(int), 14), "the null value is checked before the option");
    Require(SetFails(stream, SolSocket, SoKeepAlive, nullptr, 0, ErrorInvalid), "a null value without a length must fail with EINVAL (22)");
    std::uint32_t size = sizeof(int);
    Require(getsockopt_nid_postfix(stream, SolSocket, SoSendBuffer, nullptr, &size) == 0 && size == 0, "getsockopt without a value succeeds with length 0");
    Require(close_nid_postfix(stream) == 0, "close failed");
}

int GetIp(int descriptor, int option) {
    int value = -12345;
    std::uint32_t size = sizeof(value);
    Require(getsockopt_nid_postfix(descriptor, 0, option, &value, &size) == 0 && size == sizeof(value), "getsockopt of an IPPROTO_IP option failed");
    return value;
}

bool SetIp(int descriptor, int option, int value) {
    return setsockopt_nid_postfix(descriptor, 0, option, &value, sizeof(value)) == 0;
}

void IpLevelFollowsThePs5() {
    constexpr int IpTypeOfService = 3;
    constexpr int IpTimeToLive = 4;
    constexpr int IpMulticastTimeToLive = 10;
    constexpr int IpMulticastLoop = 11;
    const int datagram = socket_nid_postfix(2, Datagram, 0);
    const int datagram6 = socket_nid_postfix(28, Datagram, 0);
    Require(datagram >= 0 && datagram6 >= 0, "socket failed");
    Require(GetIp(datagram, IpTimeToLive) == 64 && GetIp(datagram, IpTypeOfService) == 0, "IP_TTL starts at 64 and IP_TOS at 0");
    Require(GetIp(datagram, IpMulticastTimeToLive) == 1 && GetIp(datagram, IpMulticastLoop) == 1, "IP_MULTICAST_TTL and IP_MULTICAST_LOOP start at 1");
    Require(SetIp(datagram, IpTimeToLive, 32) && GetIp(datagram, IpTimeToLive) == 32, "IP_TTL reads back");
    Require(SetIp(datagram, IpTimeToLive, 300) && GetIp(datagram, IpTimeToLive) == 44, "IP_TTL keeps the low byte");
    Require(SetIp(datagram, IpTimeToLive, -1) && GetIp(datagram, IpTimeToLive) == 255, "IP_TTL of -1 reads back as 255");
    Require(SetIp(datagram, IpTimeToLive, 0) && GetIp(datagram, IpTimeToLive) == 0, "IP_TTL of 0 is kept");
    const unsigned char byte = 9;
    Require(SetFails(datagram, 0, IpTimeToLive, &byte, 1, ErrorInvalid), "a one-byte IP_TTL must fail with EINVAL (22)");
    Require(SetIp(datagram, IpTypeOfService, 0x1ff) && GetIp(datagram, IpTypeOfService) == 0xff, "IP_TOS keeps the low byte");
    const unsigned char seven = 7;
    Require(setsockopt_nid_postfix(datagram, 0, IpMulticastTimeToLive, &seven, 1) == 0 && GetIp(datagram, IpMulticastTimeToLive) == 7, "a one-byte IP_MULTICAST_TTL is accepted");
    const int tooLarge = 300;
    Require(SetFails(datagram, 0, IpMulticastTimeToLive, &tooLarge, sizeof(tooLarge), ErrorInvalid) && GetIp(datagram, IpMulticastTimeToLive) == 7, "IP_MULTICAST_TTL above 255 must fail with EINVAL (22)");
    unsigned char narrow = 0xaa;
    std::uint32_t size = 1;
    Require(getsockopt_nid_postfix(datagram, 0, IpMulticastTimeToLive, &narrow, &size) == 0 && size == 1 && narrow == 7, "a one-byte read gets the IP_MULTICAST_TTL byte");
    Require(SetIp(datagram, IpMulticastLoop, 5) && GetIp(datagram, IpMulticastLoop) == 1, "IP_MULTICAST_LOOP reads back as 1");
    Require(SetIp(datagram, IpMulticastLoop, 0) && GetIp(datagram, IpMulticastLoop) == 0, "IP_MULTICAST_LOOP can be turned off");
    Require(SetIp(datagram, IpMulticastLoop, 1) && SetIp(datagram, IpMulticastLoop, 0x100) && GetIp(datagram, IpMulticastLoop) == 0, "an int IP_MULTICAST_LOOP keeps only its low byte");
    int value = 0;
    size = sizeof(value);
    Require(getsockopt_nid_postfix(datagram6, 0, IpTimeToLive, &value, &size) == -1 && *__error_nid_postfix() == ErrorInvalid, "IPPROTO_IP on an AF_INET6 socket must fail with EINVAL (22)");
    size = sizeof(value);
    Require(getsockopt_nid_postfix(datagram, 0, 0x7777, &value, &size) == -1 && *__error_nid_postfix() == ErrorNoProtocolOption, "an unknown IPPROTO_IP option must fail with ENOPROTOOPT (42)");
    size = sizeof(value);
    Require(getsockopt_nid_postfix(datagram, 99, 1, &value, &size) == -1 && *__error_nid_postfix() == ErrorInvalid, "an unknown level must fail with EINVAL (22)");
    Require(SetFails(datagram, 99, 1, &value, sizeof(value), ErrorInvalid), "setsockopt at an unknown level must fail with EINVAL (22)");
    Require(close_nid_postfix(datagram) == 0 && close_nid_postfix(datagram6) == 0, "close failed");
}

void MulticastMembership() {
    constexpr int IpMulticastInterface = 9;
    constexpr int IpAddMembership = 12;
    constexpr int IpDropMembership = 13;
    constexpr int ErrorNotSupported = 45;
    constexpr int ErrorAddressNotAvailable = 49;
    const int datagram = socket_nid_postfix(2, Datagram, 0);
    const int stream = socket_nid_postfix(2, Stream, 0);
    Require(datagram >= 0 && stream >= 0, "socket failed");
    const std::array<std::uint8_t, 8> group{224, 0, 0, 251, 0, 0, 0, 0};
    const std::array<std::uint8_t, 8> unicast{10, 0, 0, 1, 0, 0, 0, 0};
    const std::array<std::uint8_t, 8> other{224, 0, 0, 253, 0, 0, 0, 0};
    Require(SetFails(stream, 0, IpAddMembership, group.data(), group.size(), ErrorNotSupported), "a stream socket must fail a membership with EOPNOTSUPP (45)");
    Require(SetFails(datagram, 0, IpAddMembership, group.data(), 7, ErrorInvalid), "a short membership must fail with EINVAL (22)");
    Require(SetFails(datagram, 0, IpAddMembership, unicast.data(), unicast.size(), ErrorInvalid), "a group that is not multicast must fail with EINVAL (22)");
    Require(SetFails(datagram, 0, IpDropMembership, other.data(), other.size(), ErrorAddressNotAvailable), "dropping a group never joined must fail with EADDRNOTAVAIL (49)");
    std::array<std::uint8_t, 4> address{0xaa, 0xaa, 0xaa, 0xaa};
    std::uint32_t size = address.size();
    Require(getsockopt_nid_postfix(datagram, 0, IpMulticastInterface, address.data(), &size) == 0 && size == 4 && address == std::array<std::uint8_t, 4>{}, "IP_MULTICAST_IF starts at INADDR_ANY");
    const std::array<std::uint8_t, 4> loopback{127, 0, 0, 1};
    const std::array<std::uint8_t, 12> anyIndex{};
    Require(setsockopt_nid_postfix(datagram, 0, IpMulticastInterface, anyIndex.data(), anyIndex.size()) == 0, "an ip_mreqn without an index selects the default interface");
    size = address.size();
    Require(getsockopt_nid_postfix(datagram, 0, IpMulticastInterface, address.data(), &size) == 0 && address == std::array<std::uint8_t, 4>{}, "the default interface reads back as INADDR_ANY");
    Require(SetFails(datagram, 0, IpMulticastInterface, loopback.data(), 2, ErrorInvalid), "a short IP_MULTICAST_IF must fail with EINVAL (22)");
    Require(close_nid_postfix(datagram) == 0 && close_nid_postfix(stream) == 0, "close failed");
}

void MulticastOnTheHost() {
    constexpr int IpMulticastInterface = 9;
    constexpr int IpAddMembership = 12;
    constexpr int IpDropMembership = 13;
    constexpr int ErrorAddressInUse = 48;
    constexpr int ErrorAddressNotAvailable = 49;
    const int datagram = socket_nid_postfix(2, Datagram, 0);
    Require(datagram >= 0, "socket failed");
    const std::array<std::uint8_t, 8> group{224, 0, 0, 251, 0, 0, 0, 0};
    Require(setsockopt_nid_postfix(datagram, 0, IpAddMembership, group.data(), group.size()) == 0, "joining 224.0.0.251 through INADDR_ANY failed");
    Require(SetFails(datagram, 0, IpAddMembership, group.data(), group.size(), ErrorAddressInUse), "joining twice must fail with EADDRINUSE (48)");
    Require(setsockopt_nid_postfix(datagram, 0, IpDropMembership, group.data(), group.size()) == 0, "drop failed");
    Require(SetFails(datagram, 0, IpDropMembership, group.data(), group.size(), ErrorAddressNotAvailable), "dropping twice must fail with EADDRNOTAVAIL (49)");
    const std::array<std::uint8_t, 8> throughLoopback{224, 0, 0, 252, 127, 0, 0, 1};
    const std::array<std::uint8_t, 8> anyInterface{224, 0, 0, 252, 0, 0, 0, 0};
    Require(setsockopt_nid_postfix(datagram, 0, IpAddMembership, throughLoopback.data(), throughLoopback.size()) == 0, "joining 224.0.0.252 through 127.0.0.1 failed");
    Require(setsockopt_nid_postfix(datagram, 0, IpDropMembership, anyInterface.data(), anyInterface.size()) == 0, "a drop through INADDR_ANY leaves the group on any interface");
    Require(SetFails(datagram, 0, IpDropMembership, anyInterface.data(), anyInterface.size(), ErrorAddressNotAvailable), "dropping it again must fail with EADDRNOTAVAIL (49)");
    const std::array<std::uint8_t, 4> loopback{127, 0, 0, 1};
    Require(setsockopt_nid_postfix(datagram, 0, IpMulticastInterface, loopback.data(), loopback.size()) == 0, "selecting the loopback interface for IP_MULTICAST_IF failed");
    std::array<std::uint8_t, 4> address{0xaa, 0xaa, 0xaa, 0xaa};
    std::uint32_t size = address.size();
    Require(getsockopt_nid_postfix(datagram, 0, IpMulticastInterface, address.data(), &size) == 0 && size == 4 && address == loopback, "IP_MULTICAST_IF reads back");
    const std::array<std::uint8_t, 12> anyIndex{};
    Require(setsockopt_nid_postfix(datagram, 0, IpMulticastInterface, anyIndex.data(), anyIndex.size()) == 0, "an ip_mreqn without an index selects the default interface");
    size = address.size();
    Require(getsockopt_nid_postfix(datagram, 0, IpMulticastInterface, address.data(), &size) == 0 && address == std::array<std::uint8_t, 4>{}, "the default interface reads back as INADDR_ANY");
    Require(close_nid_postfix(datagram) == 0, "close failed");
}

void UnknownRequestsFail() {
    const int stream = socket_nid_postfix(2, Stream, 0);
    Require(stream >= 0, "socket failed");
    const int one = 1;
    Require(SetFails(stream, SolSocket, 0x7fff, &one, sizeof(one), ErrorNoProtocolOption), "an unknown SOL_SOCKET option must fail with ENOPROTOOPT (42)");
    int value = 0;
    std::uint32_t size = sizeof(value);
    Require(getsockopt_nid_postfix(stream, SolSocket, 0x7fff, &value, &size) == -1 && *__error_nid_postfix() == ErrorNoProtocolOption, "getsockopt of an unknown option must fail with ENOPROTOOPT (42)");
    Require(setsockopt_nid_postfix(-1, SolSocket, SoKeepAlive, &one, sizeof(one)) == -1 && *__error_nid_postfix() == ErrorBadDescriptor, "setsockopt on a bad descriptor must fail with EBADF (9)");
    Require(close_nid_postfix(stream) == 0, "close failed");
}

}

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "multicast") == 0) {
        MulticastOnTheHost();
        return 0;
    }
    ReportsTypeAndListening();
    ReportsAndClearsTheConnectError();
    BooleanOptionsReportTheirBit();
    ReusePortSharesAPort();
    LingerFollowsFreeBsd();
    LengthsFollowFreeBsd();
    BuffersFollowThePs5();
    NullValues();
    IpLevelFollowsThePs5();
    MulticastMembership();
    UnknownRequestsFail();
    return 0;
}
