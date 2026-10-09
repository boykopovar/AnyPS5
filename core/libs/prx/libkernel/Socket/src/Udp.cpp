#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <poll.h>
#include <unistd.h>
#include <cerrno>
#endif
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"
#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstring>
#include <new>
#include <map>
#include <memory>
#include <mutex>
#include <cstdarg>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

extern "C" int* APS5_VABI __error_nid_postfix();
namespace {
#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr auto Invalid = INVALID_SOCKET;
#else
using NativeSocket = int;
constexpr auto Invalid = -1;
#endif
int Fail(int error) { *__error_nid_postfix() = error; return -1; }
constexpr int GuestNoSignal = 0x20000;
int NativeSendFlags(int flags) {
#ifdef _WIN32
    static_cast<void>(flags);
    return 0;
#else
    return flags & GuestNoSignal ? MSG_NOSIGNAL : 0;
#endif
}
int NativeError() {
#ifdef _WIN32
    switch (WSAGetLastError()) {
        case WSAEWOULDBLOCK: return 35;
        case WSAEADDRINUSE: return 48;
        case WSAEADDRNOTAVAIL: return 49;
        case WSAEACCES: return 13;
        case WSAEMSGSIZE: return 40;
        case WSAENETUNREACH: return 51;
        case WSAEHOSTUNREACH: return 65;
        case WSAECONNRESET: return 54;
        case WSAECONNABORTED: return 53;
        case WSAEISCONN: return 56;
        case WSAENOTCONN: return 57;
        case WSAENOBUFS: return 55;
        case WSAETIMEDOUT: return 60;
        case WSAECONNREFUSED: return 61;
        case WSAEINTR: return 4;
        case WSAEINVAL: return 22;
        default: return 5;
    }
#else
    switch (errno) {
        case EAGAIN: return 35;
        case EADDRINUSE: return 48;
        case EADDRNOTAVAIL: return 49;
        case EACCES: return 13;
        case EMSGSIZE: return 40;
        case ENETUNREACH: return 51;
        case EHOSTUNREACH: return 65;
        case ECONNREFUSED: return 61;
        case ECONNRESET: return 54;
        case ECONNABORTED: return 53;
        case EISCONN: return 56;
        case ENOTCONN: return 57;
        case ENOBUFS: return 55;
        case ETIMEDOUT: return 60;
        case EINTR: return 4;
        case EINVAL: return 22;
        case EPIPE: return 32;
        default: return 5;
    }
#endif
}
struct Socket {
    NativeSocket value;
    int family;
    int type;
    std::mutex modeMutex;
    bool nonblocking = false;
    Socket(NativeSocket value, int family, int type) : value(value), family(family), type(type) {}
    ~Socket() {
        if (value == Invalid) return;
#ifdef _WIN32
        closesocket(value);
#else
        ::close(value);
#endif
    }
};
std::mutex socketsMutex;
std::map<int, std::shared_ptr<Socket>> sockets;
int nextDescriptor = GuestSockets::FirstDescriptor;
std::shared_ptr<Socket> Lookup(int descriptor) {
    std::lock_guard lock(socketsMutex);
    const auto found = sockets.find(descriptor);
    if (found != sockets.end()) return found->second;
    Fail(9);
    return {};
}
int Option(int guest) {
    switch (guest) {
        case 0x4: return SO_REUSEADDR;
        case 0x20: return SO_BROADCAST;
        case 0x1001: return SO_SNDBUF;
        case 0x1002: return SO_RCVBUF;
        default: return -1;
    }
}
bool Address(const void* input, std::uint32_t length, sockaddr_storage& native, socklen_t& size) {
    if (!input || length < 2) { Fail(14); return false; }
    const auto* bytes = static_cast<const unsigned char*>(input);
    if (bytes[1] == 2 && length >= 16) {
        auto& v4 = reinterpret_cast<sockaddr_in&>(native);
        v4.sin_family = AF_INET;
        std::memcpy(&v4.sin_port, bytes + 2, 2);
        std::memcpy(&v4.sin_addr, bytes + 4, 4);
        size = sizeof(v4);
        return true;
    }
    if (bytes[1] == 28 && length >= 28) {
        auto& v6 = reinterpret_cast<sockaddr_in6&>(native);
        v6.sin6_family = AF_INET6;
        std::memcpy(&v6.sin6_port, bytes + 2, 2);
        std::memcpy(&v6.sin6_flowinfo, bytes + 4, 4);
        std::memcpy(&v6.sin6_addr, bytes + 8, 16);
        std::memcpy(&v6.sin6_scope_id, bytes + 24, 4);
        size = sizeof(v6);
        return true;
    }
    Fail(47);
    return false;
}
void GuestAddress(const sockaddr_storage& native, void* output, std::uint32_t* length) {
    unsigned char bytes[28]{};
    bytes[0] = native.ss_family == AF_INET ? 16 : 28;
    bytes[1] = native.ss_family == AF_INET ? 2 : 28;
    if (native.ss_family == AF_INET) {
        const auto& v4 = reinterpret_cast<const sockaddr_in&>(native);
        std::memcpy(bytes + 2, &v4.sin_port, 2);
        std::memcpy(bytes + 4, &v4.sin_addr, 4);
    } else {
        const auto& v6 = reinterpret_cast<const sockaddr_in6&>(native);
        std::memcpy(bytes + 2, &v6.sin6_port, 2);
        std::memcpy(bytes + 4, &v6.sin6_flowinfo, 4);
        std::memcpy(bytes + 8, &v6.sin6_addr, 16);
        std::memcpy(bytes + 24, &v6.sin6_scope_id, 4);
    }
    std::memcpy(output, bytes, std::min<std::uint32_t>(*length, bytes[0]));
    *length = bytes[0];
}
}

