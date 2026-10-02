#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>
#endif
#include <algorithm>
#include <atomic>
#include <climits>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstddef>
#include <cstring>
#include <memory>
#include <map>
#include <mutex>
#include <new>
#include <vector>
#include <set>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// Guest socket descriptors retain PS5 semantics while their transport is backed by host sockets.

namespace {
// FreeBSD errno numbers as reported through sceNetErrnoLoc (SCE_NET_ERROR_* is 0x80410100 + errno).
constexpr int NET_ENOENT = 2;
constexpr int NET_EBADF = 9;
constexpr int NET_EINVAL = 22;
constexpr int NET_EAGAIN = 35;
constexpr int NET_ENOTSOCK = 38;
constexpr int NET_EOPNOTSUPP = 45;
constexpr int NET_EPROTONOSUPPORT = 43;
constexpr int NET_EAFNOSUPPORT = 47;
constexpr int NET_EADDRINUSE = 48;
constexpr int NET_ENETUNREACH = 51;
constexpr int NET_ECONNABORTED = 53;
constexpr int NET_EMSGSIZE = 40;
constexpr int NET_ETIMEDOUT = 60;
constexpr int NET_ECONNREFUSED = 61;
constexpr int NET_ERROR_RESOLVER_ENODNS = static_cast<int>(0x804101E1u);

constexpr int NET_AF_INET = 2;
constexpr int NET_AF_INET6 = 28;
constexpr int NET_SOCK_STREAM = 1;
constexpr int NET_SOCK_DGRAM = 2;
constexpr int NET_SOCK_RAW = 3;
constexpr int NET_SOL_SOCKET = 0xFFFF;
constexpr int NET_SO_SNDTIMEO = 0x1005;
constexpr int NET_SO_RCVTIMEO = 0x1006;
constexpr int NET_SO_NBIO = 0x1200;

#ifdef _WIN32
using NativeSocket = SOCKET;
using NativeLength = int;
constexpr NativeSocket INVALID_NATIVE_SOCKET = INVALID_SOCKET;
bool initialize_sockets() {
    static const int result = [] {
        WSADATA data{};
        return WSAStartup(MAKEWORD(2, 2), &data);
    }();
    return result == 0;
}
void close_socket(NativeSocket socket) { closesocket(socket); }
int native_error() {
    switch (WSAGetLastError()) {
        case WSAEWOULDBLOCK: return NET_EAGAIN;
        case WSAEADDRINUSE: return NET_EADDRINUSE;
        case WSAEAFNOSUPPORT: return NET_EAFNOSUPPORT;
        case WSAENETUNREACH: return NET_ENETUNREACH;
        case WSAEHOSTUNREACH: return NET_ENETUNREACH;
        case WSAECONNABORTED: return NET_ECONNABORTED;
        case WSAECONNRESET: return 54;
        case WSAETIMEDOUT: return NET_ETIMEDOUT;
        case WSAECONNREFUSED: return NET_ECONNREFUSED;
        case WSAEMSGSIZE: return NET_EMSGSIZE;
        case WSAEINTR: return 4;
        case WSAEINVAL: return NET_EINVAL;
        case WSAENOTSOCK: return NET_ENOTSOCK;
        default: return 5;
    }
}
#else
using NativeSocket = int;
using NativeLength = socklen_t;
constexpr NativeSocket INVALID_NATIVE_SOCKET = -1;
bool initialize_sockets() { return true; }
void close_socket(NativeSocket socket) { ::close(socket); }
int native_error() {
    switch (errno) {
        case EAGAIN: return NET_EAGAIN;
#if EWOULDBLOCK != EAGAIN
        case EWOULDBLOCK: return NET_EAGAIN;
#endif
        case EADDRINUSE: return NET_EADDRINUSE;
        case EAFNOSUPPORT: return NET_EAFNOSUPPORT;
        case ENETUNREACH: return NET_ENETUNREACH;
        case EHOSTUNREACH: return NET_ENETUNREACH;
        case ECONNABORTED: return NET_ECONNABORTED;
        case ECONNRESET: return 54;
        case ETIMEDOUT: return NET_ETIMEDOUT;
        case ECONNREFUSED: return NET_ECONNREFUSED;
        case EMSGSIZE: return NET_EMSGSIZE;
        case EINTR: return 4;
        case EINVAL: return NET_EINVAL;
        case ENOTSOCK: return NET_ENOTSOCK;
        default: return 5;
    }
}
#endif

struct NativeSocketHandle {
    NativeSocket value;
    explicit NativeSocketHandle(NativeSocket socket) : value(socket) {}
    ~NativeSocketHandle() {
        if (value != INVALID_NATIVE_SOCKET) close_socket(value);
    }
};

bool guest_to_native_address(const void* address, std::uint32_t length,
                             sockaddr_storage& native, NativeLength& native_length) {
    if (!address || length < 2) return false;
    const auto* bytes = static_cast<const std::uint8_t*>(address);
    if (bytes[1] == NET_AF_INET && bytes[0] == 16 && length >= 16) {
        auto& v4 = reinterpret_cast<sockaddr_in&>(native);
        v4.sin_family = AF_INET;
        std::memcpy(&v4.sin_port, bytes + 2, 2);
        std::memcpy(&v4.sin_addr, bytes + 4, 4);
        native_length = sizeof(v4);
        return true;
    }
    if (bytes[1] == NET_AF_INET6 && bytes[0] == 28 && length >= 28) {
        auto& v6 = reinterpret_cast<sockaddr_in6&>(native);
        v6.sin6_family = AF_INET6;
        std::memcpy(&v6.sin6_port, bytes + 2, 2);
        std::memcpy(&v6.sin6_flowinfo, bytes + 4, 4);
        std::memcpy(&v6.sin6_addr, bytes + 8, 16);
        std::memcpy(&v6.sin6_scope_id, bytes + 24, 4);
        native_length = sizeof(v6);
        return true;
    }
    return false;
}

bool native_to_guest_address(const sockaddr_storage& native, void* address, std::uint32_t* length) {
    if (!address || !length) return false;
    std::uint8_t bytes[28]{};
    if (native.ss_family == AF_INET) {
        const auto& v4 = reinterpret_cast<const sockaddr_in&>(native);
        bytes[0] = 16;
        bytes[1] = NET_AF_INET;
        std::memcpy(bytes + 2, &v4.sin_port, 2);
        std::memcpy(bytes + 4, &v4.sin_addr, 4);
    } else if (native.ss_family == AF_INET6) {
        const auto& v6 = reinterpret_cast<const sockaddr_in6&>(native);
        bytes[0] = 28;
        bytes[1] = NET_AF_INET6;
        std::memcpy(bytes + 2, &v6.sin6_port, 2);
        std::memcpy(bytes + 4, &v6.sin6_flowinfo, 4);
        std::memcpy(bytes + 8, &v6.sin6_addr, 16);
        std::memcpy(bytes + 24, &v6.sin6_scope_id, 4);
    } else {
        return false;
    }
    const auto required = static_cast<std::uint32_t>(bytes[0]);
    std::memcpy(address, bytes, std::min(*length, required));
    *length = required;
    return true;
}

struct Sock {
    int family = 0;
    int type = 0;
    std::shared_ptr<NativeSocketHandle> native;
    bool nonblock = false;
    bool bound = false;
    bool listening = false;
    bool aborted = false;
    int rcv_timeout_us = 0;
    int snd_timeout_us = 0;
};

std::mutex g_mutex;
std::condition_variable g_cv;
std::map<int, Sock> g_socks;
std::set<int> g_epolls;
std::map<int, std::map<int, NetEpollEvent>> g_epoll_socks;
std::set<int> g_epoll_aborted;
std::set<int> g_pools;
std::set<int> g_resolvers;
int g_next_sock = 32;
int g_next_epoll = 0x4000;
int g_next_pool = 1;
int g_next_resolver = 0x100;
bool g_net_inited = false;

int* errno_slot() {
    static thread_local int e = 0;
    return &e;
}

int fail(int err) {
    *errno_slot() = err;
    return -1;
}

void log_soft(const char* func, const char* what) {
    static std::mutex mtx;
    static std::map<std::string, int> hits;
    std::lock_guard<std::mutex> lk(mtx);
    if (++hits[func] > 3) {
        return;
    }
    std::fprintf(stderr, "[SOFT-NET] %s -> %s\n", func, what);
    std::fflush(stderr);
}

std::uint16_t swap16(std::uint16_t v) { return static_cast<std::uint16_t>((v << 8) | (v >> 8)); }
std::uint32_t swap32(std::uint32_t v) {
    return (v << 24) | ((v & 0xFF00u) << 8) | ((v >> 8) & 0xFF00u) | (v >> 24);
}

std::uint64_t swap64(std::uint64_t v) {
    return (v << 56) | ((v & 0xFF00u) << 40) | ((v & 0xFF0000u) << 24) | ((v & 0xFF000000u) << 8) |
        ((v >> 8) & 0xFF000000u) | ((v >> 24) & 0xFF0000u) | ((v >> 40) & 0xFF00u) | (v >> 56);
}

}  // namespace

