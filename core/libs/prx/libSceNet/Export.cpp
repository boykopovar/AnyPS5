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
#include <array>
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
#include <utility>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libkernel/Socket/include/SocketPoll.hpp"

// Guest socket descriptors retain PS5 semantics while their transport is backed by host sockets.

namespace {
// FreeBSD errno numbers as reported through sceNetErrnoLoc (SCE_NET_ERROR_* is 0x80410100 + errno).
constexpr int NET_ENOENT = 2;
constexpr int NET_EINTR = 4;
constexpr int NET_EBADF = 9;
constexpr int NET_EACCES = 13;
constexpr int NET_EFAULT = 14;
constexpr int NET_EINVAL = 22;
constexpr int NET_ENOSPC = 28;
constexpr int NET_EDOM = 33;
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
constexpr int NET_RESOLVER_ABORT_FLAG_NTOA_PRESERVATION = 0x1;
constexpr int NET_RESOLVER_ABORT_FLAG_ATON_PRESERVATION = 0x2;

constexpr int NET_AF_INET = 2;
constexpr int NET_AF_INET6 = 28;
constexpr int NET_SOCK_STREAM = 1;
constexpr int NET_SOCK_DGRAM = 2;
constexpr int NET_SOCK_RAW = 3;
constexpr int NET_EPOLL_CTL_ADD = 1;
constexpr int NET_EPOLL_CTL_MOD = 2;
constexpr int NET_EPOLL_CTL_DEL = 3;
constexpr int NET_SOL_SOCKET = 0xFFFF;
constexpr int NET_SO_SNDTIMEO = 0x1005;
constexpr int NET_SO_RCVTIMEO = 0x1006;
constexpr int NET_SO_ERROR = 0x1007;
constexpr int NET_SO_NBIO = 0x1200;
constexpr int NET_SO_REUSEADDR = 0x4;
constexpr int NET_SO_BROADCAST = 0x20;
constexpr int NET_SO_ONESBCAST = 0x10000;
constexpr int NET_SO_SNDBUF = 0x1001;
constexpr int NET_SO_RCVBUF = 0x1002;
constexpr int NET_SO_SCE_SNDTIMEO = 0x1105;
constexpr int NET_SO_SCE_RCVTIMEO = 0x1106;
constexpr int NET_MSG_PEEK = 0x2;
constexpr int NET_MSG_TRUNC = 0x10;
constexpr int NET_MSG_DONTWAIT = 0x80;
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
int net_error(int code) {
    switch (code) {
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
int native_error() { return net_error(WSAGetLastError()); }
#else
using NativeSocket = int;
using NativeLength = socklen_t;
constexpr NativeSocket INVALID_NATIVE_SOCKET = -1;
constexpr int NATIVE_SEND_FLAGS = MSG_NOSIGNAL;
bool initialize_sockets() { return true; }
void close_socket(NativeSocket socket) { ::close(socket); }
int net_error(int code) {
    switch (code) {
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
int native_error() { return net_error(errno); }
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
    if (bytes[1] == NET_AF_INET && length >= 16) {
        auto& v4 = reinterpret_cast<sockaddr_in&>(native);
        v4.sin_family = AF_INET;
        std::memcpy(&v4.sin_port, bytes + 2, 2);
        std::memcpy(&v4.sin_addr, bytes + 4, 4);
        native_length = sizeof(v4);
        return true;
    }
    if (bytes[1] == NET_AF_INET6 && length >= 28) {
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
    if (native.ss_family == AF_UNSPEC) {
        *length = 0;
        return true;
    }
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
    const auto copied = std::min<std::uint32_t>(*length, bytes[0]);
    std::memcpy(address, bytes, copied);
    *length = copied;
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
    bool read_shut = false;
    int rcv_timeout_us = 0;
    int snd_timeout_us = 0;
    int reported_error = 0;
    int snd_buffer = 0;
    int rcv_buffer = 0;
    int onesbcast = 0;
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
struct Resolver {
    int error = 0;
    int running = 0;
    std::uint64_t aborts = 0;
    bool abort_ntoa = false;
    bool abort_aton = false;
};
std::map<int, Resolver> g_resolvers;
int g_next_sock = 32;
int g_next_epoll = 0x4000;
int g_next_pool = 1;
int g_next_resolver = 0x100;
bool g_net_inited = false;

int* errno_slot() {
    static thread_local int e = 0;
    return &e;
}

extern "C" int* APS5_VABI __error_nid_postfix();

void set_errno(int err) {
    *errno_slot() = err;
    *__error_nid_postfix() = err;
}

int fail(int err) {
    set_errno(err);
    return NET_ERROR_BASE | err;
}

int set_native_timeout(NativeSocket native, int optname, std::int64_t microseconds) {
    const int native_option = optname == NET_SO_RCVTIMEO ? SO_RCVTIMEO : SO_SNDTIMEO;
#ifdef _WIN32
    const std::int64_t milliseconds = (microseconds + 999) / 1000;
    const DWORD timeout_ms = milliseconds > static_cast<std::int64_t>(MAXDWORD) ? MAXDWORD : static_cast<DWORD>(milliseconds);
    return ::setsockopt(native, SOL_SOCKET, native_option, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
#else
    timeval timeout{};
    timeout.tv_sec = static_cast<time_t>(microseconds / 1000000);
    timeout.tv_usec = static_cast<suseconds_t>(microseconds % 1000000);
    return ::setsockopt(native, SOL_SOCKET, native_option, &timeout, sizeof(timeout));
#endif
}

int set_kernel_timeout(int descriptor, int optname, const void* value, std::uint32_t length) {
    KernelTimeval timeout{};
    if (value == nullptr && length != 0) return NET_EFAULT;
    if (length < sizeof(timeout)) return NET_EINVAL;
    std::memcpy(&timeout, value, sizeof(timeout));
    if (timeout.tv_sec < 0 || timeout.tv_usec < 0 || timeout.tv_usec >= 1000000) return NET_EDOM;
    const std::int64_t microseconds = std::min<std::int64_t>(timeout.tv_sec, INT32_MAX) * 1000000 + timeout.tv_usec;
    Sock socket;
    {
        std::lock_guard lk(g_mutex);
        const auto it = g_socks.find(descriptor);
        if (it == g_socks.end()) return NET_EBADF;
        socket = it->second;
    }
    if (set_native_timeout(socket.native->value, optname, microseconds) != 0) return native_error();
    std::lock_guard lk(g_mutex);
    const auto it = g_socks.find(descriptor);
    if (it != g_socks.end()) {
        const int stored = static_cast<int>(std::min<std::int64_t>(microseconds, INT_MAX));
        if (optname == NET_SO_RCVTIMEO) it->second.rcv_timeout_us = stored;
        else it->second.snd_timeout_us = stored;
    }
    return 0;
}

int begin_lookup(int rid, bool Resolver::*preservedAbort, std::uint64_t& generation) {
    std::lock_guard lk(g_mutex);
    const auto resolver = g_resolvers.find(rid);
    if (resolver == g_resolvers.end()) return fail(NET_EBADF);
    if (std::exchange(resolver->second.*preservedAbort, false)) {
        resolver->second.error = NET_ERROR_BASE | NET_EINTR;
        return fail(NET_EINTR);
    }
    ++resolver->second.running;
    generation = resolver->second.aborts;
    return 0;
}

int end_lookup(int rid, std::uint64_t generation, int error) {
    std::lock_guard lk(g_mutex);
    const auto resolver = g_resolvers.find(rid);
    if (resolver == g_resolvers.end()) return error;
    --resolver->second.running;
    if (resolver->second.aborts != generation) error = fail(NET_EINTR);
    resolver->second.error = error;
    return error;
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

int timeout_option(int optname) {
    if (optname == NET_SO_SCE_SNDTIMEO) return NET_SO_SNDTIMEO;
    if (optname == NET_SO_SCE_RCVTIMEO) return NET_SO_RCVTIMEO;
    return optname;
}

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

int receive_error(const Sock& socket) {
#ifdef _WIN32
    if (WSAGetLastError() == WSAETIMEDOUT && socket.type != NET_SOCK_STREAM && socket.rcv_timeout_us > 0) return NET_EAGAIN;
#else
    (void)socket;
#endif
    return native_error();
}

bool message_flags_supported(int flags, int accepted) {
#ifdef _WIN32
    accepted &= ~NET_MSG_DONTWAIT;
#endif
    return (flags & ~accepted) == 0;
}

int native_message_flags(int flags) {
    int native = (flags & NET_MSG_PEEK) ? MSG_PEEK : 0;
#ifndef _WIN32
    if (flags & NET_MSG_DONTWAIT) native |= MSG_DONTWAIT;
#endif
    return native;
}

#ifdef _WIN32
bool has_local_address(NativeSocket native) {
    sockaddr_storage local{};
    NativeLength local_length = sizeof(local);
    return ::getsockname(native, reinterpret_cast<sockaddr*>(&local), &local_length) == 0 || WSAGetLastError() != WSAEINVAL;
}

void note_local_address(int s, const Sock& socket) {
    if (socket.type == NET_SOCK_STREAM || socket.bound || !has_local_address(socket.native->value)) return;
    std::lock_guard lk(g_mutex);
    const auto it = g_socks.find(s);
    if (it != g_socks.end() && it->second.native == socket.native) it->second.bound = true;
    g_cv.NotifyAll();
}

constexpr int RECEIVE_NOTHING = -1;

int wait_for_datagram(const Sock& socket, std::chrono::steady_clock::time_point deadline) {
    if (deadline == std::chrono::steady_clock::time_point::max()) return 0;
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) return NET_EAGAIN;
    const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(deadline - now).count();
    WSAPOLLFD descriptor{};
    descriptor.fd = socket.native->value;
    descriptor.events = POLLRDNORM;
    return WSAPoll(&descriptor, 1, static_cast<INT>(std::min<long long>(remaining, INT_MAX))) == 0 ? NET_EAGAIN : 0;
}

int wait_for_local_address(int s, const Sock& socket, bool empty) {
    if (socket.type == NET_SOCK_STREAM || socket.bound) return 0;
    const bool named = has_local_address(socket.native->value);
    if (!named && (socket.read_shut || empty)) return RECEIVE_NOTHING;
    if (!named && socket.nonblock) return NET_EAGAIN;
    const auto deadline = socket.rcv_timeout_us > 0
        ? std::chrono::steady_clock::now() + std::chrono::microseconds(socket.rcv_timeout_us)
        : std::chrono::steady_clock::time_point::max();
    std::unique_lock lk(g_mutex);
    for (bool waited = false;; waited = true) {
        const auto it = g_socks.find(s);
        if (it == g_socks.end() || it->second.native != socket.native || it->second.aborted) return 0;
        if (named || has_local_address(socket.native->value)) {
            it->second.bound = true;
            lk.unlock();
            return waited ? wait_for_datagram(socket, deadline) : 0;
        }
        if (it->second.read_shut) return RECEIVE_NOTHING;
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) return NET_EAGAIN;
        g_cv.WaitUntil(lk, std::min(deadline, now + std::chrono::milliseconds(10)));
    }
}
#endif

}  // namespace

extern "C" {

extern const std::uint32_t sce_net_in6addr_any[4] = {};
alignas(std::uint32_t) extern const std::uint8_t sce_net_in6addr_linklocal_allnodes[16] = {0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};

int* APS5_VABI sceNetErrnoLoc(void) {
    return errno_slot();
}

int APS5_VABI sceNetShowNetstat(void) {
    return 0;
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

extern "C" int APS5_VABI sceNetSocketClose(int s);

bool CloseSocketForKernel(int descriptor, int* error) {
    {
        std::lock_guard lk(g_mutex);
        if (g_socks.count(descriptor) == 0) return false;
    }
    *error = sceNetSocketClose(descriptor) == 0 ? 0 : *errno_slot();
    return true;
}

const bool g_socketCloserRegistered = (KernelSetSocketCloser_nid_no_patch(&CloseSocketForKernel), true);

extern "C" int APS5_VABI sceNetSetsockopt(int s, int level, int optname, const void* optval, uint32_t optlen);

bool SetSocketOptionForKernel(int descriptor, int level, int option, const void* value, std::uint32_t length, int* error) {
    {
        std::lock_guard lk(g_mutex);
        if (g_socks.count(descriptor) == 0) return false;
    }
    if (level == NET_SOL_SOCKET && (option == NET_SO_RCVTIMEO || option == NET_SO_SNDTIMEO)) {
        *error = set_kernel_timeout(descriptor, option, value, length);
        if (*error != 0) set_errno(*error);
        return true;
    }
    *error = sceNetSetsockopt(descriptor, level, option, value, length) == 0 ? 0 : *errno_slot();
    return true;
}

const bool g_socketOptionSetterRegistered = (KernelSetSocketOptionSetter_nid_no_patch(&SetSocketOptionForKernel), true);

constexpr int GuestFcntlGetStatus = 3;
constexpr int GuestFcntlSetStatus = 4;
constexpr int GuestReadWrite = 2;
constexpr int GuestNonblocking = 4;
constexpr int GuestStatusFlagsMask = 7;

bool ControlSocketStatusForKernel(int descriptor, int command, int flags, int* result, int* error) {
    bool nonblock = false;
    {
        std::lock_guard lk(g_mutex);
        const auto it = g_socks.find(descriptor);
        if (it == g_socks.end()) return false;
        nonblock = it->second.nonblock;
    }
    *result = 0;
    *error = 0;
    if (command == GuestFcntlGetStatus) {
        *result = GuestReadWrite | (nonblock ? GuestNonblocking : 0);
        return true;
    }
    if (command != GuestFcntlSetStatus) {
        *error = NET_EINVAL;
        return true;
    }
    if ((flags & ~GuestStatusFlagsMask) != 0) {
        *error = NET_EOPNOTSUPP;
        return true;
    }
    const int enabled = (flags & GuestNonblocking) != 0 ? 1 : 0;
    if (sceNetSetsockopt(descriptor, NET_SOL_SOCKET, NET_SO_NBIO, &enabled, sizeof(enabled)) != 0) *error = *errno_slot();
    return true;
}

const bool g_socketStatusControlRegistered = (KernelSetSocketStatusControl_nid_no_patch(&ControlSocketStatusForKernel), true);

bool FindSocketNativeForKernel(int descriptor, std::shared_ptr<void>* owner, std::uintptr_t* native) {
    std::lock_guard lk(g_mutex);
    const auto it = g_socks.find(descriptor);
    if (it == g_socks.end() || !it->second.native) return false;
    *native = static_cast<std::uintptr_t>(it->second.native->value);
    *owner = it->second.native;
    return true;
}

const bool g_socketNativeLookupRegistered = (KernelSetSocketNativeLookup_nid_no_patch(&FindSocketNativeForKernel), true);

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
    const int ipv6_only = 1;
    if (family == NET_AF_INET6 &&
        ::setsockopt(native, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&ipv6_only), sizeof(ipv6_only)) != 0) {
        const int error = native_error();
        close_socket(native);
        return fail(error);
    }
    std::shared_ptr<NativeSocketHandle> handle;
    try {
        handle = std::make_shared<NativeSocketHandle>(native);
    } catch (const std::bad_alloc&) {
        close_socket(native);
        return fail(12);
    }
    const int fd = KernelReserveSocketNumber_nid_no_patch(static_cast<std::uintptr_t>(handle->value));
    if (fd < 0) return fail(-fd);
    std::lock_guard lk(g_mutex);
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
    KernelReleaseSocketNumber_nid_no_patch(s);
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
    g_cv.NotifyAll();
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
    const int descriptor = KernelReserveSocketNumber_nid_no_patch(static_cast<std::uintptr_t>(connection.native->value));
    if (descriptor < 0) return fail(-descriptor);
    {
        std::lock_guard lk(g_mutex);
        g_socks.emplace(descriptor, connection);
    }
    if (addr && !native_to_guest_address(peer, addr, addrlen)) {
        sceNetSocketClose(descriptor);
        return fail(NET_EAFNOSUPPORT);
    }
    return descriptor;
}

static std::uint16_t& native_port(sockaddr_storage& address) {
    return address.ss_family == AF_INET ? reinterpret_cast<sockaddr_in&>(address).sin_port : reinterpret_cast<sockaddr_in6&>(address).sin6_port;
}

static int disconnect_datagram(const Sock& socket) {
    const auto value = socket.native->value;
    sockaddr_storage peer{};
    NativeLength peer_length = sizeof(peer);
    if (::getpeername(value, reinterpret_cast<sockaddr*>(&peer), &peer_length) != 0) return 0;
    sockaddr_storage local{};
    NativeLength local_length = sizeof(local);
    if (::getsockname(value, reinterpret_cast<sockaddr*>(&local), &local_length) != 0) return fail(native_error());
#ifdef _WIN32
    sockaddr_storage unspecified{};
    unspecified.ss_family = local.ss_family;
    if (::connect(value, reinterpret_cast<const sockaddr*>(&unspecified), local_length) != 0) return fail(native_error());
#else
    sockaddr unspecified{};
    unspecified.sa_family = AF_UNSPEC;
    if (::connect(value, &unspecified, sizeof(unspecified)) != 0) return fail(native_error());
#endif
    sockaddr_storage current{};
    NativeLength current_length = sizeof(current);
    if (::getsockname(value, reinterpret_cast<sockaddr*>(&current), &current_length) != 0) return fail(native_error());
    if (native_port(current) != 0) return 0;
    sockaddr_storage wildcard{};
    wildcard.ss_family = local.ss_family;
    native_port(wildcard) = native_port(local);
    if (::bind(value, reinterpret_cast<const sockaddr*>(&wildcard), local_length) != 0) return fail(native_error());
    return 0;
}

#ifdef _WIN32
extern "C" void RefuseClosedLoopbackAtOnce_nid_no_patch(SOCKET socket, const sockaddr* address);
#endif

int APS5_VABI sceNetConnect(int s, const void* addr, uint32_t addrlen) {
    Sock socket;
    {
        std::lock_guard lk(g_mutex);
        auto it = g_socks.find(s);
        if (it == g_socks.end()) return fail(NET_EBADF);
        it->second.reported_error = 0;
        socket = it->second;
    }
    if (socket.type == NET_SOCK_DGRAM && addr && addrlen >= 2 && static_cast<const std::uint8_t*>(addr)[1] == 0) {
        if (const int error = disconnect_datagram(socket)) return error;
        return fail(NET_EAFNOSUPPORT);
    }
    sockaddr_storage native{};
    NativeLength native_length = 0;
    if (!guest_to_native_address(addr, addrlen, native, native_length)) return fail(NET_EINVAL);
#ifdef _WIN32
    if (socket.type == NET_SOCK_STREAM) RefuseClosedLoopbackAtOnce_nid_no_patch(socket.native->value, reinterpret_cast<const sockaddr*>(&native));
#endif
    if (native.ss_family != (socket.family == NET_AF_INET ? AF_INET : AF_INET6)) return fail(NET_EAFNOSUPPORT);
    if (::connect(socket.native->value, reinterpret_cast<const sockaddr*>(&native), native_length) == 0) {
#ifdef _WIN32
        note_local_address(s, socket);
#endif
        return 0;
    }
#ifdef _WIN32
    if (WSAGetLastError() == WSAEWOULDBLOCK) return fail(NET_EINPROGRESS);
#endif
    return fail(native_error());
}

int64_t APS5_VABI sceNetRecv(int s, void* buf, size_t len, int flags) {
    if (!message_flags_supported(flags, NET_MSG_PEEK | NET_MSG_DONTWAIT)) return fail(NET_EOPNOTSUPP);
    if (!buf && len) return fail(NET_EINVAL);
    if (len > INT_MAX) return fail(NET_EMSGSIZE);
    Sock socket;
    {
        std::lock_guard lk(g_mutex);
        auto it = g_socks.find(s);
        if (it == g_socks.end()) return fail(NET_EBADF);
        socket = it->second;
    }
#ifdef _WIN32
    if (const int wait = wait_for_local_address(s, socket, len == 0)) return wait == RECEIVE_NOTHING ? 0 : fail(wait);
#endif
    const GuestArena::HostWrite destination(len == 0 ? nullptr : buf, len);
    if (!destination.Open()) return fail(NET_EFAULT);
    const int native_flags = native_message_flags(flags);
    const int result = ::recv(socket.native->value, static_cast<char*>(buf), static_cast<int>(len), native_flags);
#ifdef _WIN32
    if (result < 0 && socket.type == NET_SOCK_DGRAM && WSAGetLastError() == WSAEMSGSIZE) return static_cast<int64_t>(len);
#endif
    return result >= 0 ? result : fail(receive_error(socket));
}

int64_t APS5_VABI sceNetRecvfrom(int s, void* buf, size_t len, int flags, void* from, uint32_t* fromlen) {
    if (!message_flags_supported(flags, NET_MSG_PEEK | NET_MSG_DONTWAIT)) return fail(NET_EOPNOTSUPP);
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
    const GuestArena::HostWrite lengthOutput(from == nullptr ? nullptr : fromlen, from == nullptr ? 0 : sizeof(*fromlen));
    if (!lengthOutput.Open()) return fail(NET_EFAULT);
#ifdef _WIN32
    if (const int wait = wait_for_local_address(s, socket, len == 0)) {
        if (wait != RECEIVE_NOTHING) return fail(wait);
        if (from) *fromlen = 0;
        return 0;
    }
#endif
    const auto addressBytes = from != nullptr && socket.type == NET_SOCK_DGRAM ?
        std::min<std::uint32_t>(*fromlen, socket.family == NET_AF_INET ? 16u : 28u) : 0u;
    const GuestArena::HostWrite addressOutput(addressBytes == 0 ? nullptr : from, addressBytes);
    if (!addressOutput.Open()) return fail(NET_EFAULT);
    const GuestArena::HostWrite destination(len == 0 ? nullptr : buf, len);
    if (!destination.Open()) return fail(NET_EFAULT);
    sockaddr_storage peer{};
    NativeLength peer_length = sizeof(peer);
    const int native_flags = native_message_flags(flags);
    int result = ::recvfrom(socket.native->value, static_cast<char*>(buf), static_cast<int>(len),
        native_flags, from ? reinterpret_cast<sockaddr*>(&peer) : nullptr, from ? &peer_length : nullptr);
#ifdef _WIN32
    if (result < 0 && socket.type == NET_SOCK_DGRAM && WSAGetLastError() == WSAEMSGSIZE) result = static_cast<int>(len);
#endif
    if (result < 0) return fail(receive_error(socket));
    if (from && !native_to_guest_address(peer, from, fromlen)) return fail(NET_EAFNOSUPPORT);
    return result;
}

int64_t APS5_VABI sceNetSend(int s, const void* buf, size_t len, int flags) {
    if (!message_flags_supported(flags, NET_MSG_DONTWAIT)) return fail(NET_EOPNOTSUPP);
    if (!buf && len) return fail(NET_EINVAL);
    if (len > INT_MAX) return fail(NET_EMSGSIZE);
    Sock socket;
    {
        std::lock_guard lk(g_mutex);
        auto it = g_socks.find(s);
        if (it == g_socks.end()) return fail(NET_EBADF);
        socket = it->second;
    }
    const int result = ::send(socket.native->value, static_cast<const char*>(buf), static_cast<int>(len), NATIVE_SEND_FLAGS | native_message_flags(flags));
    return result >= 0 ? result : fail(native_error());
}

int64_t APS5_VABI sceNetSendto(int s, const void* buf, size_t len, int flags, const void* to, uint32_t tolen) {
    if (!message_flags_supported(flags, NET_MSG_DONTWAIT)) return fail(NET_EOPNOTSUPP);
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
        result = ::send(socket.native->value, static_cast<const char*>(buf), static_cast<int>(len), NATIVE_SEND_FLAGS | native_message_flags(flags));
    } else {
        sockaddr_storage destination{};
        NativeLength destination_length = 0;
        if (!guest_to_native_address(to, tolen, destination, destination_length)) return fail(NET_EINVAL);
        if (destination.ss_family != (socket.family == NET_AF_INET ? AF_INET : AF_INET6)) return fail(NET_EAFNOSUPPORT);
        result = ::sendto(socket.native->value, static_cast<const char*>(buf), static_cast<int>(len), NATIVE_SEND_FLAGS | native_message_flags(flags),
            reinterpret_cast<const sockaddr*>(&destination), destination_length);
    }
    if (result < 0) return fail(native_error());
#ifdef _WIN32
    note_local_address(s, socket);
#endif
    return result;
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
    if (!message_flags_supported(flags, NET_MSG_PEEK | NET_MSG_DONTWAIT)) return fail(NET_EOPNOTSUPP);
    Sock socket;
    {
        std::lock_guard lk(g_mutex);
        auto it = g_socks.find(s);
        if (it == g_socks.end()) return fail(NET_EBADF);
        socket = it->second;
    }
    const bool datagram = socket.type != NET_SOCK_STREAM;
#ifdef _WIN32
    if (const int wait = wait_for_local_address(s, socket, total == 0)) {
        if (wait != RECEIVE_NOTHING) return fail(wait);
        msg->flags = 0;
        if (msg->name) msg->name_length = 0;
        msg->control_length = 0;
        return 0;
    }
#endif
    std::vector<char> buffer;
    try {
        buffer.resize(static_cast<std::size_t>(total) + (datagram ? 1u : 0u));
    } catch (const std::bad_alloc&) {
        return fail(55);
    }
    sockaddr_storage peer{};
    NativeLength peer_length = sizeof(peer);
    std::int64_t received = ::recvfrom(socket.native->value, buffer.data(), static_cast<int>(buffer.size()),
        native_message_flags(flags), reinterpret_cast<sockaddr*>(&peer), &peer_length);
    if (received < 0) {
#ifdef _WIN32
        if (!datagram || WSAGetLastError() != WSAEMSGSIZE) return fail(receive_error(socket));
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
    const int error = ::shutdown(socket.native->value, how) == 0 ? 0 : native_error();
    if (how != 1) {
        std::lock_guard lk(g_mutex);
        const auto it = g_socks.find(s);
        if (it != g_socks.end() && it->second.native == socket.native) it->second.read_shut = true;
        g_cv.NotifyAll();
    }
    return error == 0 ? 0 : fail(error);
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
    if (optname == NET_SO_RCVTIMEO || optname == NET_SO_SNDTIMEO) {
        const int error = set_kernel_timeout(s, optname, optval, optlen);
        return error == 0 ? 0 : fail(error);
    }
    optname = timeout_option(optname);
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
        if (set_native_timeout(socket.native->value, optname, value) != 0) return fail(native_error());
    } else if (optname == NET_SO_REUSEADDR || optname == NET_SO_BROADCAST || optname == NET_SO_SNDBUF || optname == NET_SO_RCVBUF) {
        const int native_option = optname == NET_SO_REUSEADDR ? SO_REUSEADDR : optname == NET_SO_BROADCAST ? SO_BROADCAST : optname == NET_SO_SNDBUF ? SO_SNDBUF : SO_RCVBUF;
        if (::setsockopt(socket.native->value, SOL_SOCKET, native_option, reinterpret_cast<const char*>(&value), sizeof(value)) != 0)
            return fail(native_error());
    } else if (optname == NET_SO_ONESBCAST) {
    } else {
        return fail(NET_EOPNOTSUPP);
    }
    std::lock_guard lk(g_mutex);
    auto it = g_socks.find(s);
    if (it != g_socks.end()) {
        if (optname == NET_SO_NBIO) it->second.nonblock = value != 0;
        if (optname == NET_SO_RCVTIMEO) it->second.rcv_timeout_us = value;
        if (optname == NET_SO_SNDTIMEO) it->second.snd_timeout_us = value;
        if (optname == NET_SO_SNDBUF) it->second.snd_buffer = value;
        if (optname == NET_SO_RCVBUF) it->second.rcv_buffer = value;
        if (optname == NET_SO_ONESBCAST) it->second.onesbcast = value;
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
    if (optval == nullptr || optlen == nullptr) return fail(NET_EINVAL);
    if (level == NET_SOL_SOCKET && (optname == NET_SO_RCVTIMEO || optname == NET_SO_SNDTIMEO)) {
        const int microseconds = optname == NET_SO_RCVTIMEO ? socket.rcv_timeout_us : socket.snd_timeout_us;
        const KernelTimeval timeout{microseconds / 1000000, microseconds % 1000000};
        *optlen = std::min(*optlen, static_cast<std::uint32_t>(sizeof(timeout)));
        std::memcpy(optval, &timeout, *optlen);
        return 0;
    }
    if (*optlen < sizeof(int)) return fail(NET_EINVAL);
    if (level != NET_SOL_SOCKET) return fail(NET_EOPNOTSUPP);
    int value = 0;
    switch (timeout_option(optname)) {
        case NET_SO_NBIO: value = socket.nonblock ? 1 : 0; break;
        case NET_SO_RCVTIMEO: value = socket.rcv_timeout_us; break;
        case NET_SO_SNDTIMEO: value = socket.snd_timeout_us; break;
        case NET_SO_ONESBCAST: value = socket.onesbcast; break;
        case NET_SO_ERROR: {
            int pending = 0;
            NativeLength size = sizeof(pending);
            if (::getsockopt(socket.native->value, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&pending), &size) != 0)
                return fail(native_error());
#ifdef _WIN32
            std::lock_guard lk(g_mutex);
            auto it = g_socks.find(s);
            if (it == g_socks.end()) return fail(NET_EBADF);
            if (pending != 0 && pending == it->second.reported_error) pending = 0;
            else if (pending != 0) it->second.reported_error = pending;
#endif
            value = pending == 0 ? 0 : net_error(pending);
            break;
        }
        case NET_SO_SNDBUF:
        case NET_SO_RCVBUF: {
            const bool send = optname == NET_SO_SNDBUF;
            value = send ? socket.snd_buffer : socket.rcv_buffer;
            if (value != 0) break;
            NativeLength size = sizeof(value);
            if (::getsockopt(socket.native->value, SOL_SOCKET, send ? SO_SNDBUF : SO_RCVBUF, reinterpret_cast<char*>(&value), &size) != 0)
                return fail(native_error());
            break;
        }
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
    if (::getsockname(socket.native->value, reinterpret_cast<sockaddr*>(&native), &native_length) != 0) {
#ifdef _WIN32
        if (WSAGetLastError() != WSAEINVAL) return fail(native_error());
        native = {};
        native.ss_family = socket.family == NET_AF_INET ? AF_INET : AF_INET6;
#else
        return fail(native_error());
#endif
    }
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

int APS5_VABI sceNetShowNetstatWithMemory(int memid) {
    struct SocketStatus { int id; int family; int type; bool nonblock; bool bound; bool listening; };
    std::vector<SocketStatus> sockets;
    std::size_t poolSize = 0;
    {
        std::lock_guard lk(g_mutex);
        const auto pool = g_pools.find(memid);
        if (pool == g_pools.end()) return fail(NET_EBADF);
        poolSize = pool->second;
        sockets.reserve(g_socks.size());
        for (const auto& [id, socket] : g_socks)
            sockets.push_back({id, socket.family, socket.type, socket.nonblock, socket.bound, socket.listening});
    }
    std::fprintf(stdout, "sceNet netstat: pool=%d bytes=%zu sockets=%zu\n", memid, poolSize, sockets.size());
    for (const auto& socket : sockets)
        std::fprintf(stdout, "socket=%d family=%d type=%d nonblock=%d bound=%d listening=%d\n", socket.id, socket.family, socket.type, int(socket.nonblock), int(socket.bound), int(socket.listening));
    return 0;
}

int APS5_VABI sceNetGetMemoryPoolStats(int memid, NetMemoryPoolStats* stats) {
    std::lock_guard lk(g_mutex);
    const auto pool = g_pools.find(memid);
    if (pool == g_pools.end()) {
        return fail(NET_EBADF);
    }
    if (stats == nullptr) {
        return fail(NET_EINVAL);
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
    if (op != NET_EPOLL_CTL_ADD && op != NET_EPOLL_CTL_MOD && op != NET_EPOLL_CTL_DEL) return fail(NET_EINVAL);
    if (op != NET_EPOLL_CTL_DEL && !event) return fail(NET_EINVAL);
    if (op != NET_EPOLL_CTL_DEL && g_socks.count(id) == 0) return fail(NET_EBADF);
    auto& registrations = g_epoll_socks[eid];
    if (op == NET_EPOLL_CTL_ADD) {
        if (!registrations.emplace(id, *event).second) return fail(NET_EADDRINUSE);
    } else if (op == NET_EPOLL_CTL_DEL) {
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
                    registered_events.push_back({entry.second.events, 0, static_cast<std::uint64_t>(entry.first), entry.second.data});
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
            if (registered_events[index].events & 2) descriptors[index].events |= POLLWRNORM;
        }
#else
        std::vector<pollfd> descriptors(registered_events.size());
        for (std::size_t index = 0; index < descriptors.size(); ++index) {
            descriptors[index].fd = native_sockets[index]->value;
            if (registered_events[index].events & 1) descriptors[index].events |= POLLIN;
            if (registered_events[index].events & 2) descriptors[index].events |= POLLOUT;
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
                if (native_events & POLLWRNORM) guest_events |= 2;
#else
                if (native_events & (POLLIN | POLLPRI)) guest_events |= 1;
                if (native_events & POLLOUT) guest_events |= 2;
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
        set_errno(NET_EINVAL);
        return nullptr;
    }
    if (af != NET_AF_INET && af != NET_AF_INET6) {
        set_errno(NET_EAFNOSUPPORT);
        return nullptr;
    }
    char text[INET6_ADDRSTRLEN];
#ifdef _WIN32
    const auto* result = InetNtopA(af == NET_AF_INET ? AF_INET : AF_INET6, const_cast<void*>(src), text, sizeof(text));
#else
    const auto* result = ::inet_ntop(af == NET_AF_INET ? AF_INET : AF_INET6, src, text, sizeof(text));
#endif
    if (!result) {
        set_errno(native_error());
        return nullptr;
    }
    const auto length = std::strlen(text) + 1;
    if (length > size) {
        set_errno(NET_ENOSPC);
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
    g_resolvers[id] = {};
    return id;
}

int APS5_VABI sceNetResolverDestroy(int rid) {
    std::lock_guard lk(g_mutex);
    return g_resolvers.erase(rid) != 0 ? 0 : fail(NET_EBADF);
}

int lookup_host(int rid, const char* hostname, const char* func, int family, std::vector<std::array<std::uint8_t, 16>>& addresses) {
    if (!initialize_sockets()) return fail(5);
    std::uint64_t generation = 0;
    if (const int error = begin_lookup(rid, &Resolver::abort_ntoa, generation)) return error;
    addrinfo hints{};
    hints.ai_family = family;
    addrinfo* results = nullptr;
    const int result = ::getaddrinfo(hostname, nullptr, &hints, &results);
    if (result != 0) {
        set_errno(result == EAI_AGAIN ? NET_ETIMEDOUT : NET_ENOENT);
        log_soft(func, "host DNS lookup failed");
        return end_lookup(rid, generation, NET_ERROR_RESOLVER_ENODNS);
    }
    for (const addrinfo* entry = results; entry != nullptr; entry = entry->ai_next) {
        if (entry->ai_family != family) continue;
        std::array<std::uint8_t, 16> address{};
        if (family == AF_INET6) std::memcpy(address.data(), &reinterpret_cast<const sockaddr_in6*>(entry->ai_addr)->sin6_addr, 16);
        else std::memcpy(address.data(), &reinterpret_cast<const sockaddr_in*>(entry->ai_addr)->sin_addr, 4);
        if (std::find(addresses.begin(), addresses.end(), address) == addresses.end()) addresses.push_back(address);
    }
    ::freeaddrinfo(results);
    if (addresses.empty()) {
        set_errno(NET_ENOENT);
        return end_lookup(rid, generation, NET_ERROR_RESOLVER_ENODNS);
    }
    return end_lookup(rid, generation, 0);
}

int lookup_ipv4(int rid, const char* hostname, const char* func, std::vector<std::uint32_t>& addresses) {
    std::vector<std::array<std::uint8_t, 16>> found;
    if (const int error = lookup_host(rid, hostname, func, AF_INET, found)) return error;
    for (const auto& address : found) {
        std::uint32_t value = 0;
        std::memcpy(&value, address.data(), sizeof(value));
        addresses.push_back(value);
    }
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

int APS5_VABI sceNetResolverStartNtoa6(int rid, const char* hostname, void* addr, int timeout, int retry, int flags) {
    (void)timeout;
    (void)retry;
    if (flags != 0) throw std::runtime_error("sceNetResolverStartNtoa6: flags " + std::to_string(flags) + " are not supported");
    if (!hostname || !addr) return fail(NET_EINVAL);
    std::vector<std::array<std::uint8_t, 16>> addresses;
    if (const int error = lookup_host(rid, hostname, __func__, AF_INET6, addresses)) return error;
    std::memcpy(addr, addresses.front().data(), addresses.front().size());
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

int lookup_records(int rid, const char* hostname, NetResolverInfo* info, int flags, const char* func) {
    if (flags != 0) throw std::runtime_error(std::string(func) + ": flags " + std::to_string(flags) + " are not supported");
    if (!hostname || !info) return fail(NET_EINVAL);
    std::vector<std::uint32_t> addresses;
    if (const int error = lookup_ipv4(rid, hostname, func, addresses)) return error;
    *info = {};
    info->count = static_cast<std::int32_t>(std::min<std::size_t>(addresses.size(), std::size(info->records)));
    info->count4 = info->count;
    for (std::int32_t index = 0; index < info->count; ++index) {
        info->records[index].address = addresses[index];
        info->records[index].family = NET_AF_INET;
    }
    return 0;
}

int APS5_VABI sceNetResolverStartNtoaMultipleRecords(int rid, const char* hostname, NetResolverInfo* info, int timeout,
    int retry, int flags) {
    (void)timeout;
    (void)retry;
    return lookup_records(rid, hostname, info, flags, __func__);
}

int APS5_VABI sceNetResolverStartNtoaMultipleRecordsEx(int rid, const char* hostname, NetResolverInfo* info, int timeout,
    int retry, int flags) {
    (void)timeout;
    (void)retry;
    return lookup_records(rid, hostname, info, flags, __func__);
}

int APS5_VABI sceNetResolverStartAton(int rid, const void* addr, char* hostname, int len, int timeout, int retry, int flags) {
    (void)timeout;
    (void)retry;
    (void)flags;
    if (!addr || !hostname || len <= 0) return fail(NET_EINVAL);
    if (!initialize_sockets()) return fail(5);
    std::uint64_t generation = 0;
    if (const int error = begin_lookup(rid, &Resolver::abort_aton, generation)) return error;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    std::memcpy(&address.sin_addr, addr, sizeof(address.sin_addr));
    std::vector<char> name(static_cast<std::size_t>(len));
    const int result = ::getnameinfo(reinterpret_cast<const sockaddr*>(&address), sizeof(address), name.data(),
        static_cast<NativeLength>(len), nullptr, 0, NI_NAMEREQD);
    if (result != 0) {
        set_errno(result == EAI_AGAIN ? NET_ETIMEDOUT : NET_ENOENT);
        log_soft(__func__, "host reverse DNS lookup failed");
        return end_lookup(rid, generation, NET_ERROR_RESOLVER_ENODNS);
    }
    if (const int error = end_lookup(rid, generation, 0)) return error;
    std::memcpy(hostname, name.data(), std::strlen(name.data()) + 1);
    return 0;
}

int APS5_VABI sceNetResolverGetError(int rid, int* status) {
    if (!status) return fail(NET_EINVAL);
    std::lock_guard lk(g_mutex);
    const auto resolver = g_resolvers.find(rid);
    if (resolver == g_resolvers.end()) return fail(NET_EBADF);
    *status = resolver->second.error;
    return 0;
}

int APS5_VABI sceNetResolverAbort(int rid, int flags) {
    constexpr int preservation = NET_RESOLVER_ABORT_FLAG_NTOA_PRESERVATION | NET_RESOLVER_ABORT_FLAG_ATON_PRESERVATION;
    std::lock_guard lk(g_mutex);
    const auto resolver = g_resolvers.find(rid);
    if (resolver == g_resolvers.end()) return fail(NET_EBADF);
    if ((flags & ~preservation) != 0) throw std::runtime_error("sceNetResolverAbort: flags " + std::to_string(flags) + " are not supported");
    if (resolver->second.running != 0) {
        ++resolver->second.aborts;
        resolver->second.error = NET_ERROR_BASE | NET_EINTR;
        return 0;
    }
    if ((flags & NET_RESOLVER_ABORT_FLAG_NTOA_PRESERVATION) != 0) resolver->second.abort_ntoa = true;
    if ((flags & NET_RESOLVER_ABORT_FLAG_ATON_PRESERVATION) != 0) resolver->second.abort_aton = true;
    return 0;
}

int APS5_VABI sceNetShowIfconfig() {
    return 0;
}

extern const std::uint8_t in6addr_any_nid_postfix[16] = {};
extern const std::uint8_t in6addr_loopback_nid_postfix[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};

}