int GuestSockets::Close(int descriptor) {
    std::lock_guard lock(socketsMutex);
    return sockets.erase(descriptor) ? 0 : Fail(9);
}

bool GuestSockets::IsOpen(int descriptor) {
    std::lock_guard lock(socketsMutex);
    return sockets.contains(descriptor);
}

namespace {
#ifdef _WIN32
using NativePollDescriptor = WSAPOLLFD;
#else
using NativePollDescriptor = pollfd;
#endif
struct GuestPollDescriptor {
    int descriptor;
    short events;
    short revents;
};
struct PollFlag {
    short guest;
    short native;
};
constexpr short GuestPollPriority = 0x2;
constexpr short GuestPollInvalid = 0x20;
constexpr short GuestPollAccepted = 0x1 | 0x2 | 0x4 | 0x8 | 0x10 | 0x20 | 0x40 | 0x80 | 0x100;
constexpr PollFlag RequestFlags[] = {
    {0x1, POLLIN}, {0x2, POLLPRI}, {0x4, POLLOUT}, {0x40, POLLRDNORM}, {0x80, POLLRDBAND}, {0x100, POLLWRBAND}};
constexpr PollFlag StatusFlags[] = {{0x8, POLLERR}, {0x10, POLLHUP}, {GuestPollInvalid, POLLNVAL}};
std::shared_ptr<Socket> Find(int descriptor) {
    std::lock_guard lock(socketsMutex);
    const auto found = sockets.find(descriptor);
    return found != sockets.end() ? found->second : nullptr;
}
}