extern "C" {

int* APS5_VABI sceNetErrnoLoc(void) {
    return errno_slot();
}

int APS5_VABI sceNetInit_nid_postfix(void) {
    std::lock_guard<std::mutex> lk(g_mutex);
    g_net_inited = true;
    return 0;
}

int APS5_VABI sceNetTerm(void) {
    std::lock_guard<std::mutex> lk(g_mutex);
    g_net_inited = false;
    return 0;
}

int APS5_VABI sceNetPoolCreate(const char* name, int size, int flags) {
    (void)name;
    (void)flags;
    if (size <= 0) {
        return fail(NET_EINVAL);
    }
    std::lock_guard<std::mutex> lk(g_mutex);
    const int id = g_next_pool++;
    g_pools.insert(id);
    return id;
}

int APS5_VABI sceNetPoolDestroy(int memid) {
    std::lock_guard<std::mutex> lk(g_mutex);
    return g_pools.erase(memid) != 0 ? 0 : fail(NET_EBADF);
}

int APS5_VABI sceNetSocket(const char* name, int family, int type, int protocol) {
    (void)name;
    if (family != NET_AF_INET && family != NET_AF_INET6) {
        return fail(NET_EAFNOSUPPORT);
    }
    if (type != NET_SOCK_STREAM && type != NET_SOCK_DGRAM && type != NET_SOCK_RAW) {
        return fail(NET_EPROTONOSUPPORT);
    }
    if (!initialize_sockets()) return fail(5);
    const NativeSocket native = ::socket(family == NET_AF_INET ? AF_INET : AF_INET6, type, protocol);
    if (native == INVALID_NATIVE_SOCKET) return fail(native_error());
    std::shared_ptr<NativeSocketHandle> handle;
    try {
        handle = std::make_shared<NativeSocketHandle>(native);
    } catch (const std::bad_alloc&) {
        close_socket(native);
        return fail(12);
    }
    std::lock_guard<std::mutex> lk(g_mutex);
    const int fd = g_next_sock++;
    Sock s;
    s.family = family;
    s.type = type;
    s.native = std::move(handle);
    g_socks[fd] = s;
    return fd;
}

int APS5_VABI sceNetSocketClose(int s) {
    std::shared_ptr<NativeSocketHandle> native;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        const auto it = g_socks.find(s);
        if (it == g_socks.end()) return fail(NET_EBADF);
        native = it->second.native;
        g_socks.erase(it);
    }
    if (native) {
#ifdef _WIN32
        ::shutdown(native->value, SD_BOTH);
#else
        ::shutdown(native->value, SHUT_RDWR);
#endif
    }
    g_cv.notify_all();
    return 0;
}

