#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cerrno>
#endif
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"
#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <cstdarg>

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
int NativeError() {
#ifdef _WIN32
    switch (WSAGetLastError()) {
        case WSAEWOULDBLOCK: return 35;
        case WSAEADDRINUSE: return 48;
        case WSAEADDRNOTAVAIL: return 49;
        case WSAEACCES: return 13;
        case WSAEMSGSIZE: return 40;
        case WSAENETUNREACH: return 51;
        case WSAECONNRESET: return 54;
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
        case EMSGSIZE: return 40;
        case ENETUNREACH: return 51;
        case ECONNREFUSED: return 61;
        default: return 5;
    }
#endif
}
struct Socket {
    NativeSocket value;
    std::mutex modeMutex;
    bool nonblocking = false;
    explicit Socket(NativeSocket value) : value(value) {}
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
    if (bytes[1] == 2 && length >= 16 && bytes[0] == 16) {
        auto& v4 = reinterpret_cast<sockaddr_in&>(native);
        v4.sin_family = AF_INET;
        std::memcpy(&v4.sin_port, bytes + 2, 2);
        std::memcpy(&v4.sin_addr, bytes + 4, 4);
        size = sizeof(v4);
        return true;
    }
    if (bytes[1] == 28 && length >= 28 && bytes[0] == 28) {
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
    if (type != 2 || (protocol != 0 && protocol != 17)) return Fail(43);
#ifdef _WIN32
    static const int startup = [] { WSADATA data{}; return WSAStartup(MAKEWORD(2, 2), &data); }();
    if (startup) return Fail(5);
#endif
    const auto native = ::socket(family == 2 ? AF_INET : AF_INET6, SOCK_DGRAM, protocol);
    if (native == Invalid) return Fail(NativeError());
    Socket guard(native);
    try {
        auto socket = std::make_shared<Socket>(native);
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
    if (how < 0 || how > 2) return Fail(22); // SHUT_RD/SHUT_WR/SHUT_RDWR share values with the host constants
    return ::shutdown(socket->value, how) ? Fail(NativeError()) : 0;
}
int APS5_VABI getsockname_nid_postfix(int descriptor, void* address, std::uint32_t* length) {
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if (!address || !length) return Fail(14);
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
    if (flags != 0) return Fail(45);
    if (length > INT_MAX) return Fail(40);
    if (!buffer && length) return Fail(14);
    sockaddr_storage native{};
    socklen_t size;
    if (!Address(address, addressLength, native, size)) return -1;
    const auto result = ::sendto(socket->value, static_cast<const char*>(buffer), static_cast<int>(length), 0,
        reinterpret_cast<sockaddr*>(&native), size);
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
    if (address) GuestAddress(native, address, addressLength);
    return result;
}
}
