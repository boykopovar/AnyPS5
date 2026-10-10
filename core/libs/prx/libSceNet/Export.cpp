#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
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
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Socket/include/SocketPoll.hpp"

// Guest socket descriptors retain PS5 semantics while their transport is backed by host sockets.

namespace {
// FreeBSD errno numbers as reported through sceNetErrnoLoc (SCE_NET_ERROR_* is 0x80410100 + errno).
constexpr int NET_ENOENT = 2;
constexpr int NET_EBADF = 9;
constexpr int NET_EACCES = 13;
constexpr int NET_EFAULT = 14;
constexpr int NET_EINVAL = 22;
constexpr int NET_ENOSPC = 28;
constexpr int NET_EAGAIN = 35;
constexpr int NET_EINPROGRESS = 36;
constexpr int NET_EALREADY = 37;
constexpr int NET_ENOTSOCK = 38;
constexpr int NET_EOPNOTSUPP = 45;
constexpr int NET_EPROTONOSUPPORT = 43;
constexpr int NET_EAFNOSUPPORT = 47;
constexpr int NET_EADDRINUSE = 48;
constexpr int NET_EADDRNOTAVAIL = 49;
constexpr int NET_ENETUNREACH = 51;
constexpr int NET_ECONNABORTED = 53;
constexpr int NET_ENOBUFS = 55;
constexpr int NET_EISCONN = 56;
constexpr int NET_ENOTCONN = 57;
constexpr int NET_EMSGSIZE = 40;
constexpr int NET_ETIMEDOUT = 60;
constexpr int NET_ECONNREFUSED = 61;
constexpr int NET_EHOSTUNREACH = 65;
constexpr int NET_ERROR_BASE = static_cast<int>(0x80410100u);
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
constexpr int NET_MSG_PEEK = 0x2;
constexpr int NET_MSG_TRUNC = 0x10;
constexpr int NET_UIO_MAXIOV = 1024;

#ifdef _WIN32
using NativeSocket = SOCKET;
using NativeLength = int;
constexpr NativeSocket INVALID_NATIVE_SOCKET = INVALID_SOCKET;
constexpr int NATIVE_SEND_FLAGS = 0;
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
        case WSAEINPROGRESS: return NET_EINPROGRESS;
        case WSAEALREADY: return NET_EALREADY;
        case WSAEADDRINUSE: return NET_EADDRINUSE;
        case WSAEADDRNOTAVAIL: return NET_EADDRNOTAVAIL;
        case WSAEAFNOSUPPORT: return NET_EAFNOSUPPORT;
        case WSAENETUNREACH: return NET_ENETUNREACH;
        case WSAEHOSTUNREACH: return NET_EHOSTUNREACH;
        case WSAECONNABORTED: return NET_ECONNABORTED;
        case WSAENOBUFS: return NET_ENOBUFS;
        case WSAEISCONN: return NET_EISCONN;
        case WSAENOTCONN: return NET_ENOTCONN;
        case WSAEACCES: return NET_EACCES;
        case WSAECONNRESET: return 54;
        case WSAETIMEDOUT: return NET_ETIMEDOUT;
        case WSAECONNREFUSED: return NET_ECONNREFUSED;
        case WSAEMSGSIZE: return NET_EMSGSIZE;
        case WSAEINTR: return 4;
        case WSAEINVAL: return NET_EINVAL;
        case WSAENOTSOCK: return NET_ENOTSOCK;
        case WSAESHUTDOWN: return 32;
        default: return 5;
    }
}
#else
using NativeSocket = int;
using NativeLength = socklen_t;
constexpr NativeSocket INVALID_NATIVE_SOCKET = -1;
constexpr int NATIVE_SEND_FLAGS = MSG_NOSIGNAL;
bool initialize_sockets() { return true; }
void close_socket(NativeSocket socket) { ::close(socket); }
int native_error() {
    switch (errno) {
        case EAGAIN: return NET_EAGAIN;
#if EWOULDBLOCK != EAGAIN
        case EWOULDBLOCK: return NET_EAGAIN;
#endif
        case EINPROGRESS: return NET_EINPROGRESS;
        case EALREADY: return NET_EALREADY;
        case EADDRINUSE: return NET_EADDRINUSE;
        case EADDRNOTAVAIL: return NET_EADDRNOTAVAIL;
        case EAFNOSUPPORT: return NET_EAFNOSUPPORT;
        case ENETUNREACH: return NET_ENETUNREACH;
        case EHOSTUNREACH: return NET_EHOSTUNREACH;
        case ECONNABORTED: return NET_ECONNABORTED;
        case ENOBUFS: return NET_ENOBUFS;
        case EISCONN: return NET_EISCONN;
        case ENOTCONN: return NET_ENOTCONN;
        case EACCES: return NET_EACCES;
        case ECONNRESET: return 54;
        case ETIMEDOUT: return NET_ETIMEDOUT;
        case ECONNREFUSED: return NET_ECONNREFUSED;
        case EMSGSIZE: return NET_EMSGSIZE;
        case EINTR: return 4;
        case EINVAL: return NET_EINVAL;
        case ENOTSOCK: return NET_ENOTSOCK;
        case EPIPE: return 32;
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

#ifdef _WIN32
struct HostMutex {
    SRWLOCK native = SRWLOCK_INIT;

    HostMutex() = default;
    HostMutex(const HostMutex&) = delete;
    HostMutex& operator=(const HostMutex&) = delete;

    void lock() { AcquireSRWLockExclusive(&native); }
    void unlock() { ReleaseSRWLockExclusive(&native); }
};
#else
using HostMutex = std::mutex;
#endif

class HostCondition {
public:
    HostCondition() = default;
    HostCondition(const HostCondition&) = delete;
    HostCondition& operator=(const HostCondition&) = delete;

    void NotifyAll() {
#ifdef _WIN32
        WakeAllConditionVariable(&native);
#else
        native.notify_all();
#endif
    }

    void WaitUntil(std::unique_lock<HostMutex>& lock, std::chrono::steady_clock::time_point deadline) {
#ifdef _WIN32
        const auto now = std::chrono::steady_clock::now();
        if (deadline <= now) return;
        const auto milliseconds = std::chrono::ceil<std::chrono::milliseconds>(deadline - now).count();
        if (!SleepConditionVariableSRW(&native, &lock.mutex()->native, static_cast<DWORD>(milliseconds), 0) &&
            GetLastError() != ERROR_TIMEOUT)
            throw std::runtime_error("sceNetEpollWait: native condition wait failed");
#else
        native.wait_until(lock, deadline);
#endif
    }

private:
#ifdef _WIN32
    CONDITION_VARIABLE native = CONDITION_VARIABLE_INIT;
#else
    std::condition_variable native;
#endif
};

HostMutex g_mutex;
HostCondition g_cv;
std::map<int, Sock> g_socks;
std::set<int> g_epolls;
std::map<int, std::map<int, NetEpollEvent>> g_epoll_socks;
std::set<int> g_epoll_aborted;
std::map<int, std::size_t> g_pools;

struct NetMemoryPoolStats {
    std::size_t pool_size;
    std::size_t max_inuse_size;
    std::size_t current_inuse_size;
};
std::map<int, int> g_resolvers;
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
    return NET_ERROR_BASE | err;
}

void set_resolver_error(int rid, int error) {
    std::lock_guard lk(g_mutex);
    const auto resolver = g_resolvers.find(rid);
    if (resolver != g_resolvers.end()) resolver->second = error;
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

struct NetIovec {
    void* base;
    std::uint64_t length;
};

struct NetMsghdr {
    void* name;
    std::uint32_t name_length;
    NetIovec* iov;
    int iov_length;
    void* control;
    std::uint32_t control_length;
    int flags;
};
static_assert(sizeof(NetMsghdr) == 48 && offsetof(NetMsghdr, iov) == 16 && offsetof(NetMsghdr, control) == 32 &&
    offsetof(NetMsghdr, flags) == 44);

std::int64_t message_length(const NetMsghdr* message) {
    if (!message) return fail(NET_EFAULT);
    if (message->iov_length < 0 || message->iov_length > NET_UIO_MAXIOV) return fail(NET_EMSGSIZE);
    if (message->iov_length && !message->iov) return fail(NET_EFAULT);
    std::uint64_t total = 0;
    for (int i = 0; i < message->iov_length; ++i) {
        const auto& entry = message->iov[i];
        if (!entry.base && entry.length) return fail(NET_EFAULT);
        if (entry.length > INT_MAX - total) return fail(NET_EINVAL);
        total += entry.length;
    }
    return static_cast<std::int64_t>(total);
}

}  // namespace

extern "C" {

extern const std::uint32_t sce_net_in6addr_any[4] = {};

int* APS5_VABI sceNetErrnoLoc(void) {
    return errno_slot();
}

int PollSockets(KernelSocketPoll::Entry* entries, int count, int timeoutMilliseconds) {
    std::vector<std::shared_ptr<NativeSocketHandle>> natives(static_cast<std::size_t>(count));
    {
        std::lock_guard lk(g_mutex);
        for (int index = 0; index < count; ++index) {
            const auto socket = g_socks.find(entries[index].descriptor);
            if (socket != g_socks.end()) natives[static_cast<std::size_t>(index)] = socket->second.native;
        }
    }
#ifdef _WIN32
    std::vector<WSAPOLLFD> descriptors;
    constexpr short NativeReadable = POLLRDNORM;
    constexpr short NativeUrgent = POLLRDBAND;
    constexpr short NativeWritable = POLLWRNORM;
#else
    std::vector<pollfd> descriptors;
    constexpr short NativeReadable = POLLIN;
    constexpr short NativeUrgent = POLLPRI;
    constexpr short NativeWritable = POLLOUT;
#endif
    std::vector<int> owners;
    for (int index = 0; index < count; ++index) {
        auto& entry = entries[index];
        if (!natives[static_cast<std::size_t>(index)]) {
            entry.revents = KernelSocketPoll::Unknown;
            continue;
        }
        decltype(descriptors)::value_type descriptor{};
        descriptor.fd = natives[static_cast<std::size_t>(index)]->value;
        if (entry.events & KernelSocketPoll::Readable) descriptor.events |= NativeReadable;
        if (entry.events & KernelSocketPoll::Urgent) descriptor.events |= NativeUrgent;
        if (entry.events & KernelSocketPoll::Writable) descriptor.events |= NativeWritable;
        descriptors.push_back(descriptor);
        owners.push_back(index);
    }
    if (descriptors.empty()) return 0;
#ifdef _WIN32
    const int result = WSAPoll(descriptors.data(), static_cast<ULONG>(descriptors.size()), timeoutMilliseconds);
#else
    const int result = ::poll(descriptors.data(), descriptors.size(), timeoutMilliseconds);
#endif
    if (result < 0) return -native_error();
    int ready = 0;
    for (std::size_t index = 0; index < descriptors.size(); ++index) {
        const short native = descriptors[index].revents;
        short revents = 0;
        if (native & NativeReadable) revents |= KernelSocketPoll::Readable;
        if (native & NativeUrgent) revents |= KernelSocketPoll::Urgent;
        if (native & NativeWritable) revents |= KernelSocketPoll::Writable;
        if (native & (POLLERR | POLLNVAL)) revents |= KernelSocketPoll::Error;
        if (native & POLLHUP) revents |= KernelSocketPoll::HangUp;
        entries[owners[index]].revents = revents;
        if (revents != 0) ++ready;
    }
    return ready;
}

const bool g_socketPollerRegistered = (KernelSetSocketPoller_nid_no_patch(&PollSockets), true);

int APS5_VABI sceNetInit_nid_postfix(void) {
    std::lock_guard lk(g_mutex);
    g_net_inited = true;
    return 0;
}

int APS5_VABI sceNetTerm(void) {
    std::lock_guard lk(g_mutex);
    g_net_inited = false;
    return 0;
}

int APS5_VABI sceNetPoolCreate(const char* name, int size, int flags) {
    (void)name;
    (void)flags;
    if (size <= 0) {
        return fail(NET_EINVAL);
    }
    std::lock_guard lk(g_mutex);
    const int id = g_next_pool++;
    g_pools.emplace(id, static_cast<std::size_t>(size));
    return id;
}

int APS5_VABI sceNetPoolDestroy(int memid) {
    std::lock_guard lk(g_mutex);
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
    std::lock_guard lk(g_mutex);
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
        std::lock_guard lk(g_mutex);
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
    g_cv.NotifyAll();
    return 0;
}

int APS5_VABI sceNetSocketAbort(int s, int flags) {
    (void)flags;
    std::shared_ptr<NativeSocketHandle> native;
    {
        std::lock_guard lk(g_mutex);
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
    g_cv.NotifyAll();
    return 0;
}

int APS5_VABI sceNetBind_nid_postfix(int s, const void* addr, uint32_t addrlen) {
    Sock socket;
    {
        std::lock_guard lk(g_mutex);
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
    std::lock_guard lk(g_mutex);
    auto it = g_socks.find(s);
    if (it != g_socks.end()) it->second.bound = true;
    return 0;
}

int APS5_VABI sceNetListen(int s, int backlog) {
    Sock socket;
    {
        std::lock_guard lk(g_mutex);
        auto it = g_socks.find(s);
        if (it == g_socks.end()) return fail(NET_EBADF);
        socket = it->second;
    }
    if (socket.type != NET_SOCK_STREAM) return fail(NET_EOPNOTSUPP);
    if (::listen(socket.native->value, backlog) != 0) return fail(native_error());
    std::lock_guard lk(g_mutex);
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
        std::lock_guard lk(g_mutex);
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
        std::lock_guard lk(g_mutex);
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
        std::lock_guard lk(g_mutex);
        auto it = g_socks.find(s);
        if (it == g_socks.end()) return fail(NET_EBADF);
        socket = it->second;
    }
    sockaddr_storage native{};
    NativeLength native_length = 0;
    if (!guest_to_native_address(addr, addrlen, native, native_length)) return fail(NET_EINVAL);
    if (native.ss_family != (socket.family == NET_AF_INET ? AF_INET : AF_INET6)) return fail(NET_EAFNOSUPPORT);
    if (::connect(socket.native->value, reinterpret_cast<const sockaddr*>(&native), native_length) == 0) return 0;
#ifdef _WIN32
    if (WSAGetLastError() == WSAEWOULDBLOCK) return fail(NET_EINPROGRESS);
#endif
    return fail(native_error());
}

int64_t APS5_VABI sceNetRecv(int s, void* buf, size_t len, int flags) {
    if (flags != 0 && flags != 2) return fail(NET_EOPNOTSUPP);
    if (!buf && len) return fail(NET_EINVAL);
    if (len > INT_MAX) return fail(NET_EMSGSIZE);
    Sock socket;
    {
        std::lock_guard lk(g_mutex);
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
        std::lock_guard lk(g_mutex);
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
        std::lock_guard lk(g_mutex);
        auto it = g_socks.find(s);
        if (it == g_socks.end()) return fail(NET_EBADF);
        socket = it->second;
    }
    const int result = ::send(socket.native->value, static_cast<const char*>(buf), static_cast<int>(len), NATIVE_SEND_FLAGS);
    return result >= 0 ? result : fail(native_error());
}

int64_t APS5_VABI sceNetSendto(int s, const void* buf, size_t len, int flags, const void* to, uint32_t tolen) {
    if (flags != 0) return fail(NET_EOPNOTSUPP);
    if (!buf && len) return fail(NET_EINVAL);
    if (len > INT_MAX) return fail(NET_EMSGSIZE);
    Sock socket;
    {
        std::lock_guard lk(g_mutex);
        auto it = g_socks.find(s);
        if (it == g_socks.end()) return fail(NET_EBADF);
        socket = it->second;
    }
    int result;
    if (!to) {
        if (tolen != 0) return fail(NET_EINVAL);
        result = ::send(socket.native->value, static_cast<const char*>(buf), static_cast<int>(len), NATIVE_SEND_FLAGS);
    } else {
        sockaddr_storage destination{};
        NativeLength destination_length = 0;
        if (!guest_to_native_address(to, tolen, destination, destination_length)) return fail(NET_EINVAL);
        if (destination.ss_family != (socket.family == NET_AF_INET ? AF_INET : AF_INET6)) return fail(NET_EAFNOSUPPORT);
        result = ::sendto(socket.native->value, static_cast<const char*>(buf), static_cast<int>(len), NATIVE_SEND_FLAGS,
            reinterpret_cast<const sockaddr*>(&destination), destination_length);
    }
    return result >= 0 ? result : fail(native_error());
}

int64_t APS5_VABI sceNetSendmsg(int s, const NetMsghdr* msg, int flags) {
    const auto total = message_length(msg);
    if (total < 0) return total;
    if (msg->control && msg->control_length) throw std::runtime_error("sceNetSendmsg: control data is not supported");
    std::vector<char> buffer;
    try {
        buffer.resize(static_cast<std::size_t>(total));
    } catch (const std::bad_alloc&) {
        return fail(55);
    }
    std::size_t offset = 0;
    for (int i = 0; i < msg->iov_length; ++i) {
        if (msg->iov[i].length) std::memcpy(buffer.data() + offset, msg->iov[i].base, msg->iov[i].length);
        offset += msg->iov[i].length;
    }
    return sceNetSendto(s, buffer.data(), buffer.size(), flags, msg->name, msg->name ? msg->name_length : 0u);
}

int64_t APS5_VABI sceNetRecvmsg(int s, NetMsghdr* msg, int flags) {
    const auto total = message_length(msg);
    if (total < 0) return total;
    if (flags != 0 && flags != NET_MSG_PEEK) return fail(NET_EOPNOTSUPP);
    Sock socket;
    {
        std::lock_guard lk(g_mutex);
        auto it = g_socks.find(s);
        if (it == g_socks.end()) return fail(NET_EBADF);
        socket = it->second;
    }
    const bool datagram = socket.type != NET_SOCK_STREAM;
    std::vector<char> buffer;
    try {
        buffer.resize(static_cast<std::size_t>(total) + (datagram ? 1u : 0u));
    } catch (const std::bad_alloc&) {
        return fail(55);
    }
    sockaddr_storage peer{};
    NativeLength peer_length = sizeof(peer);
    std::int64_t received = ::recvfrom(socket.native->value, buffer.data(), static_cast<int>(buffer.size()),
        flags == NET_MSG_PEEK ? MSG_PEEK : 0, reinterpret_cast<sockaddr*>(&peer), &peer_length);
    if (received < 0) {
#ifdef _WIN32
        if (!datagram || WSAGetLastError() != WSAEMSGSIZE) return fail(native_error());
        received = static_cast<std::int64_t>(buffer.size());
#else
        return fail(native_error());
#endif
    }
    msg->flags = 0;
    if (received > total) {
        received = total;
        msg->flags = NET_MSG_TRUNC;
    }
    std::size_t offset = 0;
    for (int i = 0; i < msg->iov_length && offset < static_cast<std::size_t>(received); ++i) {
        const auto count = std::min<std::size_t>(msg->iov[i].length, static_cast<std::size_t>(received) - offset);
        std::memcpy(msg->iov[i].base, buffer.data() + offset, count);
        offset += count;
    }
    if (msg->name && !native_to_guest_address(peer, msg->name, &msg->name_length)) msg->name_length = 0;
    msg->control_length = 0;
    return received;
}

int APS5_VABI sceNetShutdown(int s, int how) {
    if (how < 0 || how > 2) return fail(NET_EINVAL);
    Sock socket;
    {
        std::lock_guard lk(g_mutex);
        auto it = g_socks.find(s);
        if (it == g_socks.end()) return fail(NET_EBADF);
        socket = it->second;
    }
    return ::shutdown(socket.native->value, how) == 0 ? 0 : fail(native_error());
}

int APS5_VABI sceNetSetsockopt(int s, int level, int optname, const void* optval, uint32_t optlen) {
    Sock socket;
    {
        std::lock_guard lk(g_mutex);
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
    std::lock_guard lk(g_mutex);
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
        std::lock_guard lk(g_mutex);
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
        std::lock_guard lk(g_mutex);
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
        std::lock_guard lk(g_mutex);
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
    std::lock_guard lk(g_mutex);
    return g_socks.count(s) != 0 ? 0 : fail(NET_EBADF);
}

int APS5_VABI sceNetEpollCreate(const char* name, int flags) {
    (void)name;
    (void)flags;
    std::lock_guard lk(g_mutex);
    const int id = g_next_epoll++;
    g_epolls.insert(id);
    g_epoll_socks.emplace(id, std::map<int, NetEpollEvent>{});
    return id;
}

int APS5_VABI sceNetGetMemoryPoolStats(int memid, NetMemoryPoolStats* stats) {
    if (stats == nullptr) {
        return fail(NET_EINVAL);
    }
    std::lock_guard lk(g_mutex);
    const auto pool = g_pools.find(memid);
    if (pool == g_pools.end()) {
        return fail(NET_EBADF);
    }
    *stats = {pool->second, 0, 0};
    return 0;
}

int APS5_VABI sceNetEpollDestroy(int eid) {
    std::lock_guard lk(g_mutex);
    if (g_epolls.erase(eid) == 0) {
        return fail(NET_EBADF);
    }
    g_epoll_socks.erase(eid);
    g_epoll_aborted.erase(eid);
    g_cv.NotifyAll();
    return 0;
}

int APS5_VABI sceNetEpollAbort(int eid, int flags) {
    (void)flags;
    std::lock_guard lk(g_mutex);
    if (g_epolls.count(eid) == 0) {
        return fail(NET_EBADF);
    }
    g_epoll_aborted.insert(eid);
    g_cv.NotifyAll();
    return 0;
}

int APS5_VABI sceNetEpollControl(int eid, int op, int id, const NetEpollEvent* event) {
    std::lock_guard lk(g_mutex);
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
    g_cv.NotifyAll();
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
            std::lock_guard lk(g_mutex);
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
            std::unique_lock lk(g_mutex);
            const auto now = std::chrono::steady_clock::now();
            if (timeout > 0 && now >= deadline) return 0;
            const auto slice = std::chrono::milliseconds(100);
            const auto wake = timeout > 0 ? std::min(deadline, now + slice) : now + slice;
            g_cv.WaitUntil(lk, wake);
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
            std::lock_guard lk(g_mutex);
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
    if (src == nullptr || dst == nullptr) {
        *errno_slot() = NET_EINVAL;
        return nullptr;
    }
    if (af != NET_AF_INET && af != NET_AF_INET6) {
        *errno_slot() = NET_EAFNOSUPPORT;
        return nullptr;
    }
    char text[INET6_ADDRSTRLEN];
#ifdef _WIN32
    const auto* result = InetNtopA(af == NET_AF_INET ? AF_INET : AF_INET6, const_cast<void*>(src), text, sizeof(text));
#else
    const auto* result = ::inet_ntop(af == NET_AF_INET ? AF_INET : AF_INET6, src, text, sizeof(text));
#endif
    if (!result) {
        *errno_slot() = native_error();
        return nullptr;
    }
    const auto length = std::strlen(text) + 1;
    if (length > size) {
        *errno_slot() = NET_ENOSPC;
        return nullptr;
    }
    std::memcpy(dst, text, length);
    return dst;
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
    std::lock_guard lk(g_mutex);
    const int id = g_next_resolver++;
    g_resolvers[id] = 0;
    return id;
}

int APS5_VABI sceNetResolverDestroy(int rid) {
    std::lock_guard lk(g_mutex);
    return g_resolvers.erase(rid) != 0 ? 0 : fail(NET_EBADF);
}

int lookup_ipv4(int rid, const char* hostname, const char* func, std::vector<std::uint32_t>& addresses) {
    {
        std::lock_guard lk(g_mutex);
        if (g_resolvers.count(rid) == 0) return fail(NET_EBADF);
    }
    if (!initialize_sockets()) return fail(5);
    addrinfo hints{};
    hints.ai_family = AF_INET;
    addrinfo* results = nullptr;
    const int result = ::getaddrinfo(hostname, nullptr, &hints, &results);
    if (result != 0) {
        *errno_slot() = result == EAI_AGAIN ? NET_ETIMEDOUT : NET_ENOENT;
        log_soft(func, "host DNS lookup failed");
        set_resolver_error(rid, NET_ERROR_RESOLVER_ENODNS);
        return NET_ERROR_RESOLVER_ENODNS;
    }
    for (const addrinfo* entry = results; entry != nullptr; entry = entry->ai_next) {
        const auto address = reinterpret_cast<const sockaddr_in*>(entry->ai_addr)->sin_addr.s_addr;
        if (std::find(addresses.begin(), addresses.end(), address) == addresses.end()) addresses.push_back(address);
    }
    ::freeaddrinfo(results);
    set_resolver_error(rid, 0);
    return 0;
}

int APS5_VABI sceNetResolverStartNtoa(int rid, const char* hostname, void* addr, int timeout, int retry, int flags) {
    (void)timeout;
    (void)retry;
    (void)flags;
    if (!hostname || !addr) return fail(NET_EINVAL);
    std::vector<std::uint32_t> addresses;
    if (const int error = lookup_ipv4(rid, hostname, __func__, addresses)) return error;
    std::memcpy(addr, addresses.data(), sizeof(addresses[0]));
    return 0;
}

struct NetResolverRecord {
    std::uint32_t address;
    std::uint8_t address6[12];
    std::int32_t family;
    std::int32_t reserved[3];
};

struct NetResolverInfo {
    NetResolverRecord records[10];
    std::int32_t count;
    std::int32_t count4;
    std::int32_t reserved[14];
};
static_assert(sizeof(NetResolverRecord) == 32 && offsetof(NetResolverRecord, family) == 16);
static_assert(sizeof(NetResolverInfo) == 384 && offsetof(NetResolverInfo, count) == 320 && offsetof(NetResolverInfo, count4) == 324);

int APS5_VABI sceNetResolverStartNtoaMultipleRecordsEx(int rid, const char* hostname, NetResolverInfo* info, int timeout,
    int retry, int flags) {
    (void)timeout;
    (void)retry;
    if (flags != 0) throw std::runtime_error("sceNetResolverStartNtoaMultipleRecordsEx: flags " + std::to_string(flags) + " are not supported");
    if (!hostname || !info) return fail(NET_EINVAL);
    std::vector<std::uint32_t> addresses;
    if (const int error = lookup_ipv4(rid, hostname, __func__, addresses)) return error;
    *info = {};
    info->count = static_cast<std::int32_t>(std::min<std::size_t>(addresses.size(), std::size(info->records)));
    info->count4 = info->count;
    for (std::int32_t index = 0; index < info->count; ++index) {
        info->records[index].address = addresses[index];
        info->records[index].family = NET_AF_INET;
    }
    return 0;
}

int APS5_VABI sceNetResolverStartAton(int rid, const void* addr, char* hostname, int len, int timeout, int retry, int flags) {
    (void)timeout;
    (void)retry;
    (void)flags;
    if (!addr || !hostname || len <= 0) return fail(NET_EINVAL);
    {
        std::lock_guard lk(g_mutex);
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
        set_resolver_error(rid, NET_ERROR_RESOLVER_ENODNS);
        return NET_ERROR_RESOLVER_ENODNS;
    }
    set_resolver_error(rid, 0);
    return 0;
}

int APS5_VABI sceNetResolverGetError(int rid, int* status) {
    if (!status) return fail(NET_EINVAL);
    std::lock_guard lk(g_mutex);
    const auto resolver = g_resolvers.find(rid);
    if (resolver == g_resolvers.end()) return fail(NET_EBADF);
    *status = resolver->second;
    return 0;
}

int APS5_VABI sceNetResolverAbort(void) {
    throw std::runtime_error("sceNetResolverAbort: unknown signature");
}

int APS5_VABI sceNetResolverStartNtoaMultipleRecords() {
    throw std::runtime_error("sceNetResolverStartNtoaMultipleRecords: unknown signature");
}

extern const std::uint8_t in6addr_any_nid_postfix[16] = {};
extern const std::uint8_t in6addr_loopback_nid_postfix[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};

}