int APS5_VABI sceNetSocketAbort(int s, int flags) {
    (void)flags;
    std::shared_ptr<NativeSocketHandle> native;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        auto it = g_socks.find(s);
        if (it == g_socks.end()) return fail(NET_EBADF);
        it->second.aborted = true;
        native = it->second.native;
    }
#ifdef _WIN32
    if (native) ::shutdown(native->value, SD_BOTH);
#else
    if (native) ::shutdown(native->value, SHUT_RDWR);
#endif
    g_cv.notify_all();
    return 0;
}

int APS5_VABI sceNetBind_nid_postfix(int s, const void* addr, uint32_t addrlen) {
    Sock socket;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        auto it = g_socks.find(s);
        if (it == g_socks.end()) return fail(NET_EBADF);
        socket = it->second;
        if (it->second.bound) return fail(NET_EINVAL);
    }
    sockaddr_storage native{};
    NativeLength native_length = 0;
    if (!guest_to_native_address(addr, addrlen, native, native_length)) return fail(NET_EINVAL);
    if (native.ss_family != (socket.family == NET_AF_INET ? AF_INET : AF_INET6)) return fail(NET_EAFNOSUPPORT);
    if (::bind(socket.native->value, reinterpret_cast<const sockaddr*>(&native), native_length) != 0)
        return fail(native_error());
    std::lock_guard<std::mutex> lk(g_mutex);
    auto it = g_socks.find(s);
    if (it != g_socks.end()) it->second.bound = true;
    return 0;
}

int APS5_VABI sceNetListen(int s, int backlog) {
    Sock socket;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        auto it = g_socks.find(s);
        if (it == g_socks.end()) return fail(NET_EBADF);
        socket = it->second;
    }
    if (socket.type != NET_SOCK_STREAM) return fail(NET_EOPNOTSUPP);
    if (::listen(socket.native->value, backlog) != 0) return fail(native_error());
    std::lock_guard<std::mutex> lk(g_mutex);
    auto it = g_socks.find(s);
    if (it != g_socks.end()) {
        it->second.bound = true;
        it->second.listening = true;
    }
    return 0;
}