namespace {
constexpr int GuestUnix = 1;
constexpr int GuestCloseOnExec = 0x10000000;
constexpr int GuestNonBlocking = 0x20000000;
int PairError() {
#ifdef _WIN32
    switch (WSAGetLastError()) {
        case WSAEMFILE: return 24;
        default: return NativeError();
    }
#else
    switch (errno) {
        case EMFILE: return 24;
        case ENFILE: return 23;
        case ENOMEM: return 12;
        default: return NativeError();
    }
#endif
}
int SetNonBlocking(Socket& socket) {
#ifdef _WIN32
    unsigned long value = 1;
    if (ioctlsocket(socket.value, FIONBIO, &value)) return Fail(NativeError());
#else
    int value = 1;
    if (::ioctl(socket.value, FIONBIO, &value)) return Fail(NativeError());
#endif
    socket.nonblocking = true;
    return 0;
}
#ifdef _WIN32
bool LoopbackName(NativeSocket socket, sockaddr_in& name) {
    int size = sizeof(name);
    return getsockname(socket, reinterpret_cast<sockaddr*>(&name), &size) == 0;
}
int NativePair(int type, NativeSocket (&pair)[2]) {
    static const int startup = [] { WSADATA data{}; return WSAStartup(MAKEWORD(2, 2), &data); }();
    if (startup) return 5;
    if (type == 5) throw std::runtime_error("socketpair: SOCK_SEQPACKET is not supported on Windows");
    sockaddr_in loopback{};
    loopback.sin_family = AF_INET;
    loopback.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const int nativeType = type == 1 ? SOCK_STREAM : SOCK_DGRAM;
    Socket first(::socket(AF_INET, nativeType, 0), 2, type);
    if (first.value == Invalid) return PairError();
    const int exclusive = 1;
    setsockopt(first.value, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive), sizeof(exclusive));
    if (bind(first.value, reinterpret_cast<sockaddr*>(&loopback), sizeof(loopback))) return PairError();
    sockaddr_in firstName{};
    if (!LoopbackName(first.value, firstName)) return PairError();
    Socket second(::socket(AF_INET, nativeType, 0), 2, type);
    if (second.value == Invalid) return PairError();
    if (type == 1) {
        if (listen(first.value, 1)) return PairError();
        if (connect(second.value, reinterpret_cast<sockaddr*>(&firstName), sizeof(firstName))) return PairError();
        sockaddr_in peer{};
        int peerSize = sizeof(peer);
        Socket accepted(accept(first.value, reinterpret_cast<sockaddr*>(&peer), &peerSize), 2, type);
        if (accepted.value == Invalid) return PairError();
        sockaddr_in secondName{};
        if (!LoopbackName(second.value, secondName)) return PairError();
        if (peer.sin_port != secondName.sin_port) return 53;
        const int noDelay = 1;
        setsockopt(second.value, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));
        setsockopt(accepted.value, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));
        pair[0] = second.value;
        pair[1] = accepted.value;
        second.value = Invalid;
        accepted.value = Invalid;
        return 0;
    }
    if (bind(second.value, reinterpret_cast<sockaddr*>(&loopback), sizeof(loopback))) return PairError();
    sockaddr_in secondName{};
    if (!LoopbackName(second.value, secondName)) return PairError();
    if (connect(first.value, reinterpret_cast<sockaddr*>(&secondName), sizeof(secondName))) return PairError();
    if (connect(second.value, reinterpret_cast<sockaddr*>(&firstName), sizeof(firstName))) return PairError();
    pair[0] = first.value;
    pair[1] = second.value;
    first.value = Invalid;
    second.value = Invalid;
    return 0;
}
#else
int NativePair(int type, NativeSocket (&pair)[2]) {
    const int nativeType = type == 1 ? SOCK_STREAM : type == 2 ? SOCK_DGRAM : SOCK_SEQPACKET;
    int native[2];
    if (::socketpair(AF_UNIX, nativeType, 0, native)) return PairError();
    pair[0] = native[0];
    pair[1] = native[1];
    return 0;
}
#endif
int UnnamedAddress(void* address, std::uint32_t* length) {
    const unsigned char bytes[16] = {16, GuestUnix};
    *length = std::min<std::uint32_t>(*length, sizeof(bytes));
    std::memcpy(address, bytes, *length);
    return 0;
}
}

