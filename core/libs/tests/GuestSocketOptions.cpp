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
constexpr int SoError = 0x1007;
constexpr int SoType = 0x1008;
constexpr int ErrorFault = 14;
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
    for (const int option : {SoReusePort, SoDebug, SoUseLoopback, SoTimestamp}) {
        const int one = 1;
        Require(SetFails(stream, SolSocket, option, &one, sizeof(one), ErrorNoProtocolOption), "an option no title needs must fail with ENOPROTOOPT (42)");
        int value = 0;
        std::uint32_t size = sizeof(value);
        Require(getsockopt_nid_postfix(stream, SolSocket, option, &value, &size) == -1 && *__error_nid_postfix() == ErrorNoProtocolOption, "getsockopt of an option no title needs must fail with ENOPROTOOPT (42)");
    }
    Require(close_nid_postfix(stream) == 0 && close_nid_postfix(datagram) == 0, "close failed");
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

void NullPointersFollowFreeBsd() {
    const int stream = socket_nid_postfix(2, Stream, 0);
    Require(stream >= 0, "socket failed");
    const int one = 1;
    int value = 0;
    std::uint32_t size = sizeof(value);
    Require(setsockopt_nid_postfix(stream, SolSocket, SoKeepAlive, nullptr, sizeof(one)) == -1 && *__error_nid_postfix() == ErrorFault, "setsockopt with a null value and a length must fail with EFAULT (14)");
    Require(setsockopt_nid_postfix(-1, SolSocket, SoKeepAlive, nullptr, sizeof(one)) == -1 && *__error_nid_postfix() == ErrorFault, "setsockopt checks the value pointer before the descriptor (EFAULT before EBADF)");
    Require(setsockopt_nid_postfix(stream, SolSocket, SoKeepAlive, &one, 0xffffffffu) == -1 && *__error_nid_postfix() == ErrorInvalid, "setsockopt with a negative length must fail with EINVAL (22)");
    Require(setsockopt_nid_postfix(stream, SolSocket, SoKeepAlive, nullptr, 0) == -1 && *__error_nid_postfix() == ErrorInvalid, "setsockopt with a null value and no length must fail with EINVAL (22)");
    Require(getsockopt_nid_postfix(stream, SolSocket, SoKeepAlive, &value, nullptr) == -1 && *__error_nid_postfix() == ErrorFault, "getsockopt with a value and no length pointer must fail with EFAULT (14)");
    Require(getsockopt_nid_postfix(-1, SolSocket, SoKeepAlive, &value, nullptr) == -1 && *__error_nid_postfix() == ErrorFault, "getsockopt checks the length pointer before the descriptor (EFAULT before EBADF)");
    Require(getsockopt_nid_postfix(stream, SolSocket, SoKeepAlive, nullptr, nullptr) == -1 && *__error_nid_postfix() == ErrorFault, "getsockopt with neither pointer fails with EFAULT (14) on the final copyout");
    Require(getsockopt_nid_postfix(-1, SolSocket, SoKeepAlive, nullptr, nullptr) == -1 && *__error_nid_postfix() == ErrorBadDescriptor, "getsockopt with neither pointer on a bad descriptor must fail with EBADF (9) first");
    Require(getsockopt_nid_postfix(stream, SolSocket, 0x7fff, nullptr, nullptr) == -1 && *__error_nid_postfix() == ErrorNoProtocolOption, "getsockopt with neither pointer on an unknown option must fail with ENOPROTOOPT (42) first");
    size = 0xffffffffu;
    Require(getsockopt_nid_postfix(stream, SolSocket, SoKeepAlive, &value, &size) == -1 && *__error_nid_postfix() == ErrorInvalid, "getsockopt with a negative length must fail with EINVAL (22)");
    size = 0xffffffffu;
    Require(getsockopt_nid_postfix(-1, SolSocket, SoKeepAlive, &value, &size) == -1 && *__error_nid_postfix() == ErrorInvalid, "getsockopt checks a negative length before the descriptor (EINVAL before EBADF)");
    size = 0xffffffffu;
    Require(getsockopt_nid_postfix(stream, 0, 1, &value, &size) == -1 && *__error_nid_postfix() == ErrorInvalid, "getsockopt checks a negative length before the level (EINVAL before ENOPROTOOPT)");
    size = sizeof(value);
    Require(getsockopt_nid_postfix(stream, SolSocket, SoKeepAlive, nullptr, &size) == 0 && size == 0, "getsockopt with a null value reports a zero length");
    Require(close_nid_postfix(stream) == 0, "close failed");
}

}

int main() {
    ReportsTypeAndListening();
    ReportsAndClearsTheConnectError();
    BooleanOptionsReportTheirBit();
    LingerFollowsFreeBsd();
    LengthsFollowFreeBsd();
    UnknownRequestsFail();
    NullPointersFollowFreeBsd();
    return 0;
}