int APS5_VABI sceNetAccept(int s, void* addr, uint32_t* addrlen) {
    Sock listener;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        auto it = g_socks.find(s);
        if (it == g_socks.end()) return fail(NET_EBADF);
        listener = it->second;
    }
    if (!listener.listening) return fail(NET_EINVAL);
    if (addr && !addrlen) return fail(NET_EINVAL);
    sockaddr_storage peer{};
    NativeLength peer_length = sizeof(peer);
    const NativeSocket accepted = ::accept(listener.native->value,
        addr ? reinterpret_cast<sockaddr*>(&peer) : nullptr, addr ? &peer_length : nullptr);
    if (accepted == INVALID_NATIVE_SOCKET) return fail(native_error());
    std::shared_ptr<NativeSocketHandle> handle;
    try {
        handle = std::make_shared<NativeSocketHandle>(accepted);
    } catch (const std::bad_alloc&) {
        close_socket(accepted);
        return fail(12);
    }
    Sock connection;
    connection.family = listener.family;
    connection.type = NET_SOCK_STREAM;
    connection.native = std::move(handle);
    int descriptor;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        descriptor = g_next_sock++;
        g_socks.emplace(descriptor, connection);
    }
    if (addr && !native_to_guest_address(peer, addr, addrlen)) {
        sceNetSocketClose(descriptor);
        return fail(NET_EAFNOSUPPORT);
    }
    return descriptor;
}

int APS5_VABI sceNetConnect(int s, const void* addr, uint32_t addrlen) {
    Sock socket;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        auto it = g_socks.find(s);
        if (it == g_socks.end()) return fail(NET_EBADF);
        socket = it->second;
    }
    sockaddr_storage native{};
    NativeLength native_length = 0;
    if (!guest_to_native_address(addr, addrlen, native, native_length)) return fail(NET_EINVAL);
    if (native.ss_family != (socket.family == NET_AF_INET ? AF_INET : AF_INET6)) return fail(NET_EAFNOSUPPORT);
    return ::connect(socket.native->value, reinterpret_cast<const sockaddr*>(&native), native_length) == 0
        ? 0 : fail(native_error());
}

int64_t APS5_VABI sceNetRecv(int s, void* buf, size_t len, int flags) {
    if (flags != 0 && flags != 2) return fail(NET_EOPNOTSUPP);
    if (!buf && len) return fail(NET_EINVAL);
    if (len > INT_MAX) return fail(NET_EMSGSIZE);
    Sock socket;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        auto it = g_socks.find(s);
        if (it == g_socks.end()) return fail(NET_EBADF);
        socket = it->second;
    }
    const int native_flags = flags == 2 ? MSG_PEEK : 0;
    const int result = ::recv(socket.native->value, static_cast<char*>(buf), static_cast<int>(len), native_flags);
    return result >= 0 ? result : fail(native_error());
}

int64_t APS5_VABI sceNetRecvfrom(int s, void* buf, size_t len, int flags, void* from, uint32_t* fromlen) {
    if (flags != 0 && flags != 2) return fail(NET_EOPNOTSUPP);
    if (!buf && len) return fail(NET_EINVAL);
    if (len > INT_MAX) return fail(NET_EMSGSIZE);
    if (from && !fromlen) return fail(NET_EINVAL);
    Sock socket;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        auto it = g_socks.find(s);
        if (it == g_socks.end()) return fail(NET_EBADF);
        socket = it->second;
    }
    sockaddr_storage peer{};
    NativeLength peer_length = sizeof(peer);
    const int native_flags = flags == 2 ? MSG_PEEK : 0;
    const int result = ::recvfrom(socket.native->value, static_cast<char*>(buf), static_cast<int>(len),
        native_flags, from ? reinterpret_cast<sockaddr*>(&peer) : nullptr, from ? &peer_length : nullptr);
    if (result < 0) return fail(native_error());
    if (from && !native_to_guest_address(peer, from, fromlen)) return fail(NET_EAFNOSUPPORT);
    return result;
}

int64_t APS5_VABI sceNetSend(int s, const void* buf, size_t len, int flags) {
    if (flags != 0) return fail(NET_EOPNOTSUPP);
    if (!buf && len) return fail(NET_EINVAL);
    if (len > INT_MAX) return fail(NET_EMSGSIZE);
    Sock socket;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        auto it = g_socks.find(s);
        if (it == g_socks.end()) return fail(NET_EBADF);
        socket = it->second;
    }
    const int result = ::send(socket.native->value, static_cast<const char*>(buf), static_cast<int>(len), 0);
    return result >= 0 ? result : fail(native_error());
}