extern "C" {
int APS5_VABI fcntl_nid_postfix(int descriptor, int command, ...) {
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    std::lock_guard lock(socket->modeMutex);
    if (command == 3) return 2 | (socket->nonblocking ? 4 : 0); // F_GETFL, O_RDWR
    if (command != 4) return Fail(22);
#ifdef _WIN32
    __builtin_sysv_va_list arguments;
    __builtin_sysv_va_start(arguments, command);
    const int flags = __builtin_va_arg(arguments, int);
    __builtin_sysv_va_end(arguments);
#else
    std::va_list arguments;
    va_start(arguments, command);
    const int flags = va_arg(arguments, int);
    va_end(arguments);
#endif
    if ((flags & ~7) != 0) return Fail(45);
#ifdef _WIN32
    unsigned long enabled = (flags & 4) != 0;
    if (ioctlsocket(socket->value, FIONBIO, &enabled)) return Fail(NativeError());
#else
    int enabled = (flags & 4) != 0;
    if (::ioctl(socket->value, FIONBIO, &enabled)) return Fail(NativeError());
#endif
    socket->nonblocking = enabled != 0;
    return 0;
}
int APS5_VABI setsockopt_nid_postfix(int descriptor, int level, int option,
                                    const void* value, std::uint32_t length) {
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    const int nativeOption = Option(option);
    if (level != 0xffff || nativeOption == -1) return Fail(42);
    if (!value || length != sizeof(int)) return Fail(22);
    return ::setsockopt(socket->value, SOL_SOCKET, nativeOption,
        static_cast<const char*>(value), sizeof(int)) ? Fail(NativeError()) : 0;
}
int APS5_VABI getsockopt_nid_postfix(int descriptor, int level, int option,
                                    void* value, std::uint32_t* length) {
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    const int nativeOption = Option(option);
    if (level != 0xffff || nativeOption == -1) return Fail(42);
    if (!value || !length || *length < sizeof(int)) return Fail(22);
    int result = 0;
    socklen_t size = sizeof(result);
    if (::getsockopt(socket->value, SOL_SOCKET, nativeOption, reinterpret_cast<char*>(&result), &size))
        return Fail(NativeError());
    std::memcpy(value, &result, sizeof(result));
    *length = sizeof(result);
    return 0;
}
int APS5_VABI socket_nid_postfix(int family, int type, int protocol) {
    if (family != 2 && family != 28) return Fail(47);
    const int nativeType = type == 1 ? SOCK_STREAM : type == 2 ? SOCK_DGRAM : -1;
    const int expectedProtocol = type == 1 ? 6 : 17;
    if (nativeType == -1 || (protocol != 0 && protocol != expectedProtocol)) return Fail(43);
#ifdef _WIN32
    static const int startup = [] { WSADATA data{}; return WSAStartup(MAKEWORD(2, 2), &data); }();
    if (startup) return Fail(5);
#endif
    const auto native = ::socket(family == 2 ? AF_INET : AF_INET6, nativeType, protocol);
    if (native == Invalid) return Fail(NativeError());
    Socket guard(native, family, type);
    try {
        auto socket = std::make_shared<Socket>(native, family, type);
        guard.value = Invalid;
        std::lock_guard lock(socketsMutex);
        if (nextDescriptor == INT_MAX) return Fail(24);
        const int descriptor = nextDescriptor++;
        sockets.emplace(descriptor, std::move(socket));
        return descriptor;
    } catch (const std::bad_alloc&) {
        return Fail(12);
    }
}
int APS5_VABI socketpair_nid_postfix(int family, int type, int protocol, int* descriptors) {
    const bool nonblocking = (type & GuestNonBlocking) != 0;
    const int plainType = type & ~(GuestCloseOnExec | GuestNonBlocking);
    if (family != GuestUnix && family != 2 && family != 28) return Fail(47);
    if (plainType == 0) return Fail(43);
    if (family != GuestUnix) {
        if (plainType != 1 && plainType != 2) return Fail(41);
        return Fail(protocol == 0 || protocol == (plainType == 1 ? 6 : 17) ? 45 : 43);
    }
    if (protocol != 0) return Fail(43);
    if (plainType != 1 && plainType != 2 && plainType != 5) return Fail(41);
    if (!descriptors) return Fail(14);
    const GuestArena::HostWrite destination(descriptors, 2 * sizeof(int));
    if (!destination.Open()) return Fail(14);
    NativeSocket native[2];
    if (const int error = NativePair(plainType, native)) return Fail(error);
    Socket firstGuard(native[0], GuestUnix, plainType);
    Socket secondGuard(native[1], GuestUnix, plainType);
    try {
        auto first = std::make_shared<Socket>(native[0], GuestUnix, plainType);
        firstGuard.value = Invalid;
        auto second = std::make_shared<Socket>(native[1], GuestUnix, plainType);
        secondGuard.value = Invalid;
        if (nonblocking && (SetNonBlocking(*first) || SetNonBlocking(*second))) return -1;
        std::lock_guard lock(socketsMutex);
        if (nextDescriptor >= INT_MAX - 1) return Fail(24);
        const int firstDescriptor = nextDescriptor++;
        const int secondDescriptor = nextDescriptor++;
        sockets.emplace(firstDescriptor, std::move(first));
        try {
            sockets.emplace(secondDescriptor, std::move(second));
        } catch (const std::bad_alloc&) {
            sockets.erase(firstDescriptor);
            return Fail(12);
        }
        descriptors[0] = firstDescriptor;
        descriptors[1] = secondDescriptor;
        return 0;
    } catch (const std::bad_alloc&) {
        return Fail(12);
    }
}
int APS5_VABI connect_nid_postfix(int descriptor, const void* address, std::uint32_t length) {
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    sockaddr_storage native{};
    socklen_t size;
    if (!Address(address, length, native, size)) return -1;
    if (native.ss_family != (socket->family == 2 ? AF_INET : AF_INET6)) return Fail(47);
    return ::connect(socket->value, reinterpret_cast<sockaddr*>(&native), size) ? Fail(NativeError()) : 0;
}
int APS5_VABI listen_nid_postfix(int descriptor, int backlog) {
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if (socket->type != 1) return Fail(45);
    return ::listen(socket->value, backlog) ? Fail(NativeError()) : 0;
}
int APS5_VABI accept_nid_postfix(int descriptor, void* address, std::uint32_t* length) {
    const auto listener = Lookup(descriptor);
    if (!listener) return -1;
    if (listener->type != 1 || (address && !length)) return Fail(22);
    sockaddr_storage peer{};
    socklen_t size = sizeof(peer);
    const auto native = ::accept(listener->value, address ? reinterpret_cast<sockaddr*>(&peer) : nullptr,
        address ? &size : nullptr);
    if (native == Invalid) return Fail(NativeError());
    Socket guard(native, listener->family, listener->type);
    try {
        auto accepted = std::make_shared<Socket>(native, listener->family, listener->type);
        guard.value = Invalid;
        std::lock_guard lock(socketsMutex);
        if (nextDescriptor == INT_MAX) return Fail(24);
        const int acceptedDescriptor = nextDescriptor++;
        sockets.emplace(acceptedDescriptor, std::move(accepted));
        if (address) GuestAddress(peer, address, length);
        return acceptedDescriptor;
    } catch (const std::bad_alloc&) {
        return Fail(12);
    }
}
std::int64_t APS5_VABI send_nid_postfix(int descriptor, const void* buffer, std::uint64_t length, int flags) {
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if ((flags & ~GuestNoSignal) != 0) return Fail(45);
    if (length > INT_MAX) return Fail(40);
    if (!buffer && length) return Fail(14);
    const auto result = ::send(socket->value, static_cast<const char*>(buffer), static_cast<int>(length), NativeSendFlags(flags));
    return result < 0 ? Fail(NativeError()) : result;
}
std::int64_t APS5_VABI recv_nid_postfix(int descriptor, void* buffer, std::uint64_t length, int flags) {
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if ((flags & ~2) != 0) return Fail(45);
    if (length > INT_MAX) return Fail(40);
    if (!buffer && length) return Fail(14);
    const auto result = ::recv(socket->value, static_cast<char*>(buffer), static_cast<int>(length),
        flags & 2 ? MSG_PEEK : 0);
    return result < 0 ? Fail(NativeError()) : result;
}
int APS5_VABI bind_nid_postfix(int descriptor, const void* address, std::uint32_t length) {
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    sockaddr_storage native{};
    socklen_t size;
    if (!Address(address, length, native, size)) return -1;
    return ::bind(socket->value, reinterpret_cast<sockaddr*>(&native), size) ? Fail(NativeError()) : 0;
}
int APS5_VABI shutdown_nid_postfix(int descriptor, int how) {
    const auto socket = Lookup(descriptor);
    if (!socket) return Fail(9);
    if (how < 0 || how > 2) return Fail(22);
    return ::shutdown(socket->value, how) ? Fail(NativeError()) : 0;
}
int APS5_VABI getsockname_nid_postfix(int descriptor, void* address, std::uint32_t* length) {
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if (!address || !length) return Fail(14);
    if (socket->family == GuestUnix) return UnnamedAddress(address, length);
    sockaddr_storage native{};
    socklen_t size = sizeof(native);
    if (::getsockname(socket->value, reinterpret_cast<sockaddr*>(&native), &size)) return Fail(NativeError());
    GuestAddress(native, address, length);
    return 0;
}
int APS5_VABI getpeername_nid_postfix(int descriptor, void* address, std::uint32_t* length) {
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if (!address || !length) return Fail(14);
    if (socket->family == GuestUnix) return UnnamedAddress(address, length);
    sockaddr_storage native{};
    socklen_t size = sizeof(native);
    if (::getpeername(socket->value, reinterpret_cast<sockaddr*>(&native), &size)) return Fail(NativeError());
    GuestAddress(native, address, length);
    return 0;
}
int APS5_VABI ioctl_nid_postfix(int descriptor, std::uint64_t request, void* argument) {
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if (!argument) return Fail(14);
    if (request != 0x8004667e && request != 0x4004667f) return Fail(25);
    std::lock_guard lock(socket->modeMutex);
    unsigned long value = 0;
    if (request == 0x8004667e) value = *static_cast<int*>(argument) != 0;
#ifdef _WIN32
    const auto result = ioctlsocket(socket->value, request == 0x8004667e ? FIONBIO : FIONREAD, &value);
#else
    int nativeValue = static_cast<int>(value);
    const auto result = ::ioctl(socket->value, request == 0x8004667e ? FIONBIO : FIONREAD, &nativeValue);
    value = static_cast<unsigned long>(nativeValue);
#endif
    if (result) return Fail(NativeError());
    if (request == 0x8004667e) socket->nonblocking = value != 0;
    if (request == 0x4004667f) *static_cast<int*>(argument) = static_cast<int>(value);
    return 0;
}
std::int64_t APS5_VABI sendto_nid_postfix(int descriptor, const void* buffer, std::uint64_t length,
    int flags, const void* address, std::uint32_t addressLength) {
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if ((flags & ~GuestNoSignal) != 0) return Fail(45);
    if (length > INT_MAX) return Fail(40);
    if (!buffer && length) return Fail(14);
    int result;
    if (!address) {
        if (addressLength != 0) return Fail(22);
        result = ::send(socket->value, static_cast<const char*>(buffer), static_cast<int>(length), NativeSendFlags(flags));
    } else {
        sockaddr_storage native{};
        socklen_t size;
        if (!Address(address, addressLength, native, size)) return -1;
        if (native.ss_family != (socket->family == 2 ? AF_INET : AF_INET6)) return Fail(47);
        result = static_cast<int>(::sendto(socket->value, static_cast<const char*>(buffer), static_cast<int>(length), NativeSendFlags(flags),
            reinterpret_cast<sockaddr*>(&native), size));
    }
    return result < 0 ? Fail(NativeError()) : result;
}
std::int64_t APS5_VABI recvfrom_nid_postfix(int descriptor, void* buffer, std::uint64_t length,
    int flags, void* address, std::uint32_t* addressLength) {
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if ((flags & ~2) != 0) return Fail(45);
    if (length > INT_MAX) return Fail(40);
    if ((!buffer && length) || (address && !addressLength)) return Fail(14);
    sockaddr_storage native{};
    socklen_t size = sizeof(native);
    const auto result = ::recvfrom(socket->value, static_cast<char*>(buffer), static_cast<int>(length),
        flags & 2 ? MSG_PEEK : 0, reinterpret_cast<sockaddr*>(&native), &size);
    if (result < 0) return Fail(NativeError());
    if (address && socket->family == GuestUnix) {
        if (socket->type == 1) *addressLength = 0; else UnnamedAddress(address, addressLength);
        return result;
    }
    if (address) GuestAddress(native, address, addressLength);
    return result;
}
struct GuestIovec {
    void* base;
    std::uint64_t length;
};
struct GuestMsghdr {
    void* name;
    std::uint32_t nameLength;
    GuestIovec* iov;
    int iovLength;
    void* control;
    std::uint32_t controlLength;
    int flags;
};
static_assert(sizeof(GuestMsghdr) == 48 && offsetof(GuestMsghdr, iov) == 16 && offsetof(GuestMsghdr, control) == 32 && offsetof(GuestMsghdr, flags) == 44);
static std::int64_t MessageLength(const GuestMsghdr* message) {
    if (!message) return Fail(14);
    if (message->iovLength < 0 || message->iovLength > 1024) return Fail(40);
    if (message->iovLength && !message->iov) return Fail(14);
    std::uint64_t total = 0;
    for (int i = 0; i < message->iovLength; ++i) {
        const auto& entry = message->iov[i];
        if (!entry.base && entry.length) return Fail(14);
        if (entry.length > INT_MAX - total) return Fail(22);
        total += entry.length;
    }
    return static_cast<std::int64_t>(total);
}
std::int64_t APS5_VABI sendmsg_nid_postfix(int descriptor, const GuestMsghdr* message, int flags) {
    const auto total = MessageLength(message);
    if (total < 0) return -1;
    if (message->control && message->controlLength) throw std::runtime_error("sendmsg: control data is not supported");
    std::vector<char> buffer;
    try {
        buffer.resize(static_cast<std::size_t>(total));
    } catch (const std::bad_alloc&) {
        return Fail(55);
    }
    std::size_t offset = 0;
    for (int i = 0; i < message->iovLength; ++i) {
        if (message->iov[i].length) std::memcpy(buffer.data() + offset, message->iov[i].base, message->iov[i].length);
        offset += message->iov[i].length;
    }
    return sendto_nid_postfix(descriptor, buffer.data(), buffer.size(), flags, message->name, message->name ? message->nameLength : 0u);
}
std::int64_t APS5_VABI recvmsg_nid_postfix(int descriptor, GuestMsghdr* message, int flags) {
    const auto total = MessageLength(message);
    if (total < 0) return -1;
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if ((flags & ~2) != 0) return Fail(45);
    const bool datagram = socket->type == 2;
    std::vector<char> buffer;
    try {
        buffer.resize(static_cast<std::size_t>(total) + (datagram ? 1u : 0u));
    } catch (const std::bad_alloc&) {
        return Fail(55);
    }
    sockaddr_storage native{};
    socklen_t size = sizeof(native);
    std::int64_t received = ::recvfrom(socket->value, buffer.data(), static_cast<int>(buffer.size()),
        flags & 2 ? MSG_PEEK : 0, reinterpret_cast<sockaddr*>(&native), &size);
    if (received < 0) {
#ifdef _WIN32
        if (!datagram || WSAGetLastError() != WSAEMSGSIZE) return Fail(NativeError());
        received = static_cast<std::int64_t>(buffer.size());
#else
        return Fail(NativeError());
#endif
    }
    message->flags = 0;
    if (received > total) {
        received = total;
        message->flags = 0x10;
    }
    std::size_t offset = 0;
    for (int i = 0; i < message->iovLength && offset < static_cast<std::size_t>(received); ++i) {
        const auto count = std::min<std::size_t>(message->iov[i].length, static_cast<std::size_t>(received) - offset);
        std::memcpy(message->iov[i].base, buffer.data() + offset, count);
        offset += count;
    }
    if (message->name && socket->family == GuestUnix) {
        if (socket->type == 1) message->nameLength = 0; else UnnamedAddress(message->name, &message->nameLength);
    } else if (message->name) {
        if (size > 0 && (native.ss_family == AF_INET || native.ss_family == AF_INET6)) {
            GuestAddress(native, message->name, &message->nameLength);
        } else {
            message->nameLength = 0;
        }
    }
    message->controlLength = 0;
    return received;
}
int APS5_VABI poll_nid_postfix(GuestPollDescriptor* descriptors, std::uint32_t count, int timeout) {
    if (timeout < -1) return Fail(22);
    if (count && !descriptors) return Fail(14);
    std::vector<std::shared_ptr<Socket>> held;
    std::vector<NativePollDescriptor> native;
    std::vector<std::uint32_t> owners;
    int ready = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        auto& entry = descriptors[i];
        entry.revents = 0;
        if (entry.descriptor < 0) continue;
        if ((entry.events & ~GuestPollAccepted) != 0)
            throw std::runtime_error("poll: unsupported event flags " + std::to_string(entry.events));
        if (entry.descriptor < GuestSockets::FirstDescriptor)
            throw std::runtime_error("poll: descriptor " + std::to_string(entry.descriptor) + " is not a socket");
        auto socket = Find(entry.descriptor);
        if (!socket) {
            entry.revents = GuestPollInvalid;
            ++ready;
            continue;
        }
#ifdef _WIN32
        if (entry.events & GuestPollPriority) throw std::runtime_error("poll: POLLPRI is not supported by WSAPoll");
#endif
        NativePollDescriptor request{};
        request.fd = socket->value;
        for (const auto& flag : RequestFlags)
            if (entry.events & flag.guest) request.events = static_cast<short>(request.events | flag.native);
        native.push_back(request);
        owners.push_back(i);
        held.push_back(std::move(socket));
    }
    if (native.empty()) {
        if (ready || timeout == 0) return ready;
        if (timeout < 0) throw std::runtime_error("poll: an infinite wait without sockets never returns");
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
        return 0;
    }
    const int wait = ready ? 0 : timeout;
#ifdef _WIN32
    const int result = WSAPoll(native.data(), static_cast<ULONG>(native.size()), wait);
#else
    const int result = ::poll(native.data(), static_cast<nfds_t>(native.size()), wait);
#endif
    if (result < 0) return Fail(NativeError());
    for (std::size_t i = 0; i < native.size(); ++i) {
        auto& entry = descriptors[owners[i]];
        for (const auto& flag : RequestFlags)
            if ((entry.events & flag.guest) && (native[i].revents & flag.native))
                entry.revents = static_cast<short>(entry.revents | flag.guest);
        for (const auto& flag : StatusFlags)
            if (native[i].revents & flag.native) entry.revents = static_cast<short>(entry.revents | flag.guest);
        if (entry.revents) ++ready;
    }
    return ready;
}
}