int64_t APS5_VABI sceNetSendto(int s, const void* buf, size_t len, int flags, const void* to, uint32_t tolen) {
    if (flags != 0) return fail(NET_EOPNOTSUPP);
    if (!buf && len) return fail(NET_EINVAL);
    if (len > INT_MAX) return fail(NET_EMSGSIZE);
    Sock socket;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        auto it = g_socks.find(s);
        if (it == g_socks.end()) return fail(NET_EBADF);
        socket = it->second;
    }
    int result;
    if (!to) {
        if (tolen != 0) return fail(NET_EINVAL);
        result = ::send(socket.native->value, static_cast<const char*>(buf), static_cast<int>(len), 0);
    } else {
        sockaddr_storage destination{};
        NativeLength destination_length = 0;
        if (!guest_to_native_address(to, tolen, destination, destination_length)) return fail(NET_EINVAL);
        if (destination.ss_family != (socket.family == NET_AF_INET ? AF_INET : AF_INET6)) return fail(NET_EAFNOSUPPORT);
        result = ::sendto(socket.native->value, static_cast<const char*>(buf), static_cast<int>(len), 0,
            reinterpret_cast<const sockaddr*>(&destination), destination_length);
    }
    return result >= 0 ? result : fail(native_error());
}

int APS5_VABI sceNetShutdown(int s, int how) {
    if (how < 0 || how > 2) return fail(NET_EINVAL);
    Sock socket;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        auto it = g_socks.find(s);
        if (it == g_socks.end()) return fail(NET_EBADF);
        socket = it->second;
    }
    return ::shutdown(socket.native->value, how) == 0 ? 0 : fail(native_error());
}

int APS5_VABI sceNetSetsockopt(int s, int level, int optname, const void* optval, uint32_t optlen) {
    Sock socket;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        auto it = g_socks.find(s);
        if (it == g_socks.end()) return fail(NET_EBADF);
        socket = it->second;
    }
    if (level != NET_SOL_SOCKET) return fail(NET_EOPNOTSUPP);
    if (!optval || optlen < sizeof(int)) return fail(NET_EINVAL);
    int value = 0;
    std::memcpy(&value, optval, sizeof(value));
    if (optname == NET_SO_NBIO) {
#ifdef _WIN32
        u_long enabled = value != 0;
        if (ioctlsocket(socket.native->value, FIONBIO, &enabled) != 0) return fail(native_error());
#else
        const int old_flags = fcntl(socket.native->value, F_GETFL, 0);
        if (old_flags < 0 || fcntl(socket.native->value, F_SETFL,
            value ? old_flags | O_NONBLOCK : old_flags & ~O_NONBLOCK) != 0) return fail(native_error());
#endif
    } else if (optname == NET_SO_RCVTIMEO || optname == NET_SO_SNDTIMEO) {
        if (value < 0) return fail(NET_EINVAL);
#ifdef _WIN32
        const DWORD timeout_ms = value == 0 ? 0 : static_cast<DWORD>((value + 999) / 1000);
        const int native_option = optname == NET_SO_RCVTIMEO ? SO_RCVTIMEO : SO_SNDTIMEO;
        if (::setsockopt(socket.native->value, SOL_SOCKET, native_option,
                         reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms)) != 0)
            return fail(native_error());
#else
        timeval timeout{};
        timeout.tv_sec = value / 1000000;
        timeout.tv_usec = value % 1000000;
        const int native_option = optname == NET_SO_RCVTIMEO ? SO_RCVTIMEO : SO_SNDTIMEO;
        if (::setsockopt(socket.native->value, SOL_SOCKET, native_option, &timeout, sizeof(timeout)) != 0)
            return fail(native_error());
#endif
    } else {
        return fail(NET_EOPNOTSUPP);
    }
    std::lock_guard<std::mutex> lk(g_mutex);
    auto it = g_socks.find(s);
    if (it != g_socks.end()) {
        if (optname == NET_SO_NBIO) it->second.nonblock = value != 0;
        if (optname == NET_SO_RCVTIMEO) it->second.rcv_timeout_us = value;
        if (optname == NET_SO_SNDTIMEO) it->second.snd_timeout_us = value;
    }
    return 0;
}

int APS5_VABI sceNetGetsockopt(int s, int level, int optname, void* optval, uint32_t* optlen) {
    Sock socket;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        auto it = g_socks.find(s);
        if (it == g_socks.end()) return fail(NET_EBADF);
        socket = it->second;
    }
    if (optval == nullptr || optlen == nullptr || *optlen < sizeof(int)) {
        return fail(NET_EINVAL);
    }
    if (level != NET_SOL_SOCKET) return fail(NET_EOPNOTSUPP);
    int value = 0;
    switch (optname) {
        case NET_SO_NBIO: value = socket.nonblock ? 1 : 0; break;
        case NET_SO_RCVTIMEO: value = socket.rcv_timeout_us; break;
        case NET_SO_SNDTIMEO: value = socket.snd_timeout_us; break;
        default: return fail(NET_EOPNOTSUPP);
    }
    std::memcpy(optval, &value, sizeof(value));
    *optlen = sizeof(value);
    return 0;
}

int APS5_VABI sceNetGetsockname(int s, void* addr, uint32_t* addrlen) {
    Sock socket;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        auto it = g_socks.find(s);
        if (it == g_socks.end()) return fail(NET_EBADF);
        socket = it->second;
    }
    if (!addr || !addrlen) return fail(NET_EINVAL);
    sockaddr_storage native{};
    NativeLength native_length = sizeof(native);
    if (::getsockname(socket.native->value, reinterpret_cast<sockaddr*>(&native), &native_length) != 0)
        return fail(native_error());
    return native_to_guest_address(native, addr, addrlen) ? 0 : fail(NET_EAFNOSUPPORT);
}

int APS5_VABI sceNetGetpeername(int s, void* addr, uint32_t* addrlen) {
    Sock socket;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        auto it = g_socks.find(s);
        if (it == g_socks.end()) return fail(NET_EBADF);
        socket = it->second;
    }
    if (!addr || !addrlen) return fail(NET_EINVAL);
    sockaddr_storage native{};
    NativeLength native_length = sizeof(native);
    if (::getpeername(socket.native->value, reinterpret_cast<sockaddr*>(&native), &native_length) != 0)
        return fail(native_error());
    return native_to_guest_address(native, addr, addrlen) ? 0 : fail(NET_EAFNOSUPPORT);
}

int APS5_VABI sceNetGetSockInfo(int s, void* info, int n, int flags) {
    (void)info;
    (void)n;
    (void)flags;
    std::lock_guard<std::mutex> lk(g_mutex);
    return g_socks.count(s) != 0 ? 0 : fail(NET_EBADF);
}

int APS5_VABI sceNetEpollCreate(const char* name, int flags) {
    (void)name;
    (void)flags;
    std::lock_guard<std::mutex> lk(g_mutex);
    const int id = g_next_epoll++;
    g_epolls.insert(id);
    g_epoll_socks.emplace(id, std::map<int, NetEpollEvent>{});
    return id;
}

int APS5_VABI sceNetEpollDestroy(int eid) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_epolls.erase(eid) == 0) {
        return fail(NET_EBADF);
    }
    g_epoll_socks.erase(eid);
    g_epoll_aborted.erase(eid);
    g_cv.notify_all();
    return 0;
}

int APS5_VABI sceNetEpollAbort(int eid, int flags) {
    (void)flags;
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_epolls.count(eid) == 0) {
        return fail(NET_EBADF);
    }
    g_epoll_aborted.insert(eid);
    g_cv.notify_all();
    return 0;
}

int APS5_VABI sceNetEpollControl(int eid, int op, int id, const NetEpollEvent* event) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_epolls.count(eid) == 0) {
        return fail(NET_EBADF);
    }
    if (op < 1 || op > 3) return fail(NET_EINVAL);
    if (op != 2 && !event) return fail(NET_EINVAL);
    if (op != 2 && g_socks.count(id) == 0) return fail(NET_EBADF);
    auto& registrations = g_epoll_socks[eid];
    if (op == 1) {
        if (!registrations.emplace(id, *event).second) return fail(NET_EADDRINUSE);
    } else if (op == 2) {
        if (registrations.erase(id) == 0) return fail(NET_ENOENT);
    } else {
        auto registration = registrations.find(id);
        if (registration == registrations.end()) return fail(NET_ENOENT);
        registration->second = *event;
    }
    g_cv.notify_all();
    return 0;
}

int APS5_VABI sceNetEpollWait(int eid, NetEpollEvent* events, int maxevents, int timeout) {
    if (!events || maxevents <= 0) return fail(NET_EINVAL);
    const auto deadline = timeout > 0
        ? std::chrono::steady_clock::now() + std::chrono::microseconds(timeout)
        : std::chrono::steady_clock::time_point::max();
    for (;;) {
        std::vector<NetEpollEvent> registered_events;
        std::vector<std::shared_ptr<NativeSocketHandle>> native_sockets;
        {
            std::lock_guard<std::mutex> lk(g_mutex);
            if (g_epolls.count(eid) == 0) return fail(NET_EBADF);
            if (g_epoll_aborted.erase(eid) != 0) return fail(NET_ECONNABORTED);
            const auto registration = g_epoll_socks.find(eid);
            if (registration != g_epoll_socks.end()) {
                for (const auto& entry : registration->second) {
                    const auto socket = g_socks.find(entry.first);
                    if (socket == g_socks.end()) continue;
                    registered_events.push_back(entry.second);
                    native_sockets.push_back(socket->second.native);
                }
            }
        }
        if (registered_events.empty()) {
            if (timeout == 0) return 0;
            std::unique_lock<std::mutex> lk(g_mutex);
            const auto now = std::chrono::steady_clock::now();
            if (timeout > 0 && now >= deadline) return 0;
            const auto slice = std::chrono::milliseconds(100);
            const auto wake = timeout > 0 ? std::min(deadline, now + slice) : now + slice;
            g_cv.wait_until(lk, wake);
            continue;
        }

#ifdef _WIN32
        std::vector<WSAPOLLFD> descriptors(registered_events.size());
        for (std::size_t index = 0; index < descriptors.size(); ++index) {
            descriptors[index].fd = native_sockets[index]->value;
            if (registered_events[index].events & 1) descriptors[index].events |= POLLRDNORM;
            if (registered_events[index].events & 4) descriptors[index].events |= POLLWRNORM;
        }
#else
        std::vector<pollfd> descriptors(registered_events.size());
        for (std::size_t index = 0; index < descriptors.size(); ++index) {
            descriptors[index].fd = native_sockets[index]->value;
            if (registered_events[index].events & 1) descriptors[index].events |= POLLIN;
            if (registered_events[index].events & 4) descriptors[index].events |= POLLOUT;
        }
#endif
        const auto now = std::chrono::steady_clock::now();
        if (timeout > 0 && now >= deadline) return 0;
        int wait_ms = 100;
        if (timeout == 0) wait_ms = 0;
        else if (timeout > 0) {
            const auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(deadline - now).count();
            wait_ms = static_cast<int>(std::min<std::int64_t>(100, (remaining + 999) / 1000));
        }
#ifdef _WIN32
        const int ready_count = WSAPoll(descriptors.data(), static_cast<ULONG>(descriptors.size()), wait_ms);
#else
        const int ready_count = ::poll(descriptors.data(), descriptors.size(), wait_ms);
#endif
        if (ready_count < 0) return fail(native_error());
        {
            std::lock_guard<std::mutex> lk(g_mutex);
            if (g_epolls.count(eid) == 0) return fail(NET_EBADF);
            if (g_epoll_aborted.erase(eid) != 0) return fail(NET_ECONNABORTED);
        }
        if (ready_count > 0) {
            int count = 0;
            for (std::size_t index = 0; index < descriptors.size() && count < maxevents; ++index) {
                const auto native_events = descriptors[index].revents;
                if (!native_events) continue;
                std::uint32_t guest_events = 0;
#ifdef _WIN32
                if (native_events & (POLLRDNORM | POLLPRI)) guest_events |= 1;
                if (native_events & POLLWRNORM) guest_events |= 4;
#else
                if (native_events & (POLLIN | POLLPRI)) guest_events |= 1;
                if (native_events & POLLOUT) guest_events |= 4;
#endif
                if (native_events & POLLERR) guest_events |= 8;
                if (native_events & POLLHUP) guest_events |= 16;
                events[count] = registered_events[index];
                events[count].events = guest_events;
                ++count;
            }
            if (count > 0) return count;
        }
        if (timeout == 0) return 0;
    }
}

// Byte order and text conversion.
uint32_t APS5_VABI sceNetHtonl_nid_postfix(uint32_t host32) { return swap32(host32); }
uint16_t APS5_VABI sceNetHtons_nid_postfix(uint16_t host16) { return swap16(host16); }
uint64_t APS5_VABI sceNetHtonll(std::uint64_t host64) { return swap64(host64); }
uint64_t APS5_VABI sceNetNtohll(std::uint64_t net64) { return swap64(net64); }
uint32_t APS5_VABI sceNetNtohl_nid_postfix(uint32_t net32) { return swap32(net32); }
uint16_t APS5_VABI sceNetNtohs_nid_postfix(uint16_t net16) { return swap16(net16); }

int APS5_VABI sceNetInetPton(int af, const char* src, void* dst) {
    if (src == nullptr || dst == nullptr) {
        return fail(NET_EINVAL);
    }
    if (af != NET_AF_INET && af != NET_AF_INET6) {
        return fail(NET_EAFNOSUPPORT);
    }
#ifdef _WIN32
    return InetPtonA(af == NET_AF_INET ? AF_INET : AF_INET6, src, dst);
#else
    return ::inet_pton(af == NET_AF_INET ? AF_INET : AF_INET6, src, dst);
#endif
}

const char* APS5_VABI sceNetInetNtop(int af, const void* src, char* dst, uint32_t size) {
    if (src == nullptr || dst == nullptr || size == 0) {
        *errno_slot() = NET_EINVAL;
        return nullptr;
    }
    if (af != NET_AF_INET && af != NET_AF_INET6) {
        *errno_slot() = NET_EAFNOSUPPORT;
        return nullptr;
    }
#ifdef _WIN32
    return InetNtopA(af == NET_AF_INET ? AF_INET : AF_INET6, const_cast<void*>(src), dst, size);
#else
    return ::inet_ntop(af == NET_AF_INET ? AF_INET : AF_INET6, src, dst, size);
#endif
}

int APS5_VABI sceNetEtherNtostr(const NetEtherAddr* n, char* str, size_t len) {
    if (n == nullptr || str == nullptr || len < 18) {
        return fail(NET_EINVAL);
    }
    std::snprintf(str, len, "%02x:%02x:%02x:%02x:%02x:%02x", n->data[0], n->data[1], n->data[2], n->data[3], n->data[4], n->data[5]);
    return 0;
}

int APS5_VABI sceNetGetMacAddress(NetEtherAddr* addr, int flags) {
    (void)flags;
    if (addr == nullptr) {
        return fail(NET_EINVAL);
    }
    // Keep guest identity stable without exposing the host adapter address.
    const std::uint8_t mac[6] = {0x02, 0x50, 0x53, 0x35, 0x00, 0x01};
    std::memcpy(addr->data, mac, 6);
    return 0;
}

// Resolver results use the guest IPv4 address format while delegating lookup to host DNS.
int APS5_VABI sceNetResolverCreate(const char* name, int memid, int flags) {
    (void)name;
    (void)memid;
    (void)flags;
    std::lock_guard<std::mutex> lk(g_mutex);
    const int id = g_next_resolver++;
    g_resolvers.insert(id);
    return id;
}

int APS5_VABI sceNetResolverDestroy(int rid) {
    std::lock_guard<std::mutex> lk(g_mutex);
    return g_resolvers.erase(rid) != 0 ? 0 : fail(NET_EBADF);
}

int APS5_VABI sceNetResolverStartNtoa(int rid, const char* hostname, void* addr, int timeout, int retry, int flags) {
    (void)timeout;
    (void)retry;
    (void)flags;
    if (!hostname || !addr) return fail(NET_EINVAL);
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        if (g_resolvers.count(rid) == 0) return fail(NET_EBADF);
    }
    if (!initialize_sockets()) return fail(5);
    addrinfo hints{};
    hints.ai_family = AF_INET;
    addrinfo* results = nullptr;
    const int result = ::getaddrinfo(hostname, nullptr, &hints, &results);
    if (result != 0) {
        *errno_slot() = result == EAI_AGAIN ? NET_ETIMEDOUT : NET_ENOENT;
        log_soft(__func__, "host DNS lookup failed");
        return NET_ERROR_RESOLVER_ENODNS;
    }
    const auto* address = reinterpret_cast<const sockaddr_in*>(results->ai_addr);
    std::memcpy(addr, &address->sin_addr, sizeof(address->sin_addr));
    ::freeaddrinfo(results);
    return 0;
}

int APS5_VABI sceNetResolverStartAton(int rid, const void* addr, char* hostname, int len, int timeout, int retry, int flags) {
    (void)timeout;
    (void)retry;
    (void)flags;
    if (!addr || !hostname || len <= 0) return fail(NET_EINVAL);
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        if (g_resolvers.count(rid) == 0) return fail(NET_EBADF);
    }
    if (!initialize_sockets()) return fail(5);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    std::memcpy(&address.sin_addr, addr, sizeof(address.sin_addr));
    const int result = ::getnameinfo(reinterpret_cast<const sockaddr*>(&address), sizeof(address), hostname,
        static_cast<NativeLength>(len), nullptr, 0, NI_NAMEREQD);
    if (result != 0) {
        *errno_slot() = result == EAI_AGAIN ? NET_ETIMEDOUT : NET_ENOENT;
        log_soft(__func__, "host reverse DNS lookup failed");
        return NET_ERROR_RESOLVER_ENODNS;
    }
    return 0;
}
}
