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
#include <netinet/tcp.h>
#include <sys/ioctl.h>
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
#include <deque>
#include <memory>
#include <map>
#include <mutex>
#include <new>
#include <random>
#include <vector>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include "prx/libkernel/Socket/include/SocketError.hpp"
#include "prx/libkernel/Socket/include/SocketPoll.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "src/ResolverTestHooks.hpp"

// Guest socket descriptors retain PS5 semantics while their transport is backed by host sockets.

struct NetWlanInformationElement;
struct NetWlanWakeParameter;
struct NetWlanScanParameter;
struct NetWlanScanResult;
struct NetWlanAdhocScanParameter;
struct NetWlanInfraScanParameter;
struct NetWlanJoinResult;
struct NetDumpFilterParameter;

struct NetWlanAdhocParameter;
struct NetWlanWakeInfo;
struct NetWlanPspWakeInfo;
struct NetWlanPspWakeParameter;
struct NetWlanApParameter;
struct NetWlanDeviceInfo;
struct NetWlanAntennaParameter;
struct NetWlanRateParameter;
struct NetWlanDeviceConfiguration;
struct NetWlanRssiInfo;
struct NetDhcpInfo;
struct NetAutoIpInfo;
struct NetAddress6Info;

struct NetDhcpOptionInfo {
    std::uint32_t option;
    std::uint32_t capacity;
    void* data;
    std::uint32_t size;
    std::uint32_t reserved;
};

static_assert(sizeof(NetDhcpOptionInfo) == 24);
static_assert(offsetof(NetDhcpOptionInfo, capacity) == 4);
static_assert(offsetof(NetDhcpOptionInfo, data) == 8);
static_assert(offsetof(NetDhcpOptionInfo, size) == 16);

struct NetSockInfo6;
struct NetStatisticsInfo;
struct NetInterfaceStats;
struct NetRouteInfo;
struct NetDebugParameter;
struct NetArpInfo;
struct NetAppMemoryInfo;
struct NetSystemMemoryInfo;
struct NetBandwidthParameter;
struct NetDataTraffic;
struct NetEmulationParameter;

struct NetResolverConnection;
struct NetThreadParameter;
struct NetIpv4InterfaceAddress;
struct NetStatisticsInfoInternal;
struct NetInitializeParameter;
struct NetDhcpStartParameter;
struct NetAddress6StartParameter;

using NetAddressCallback = void (APS5_VABI *)(void* userArg, std::int32_t event, std::int32_t error);

struct NetDhcpdStartParameter {
    std::uint32_t addresses[5];
    std::uint32_t maxClients;
};

struct NetPppoeStartParameter {
    std::uint32_t dnsAddresses[2];
    char userName[128];
    char password[128];
    std::int32_t mtu;
    std::uint32_t flags;
};

struct NetRoutingAddressParameter {
    std::uint32_t address;
    std::uint32_t mask;
};

struct NetRoutingServerParameter {
    std::int32_t count;
    std::uint32_t localAddresses[2];
    std::uint32_t remoteAddresses[2];
};

struct NetRoutingRule {
    std::int32_t protocol;
    std::uint32_t firstAddress;
    std::uint16_t firstPort;
    std::uint16_t reserved;
    std::uint32_t secondAddress;
    std::uint16_t secondPort;
    std::uint16_t flags;
};

struct NetRoutingRuleParameter {
    std::int32_t count;
    const NetRoutingRule* rules;
};

static_assert(sizeof(NetRoutingAddressParameter) == 8);
static_assert(sizeof(NetRoutingServerParameter) == 20);
static_assert(offsetof(NetRoutingServerParameter, remoteAddresses) == 12);
static_assert(sizeof(NetRoutingRule) == 20);
static_assert(offsetof(NetRoutingRule, firstPort) == 8);
static_assert(offsetof(NetRoutingRule, secondAddress) == 12);
static_assert(offsetof(NetRoutingRule, flags) == 18);
static_assert(sizeof(NetRoutingRuleParameter) == 16);
static_assert(offsetof(NetRoutingRuleParameter, rules) == 8);

static_assert(sizeof(NetDhcpdStartParameter) == 24);
static_assert(offsetof(NetDhcpdStartParameter, maxClients) == 20);
static_assert(sizeof(NetPppoeStartParameter) == 272);
static_assert(offsetof(NetPppoeStartParameter, userName) == 8);
static_assert(offsetof(NetPppoeStartParameter, password) == 136);
static_assert(offsetof(NetPppoeStartParameter, mtu) == 264);
static_assert(offsetof(NetPppoeStartParameter, flags) == 268);

struct NetInfoDumpParameter {
    std::uint32_t flags;
    std::uint32_t intervalSeconds;
    char path[64];
};

static_assert(sizeof(NetInfoDumpParameter) == 72);
static_assert(offsetof(NetInfoDumpParameter, path) == 8);

struct NetInterfaceInfo;
struct NetInterfaceStatistics;

struct NetResolverConnectOption {
    std::int32_t connectionTimeout;
    std::int32_t resolverTimeout;
    std::int32_t resolverRetryCount;
    std::uint32_t reserved;
};

static_assert(sizeof(NetResolverConnectOption) == 16);
static_assert(offsetof(NetResolverConnectOption, resolverTimeout) == 4);
static_assert(offsetof(NetResolverConnectOption, resolverRetryCount) == 8);
static_assert(offsetof(NetResolverConnectOption, reserved) == 12);

struct NetSyncObject {
    void* mutex;
    void* condition;
    std::uint32_t signals;
    std::uint32_t reserved;
};

static_assert(sizeof(NetSyncObject) == 24);

using NetEventCallback = std::uint64_t (APS5_VABI*)(std::uint32_t interfaceId, std::uint64_t events, void* userData, void* waitData);

struct NetIpv6Address {
    std::uint8_t bytes[16];
};

static_assert(sizeof(NetIpv6Address) == 16);

struct NetDnsInfo {
    std::uint32_t primaryAddress;
    std::uint32_t secondaryAddress;
};

struct NetDns6Info {
    NetIpv6Address primaryAddress;
    NetIpv6Address secondaryAddress;
};

static_assert(sizeof(NetDnsInfo) == 8);
static_assert(sizeof(NetDns6Info) == 32);

struct NetIpv6InterfaceAddress {
    NetIpv6Address address;
    std::uint8_t prefixLength;
    std::uint8_t reserved[3];
};

static_assert(sizeof(NetIpv6InterfaceAddress) == 20);

namespace {
// FreeBSD errno numbers as reported through sceNetErrnoLoc (SCE_NET_ERROR_* is 0x80410100 + errno).
constexpr int NET_ENOENT = 2;
constexpr int NET_EINTR = 4;
constexpr int NET_EBADF = 9;
constexpr int NET_EACCES = 13;
constexpr int NET_EFAULT = 14;
constexpr int NET_EINVAL = 22;
constexpr int NET_ENOTTY = 25;
constexpr int NET_ENOSPC = 28;
constexpr int NET_EDOM = 33;
constexpr int NET_EAGAIN = 35;
constexpr int NET_EINPROGRESS = 36;
constexpr int NET_EALREADY = 37;
constexpr int NET_ENOTSOCK = 38;
constexpr int NET_ENOPROTOOPT = 42;
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
constexpr int NET_ENOTINIT = 200;
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
constexpr int NET_IPPROTO_TCP = 6;
constexpr int NET_TCP_NODELAY = 1;
constexpr int NET_TF_NODELAY = 4;
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
int net_error(int code) { return GuestSockets::GuestErrorFromHost(code); }
int native_error() { return net_error(WSAGetLastError()); }
#else
using NativeSocket = int;
using NativeLength = socklen_t;
constexpr NativeSocket INVALID_NATIVE_SOCKET = -1;
constexpr int NATIVE_SEND_FLAGS = MSG_NOSIGNAL;
bool initialize_sockets() { return true; }
void close_socket(NativeSocket socket) { ::close(socket); }
int net_error(int code) { return GuestSockets::GuestErrorFromHost(code); }
int native_error() { return net_error(errno); }
#endif

#ifdef _WIN32
int TransferSocketForKernel(KernelSocketPoll::NativeOwner* owner, bool send, void* buffer, int length);
#endif

struct NativeSocketHandle : KernelSocketPoll::NativeOwner {
    NativeSocket value;
    const bool stream;
    std::atomic<bool> nonblock{false};
    std::atomic<int> receiveTimeout{0};
    std::atomic<int> sendTimeout{0};
    NativeSocketHandle(NativeSocket socket, bool stream) : value(socket), stream(stream) {
#ifdef _WIN32
        transfer = &TransferSocketForKernel;
#endif
    }
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
    bool bound = false;
    bool listening = false;
    bool aborted = false;
    bool read_shut = false;
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
using EpollRegistrations = std::map<int, std::shared_ptr<const NetEpollEvent>>;
std::map<int, EpollRegistrations> g_epoll_socks;
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
std::atomic<GuestSceNetTest::ResolverLookup> g_resolverLookupForTests{nullptr};

int* errno_slot() {
    static thread_local int e = 0;
    return &e;
}

extern "C" int* APS5_VABI __error_nid_postfix();
extern "C" std::size_t NetCtlHostDnsServers_nid_no_patch(std::uint32_t* servers, std::size_t capacity);

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
        if (optname == NET_SO_RCVTIMEO) it->second.native->receiveTimeout = stored;
        else it->second.native->sendTimeout = stored;
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

bool message_flags_supported(int flags, int accepted) {
    return (flags & ~accepted) == 0;
}

template <typename TOperation>
auto native_transfer(const NativeSocketHandle& socket, int flags, bool send, TOperation operation, bool timed = true, int length = 0) {
#ifdef _WIN32
    using Result = decltype(operation(0));
    const bool nonblocking = (flags & NET_MSG_DONTWAIT) != 0 || socket.nonblock.load();
    const int completeLength = send && socket.stream && !nonblocking ? length : 0;
    const int timeout = timed ? (send ? socket.sendTimeout.load() : socket.receiveTimeout.load()) : 0;
    const auto deadline = timeout > 0 ? std::chrono::steady_clock::now() + std::chrono::microseconds(timeout)
        : std::chrono::steady_clock::time_point::max();
    int transferred = 0;
    for (;;) {
        const auto result = operation(transferred);
        if (result != static_cast<Result>(-1)) {
            if (completeLength == 0) return result;
            transferred += static_cast<int>(result);
            if (transferred == completeLength || result == 0) return static_cast<Result>(transferred);
        } else if (WSAGetLastError() != WSAEWOULDBLOCK || nonblocking) {
            return transferred > 0 ? static_cast<Result>(transferred) : result;
        }
        int milliseconds = 10;
        if (timeout > 0) {
            const auto remaining = deadline - std::chrono::steady_clock::now();
            if (remaining <= std::chrono::steady_clock::duration::zero()) {
                WSASetLastError(send && completeLength == 0 ? WSAETIMEDOUT : WSAEWOULDBLOCK);
                return transferred > 0 ? static_cast<Result>(transferred) : static_cast<Result>(-1);
            }
            milliseconds = static_cast<int>(std::min<std::int64_t>(std::chrono::ceil<std::chrono::milliseconds>(remaining).count(), milliseconds));
        }
        WSAPOLLFD descriptor{};
        descriptor.fd = socket.value;
        descriptor.events = send ? POLLWRNORM : POLLRDNORM;
        const int ready = WSAPoll(&descriptor, 1, milliseconds);
        if (ready < 0) return transferred > 0 ? static_cast<Result>(transferred) : static_cast<Result>(-1);
    }
#else
    static_cast<void>(socket);
    static_cast<void>(flags);
    static_cast<void>(send);
    static_cast<void>(timed);
    static_cast<void>(length);
    return operation(0);
#endif
}

#ifdef _WIN32
int TransferSocketForKernel(KernelSocketPoll::NativeOwner* owner, bool send, void* buffer, int length) {
    const auto* socket = static_cast<NativeSocketHandle*>(owner);
    return native_transfer(*socket, 0, send, [&](int offset) {
        return send ? ::send(socket->value, offset ? static_cast<const char*>(buffer) + offset : static_cast<const char*>(buffer), length - offset, 0)
            : ::recv(socket->value, static_cast<char*>(buffer), length, 0);
    }, true, length);
}
#endif

int native_message_flags(int flags) {
    int native = (flags & NET_MSG_PEEK) ? MSG_PEEK : 0;
#ifndef _WIN32
    if (flags & NET_MSG_DONTWAIT) native |= MSG_DONTWAIT;
#endif
    return native;
}

#if defined(_WIN32) || defined(__APPLE__)
#ifdef __APPLE__
bool has_local_address(NativeSocket native) {
    sockaddr_storage local{};
    NativeLength local_length = sizeof(local);
    if (::getsockname(native, reinterpret_cast<sockaddr*>(&local), &local_length) != 0) return true;
    if (local.ss_family == AF_INET) return reinterpret_cast<const sockaddr_in&>(local).sin_port != 0;
    if (local.ss_family == AF_INET6) return reinterpret_cast<const sockaddr_in6&>(local).sin6_port != 0;
    return true;
}
#else
bool has_local_address(NativeSocket native) {
    sockaddr_storage local{};
    NativeLength local_length = sizeof(local);
    return ::getsockname(native, reinterpret_cast<sockaddr*>(&local), &local_length) == 0 || WSAGetLastError() != WSAEINVAL;
}
#endif

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
#ifdef _WIN32
    WSAPOLLFD descriptor{};
    descriptor.fd = socket.native->value;
    descriptor.events = POLLRDNORM;
    return WSAPoll(&descriptor, 1, static_cast<INT>(std::min<long long>(remaining, INT_MAX))) == 0 ? NET_EAGAIN : 0;
#else
    pollfd descriptor{};
    descriptor.fd = socket.native->value;
    descriptor.events = POLLIN;
    return ::poll(&descriptor, 1, static_cast<int>(std::min<long long>(remaining, INT_MAX))) == 0 ? NET_EAGAIN : 0;
#endif
}

int wait_for_local_address(int s, const Sock& socket, bool empty, int flags) {
    if (socket.type == NET_SOCK_STREAM || socket.bound) return 0;
    const bool named = has_local_address(socket.native->value);
    if (!named && (socket.read_shut || empty)) return RECEIVE_NOTHING;
    if (!named && (socket.native->nonblock || (flags & NET_MSG_DONTWAIT) != 0)) return NET_EAGAIN;
    const int timeout = socket.native->receiveTimeout;
    const auto deadline = timeout > 0
        ? std::chrono::steady_clock::now() + std::chrono::microseconds(timeout)
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

bool parse_ipv4_parts(const char* text, std::uint32_t& address) {
    std::array<std::uint32_t, 3> parts{};
    std::size_t count = 0;
    std::uint32_t value = 0;
    for (;;) {
        if (*text < '0' || *text > '9') return false;
        std::uint32_t base = 10;
        if (*text == '0') {
            ++text;
            if (*text == 'x' || *text == 'X') {
                base = 16;
                ++text;
            } else if (*text != '9') {
                base = 8;
            }
        }
        value = 0;
        for (;; ++text) {
            const char c = *text;
            if (c >= '0' && c <= '9') {
                if (base == 8 && c >= '8') return false;
                value = value * base + static_cast<std::uint32_t>(c - '0');
            } else if (base == 16 && ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                value = value * 16 + static_cast<std::uint32_t>((c | 0x20) - 'a' + 10);
            } else {
                break;
            }
        }
        if (*text != '.') break;
        if (count == parts.size() || value > 0xff) return false;
        parts[count++] = value;
        ++text;
    }
    if (*text != '\0' && std::strchr(" \t\n\v\f\r", *text) == nullptr) return false;
    if (value > (0xffffffffu >> (8 * count))) return false;
    for (std::size_t i = 0; i < count; ++i) value |= parts[i] << (24 - 8 * i);
    address = value;
    return true;
}

}  // namespace

void GuestSceNetTest::SetResolverLookupForTests_nid_no_patch(ResolverLookup lookup) noexcept {
    g_resolverLookupForTests.store(lookup);
}

extern "C" {

extern const std::uint32_t sce_net_in6addr_any[4] = {};
alignas(std::uint32_t) extern const std::uint8_t sce_net_in6addr_linklocal_allnodes[16] = {0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};

int* APS5_VABI sceNetErrnoLoc(void) {
    return errno_slot();
}

int APS5_VABI sceNetShowNetstat(void) {
    std::lock_guard lock(g_mutex);
    if (!g_net_inited) return fail(NET_ENOTINIT);
    throw std::runtime_error(__func__);
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
        nonblock = it->second.native->nonblock;
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
    *owner = std::static_pointer_cast<KernelSocketPoll::NativeOwner>(it->second.native);
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
#ifdef _WIN32
    u_long nonblocking = 1;
    if (ioctlsocket(native, FIONBIO, &nonblocking) != 0) {
        const int error = native_error();
        close_socket(native);
        return fail(error);
    }
#endif
    const int ipv6_only = 1;
    if (family == NET_AF_INET6 &&
        ::setsockopt(native, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&ipv6_only), sizeof(ipv6_only)) != 0) {
        const int error = native_error();
        close_socket(native);
        return fail(error);
    }
    std::shared_ptr<NativeSocketHandle> handle;
    try {
        handle = std::make_shared<NativeSocketHandle>(native, type == NET_SOCK_STREAM);
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

extern "C" int APS5_VABI close_nid_postfix(int d);

int APS5_VABI sceNetSocketClose(int s) {
    std::shared_ptr<NativeSocketHandle> native;
    bool found = false;
    {
        std::lock_guard lk(g_mutex);
        const auto it = g_socks.find(s);
        if (it != g_socks.end()) {
            native = it->second.native;
            g_socks.erase(it);
            for (auto& epoll : g_epoll_socks) epoll.second.erase(s);
            found = true;
        }
    }
    if (!found) {
        if (!GuestSocketIsOpen_nid_no_patch(s)) return fail(NET_EBADF);
        return close_nid_postfix(s) == 0 ? 0 : fail(*__error_nid_postfix());
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
    const NativeSocket accepted = native_transfer(*listener.native, 0, false, [&](int) {
        peer_length = sizeof(peer);
        return ::accept(listener.native->value,
            addr ? reinterpret_cast<sockaddr*>(&peer) : nullptr, addr ? &peer_length : nullptr);
    }, false);
    if (accepted == INVALID_NATIVE_SOCKET) return fail(native_error());
    std::shared_ptr<NativeSocketHandle> handle;
    try {
        handle = std::make_shared<NativeSocketHandle>(accepted, true);
    } catch (const std::bad_alloc&) {
        close_socket(accepted);
        return fail(12);
    }
    Sock connection;
    connection.family = listener.family;
    connection.type = NET_SOCK_STREAM;
    connection.native = std::move(handle);
#ifdef _WIN32
    connection.native->receiveTimeout = listener.native->receiveTimeout.load();
    connection.native->sendTimeout = listener.native->sendTimeout.load();
#endif
    const int descriptor = KernelReserveSocketNumber_nid_no_patch(static_cast<std::uintptr_t>(connection.native->value));
    if (descriptor < 0) return fail(-descriptor);
    {
        std::lock_guard lk(g_mutex);
        g_socks.emplace(descriptor, connection);
    }
    if (listener.native->nonblock) {
        const int enabled = 1;
        if (sceNetSetsockopt(descriptor, NET_SOL_SOCKET, NET_SO_NBIO, &enabled, sizeof(enabled)) != 0) {
            const int error = *errno_slot();
            sceNetSocketClose(descriptor);
            return fail(error);
        }
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
#if defined(_WIN32) || defined(__APPLE__)
        note_local_address(s, socket);
#endif
        return 0;
    }
#ifdef _WIN32
    if (WSAGetLastError() == WSAEWOULDBLOCK) {
        if (socket.native->nonblock) return fail(NET_EINPROGRESS);
        fd_set writable;
        fd_set failed;
        FD_ZERO(&writable);
        FD_ZERO(&failed);
        FD_SET(socket.native->value, &writable);
        FD_SET(socket.native->value, &failed);
        if (::select(0, nullptr, &writable, &failed, nullptr) < 0) return fail(native_error());
        int error = 0;
        int size = sizeof(error);
        if (::getsockopt(socket.native->value, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &size) != 0)
            return fail(native_error());
        return error == 0 ? 0 : fail(net_error(error));
    }
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
#if defined(_WIN32) || defined(__APPLE__)
    if (const int wait = wait_for_local_address(s, socket, len == 0, flags)) return wait == RECEIVE_NOTHING ? 0 : fail(wait);
#endif
    const GuestArena::HostWrite destination(len == 0 ? nullptr : buf, len);
    if (!destination.Open()) return fail(NET_EFAULT);
    const int native_flags = native_message_flags(flags);
    const int result = native_transfer(*socket.native, flags, false, [&](int) {
        return ::recv(socket.native->value, static_cast<char*>(buf), static_cast<int>(len), native_flags);
    });
#ifdef _WIN32
    if (result < 0 && socket.type == NET_SOCK_DGRAM && WSAGetLastError() == WSAEMSGSIZE) return static_cast<int64_t>(len);
#endif
    return result >= 0 ? result : fail(native_error());
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
#if defined(_WIN32) || defined(__APPLE__)
    if (const int wait = wait_for_local_address(s, socket, len == 0, flags)) {
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
    int result = native_transfer(*socket.native, flags, false, [&](int) {
        peer_length = sizeof(peer);
        return ::recvfrom(socket.native->value, static_cast<char*>(buf), static_cast<int>(len),
            native_flags, from ? reinterpret_cast<sockaddr*>(&peer) : nullptr, from ? &peer_length : nullptr);
    });
#ifdef _WIN32
    if (result < 0 && socket.type == NET_SOCK_DGRAM && WSAGetLastError() == WSAEMSGSIZE) result = static_cast<int>(len);
#endif
    if (result < 0) return fail(native_error());
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
    const int result = native_transfer(*socket.native, flags, true, [&](int offset) {
        return ::send(socket.native->value, offset ? static_cast<const char*>(buf) + offset : static_cast<const char*>(buf), static_cast<int>(len) - offset, NATIVE_SEND_FLAGS | native_message_flags(flags));
    }, true, static_cast<int>(len));
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
        result = native_transfer(*socket.native, flags, true, [&](int offset) {
            return ::send(socket.native->value, offset ? static_cast<const char*>(buf) + offset : static_cast<const char*>(buf), static_cast<int>(len) - offset, NATIVE_SEND_FLAGS | native_message_flags(flags));
        }, true, static_cast<int>(len));
    } else {
        sockaddr_storage destination{};
        NativeLength destination_length = 0;
        if (!guest_to_native_address(to, tolen, destination, destination_length)) return fail(NET_EINVAL);
        if (destination.ss_family != (socket.family == NET_AF_INET ? AF_INET : AF_INET6)) return fail(NET_EAFNOSUPPORT);
        result = native_transfer(*socket.native, flags, true, [&](int offset) {
            return ::sendto(socket.native->value, offset ? static_cast<const char*>(buf) + offset : static_cast<const char*>(buf), static_cast<int>(len) - offset, NATIVE_SEND_FLAGS | native_message_flags(flags),
                reinterpret_cast<const sockaddr*>(&destination), destination_length);
        }, true, static_cast<int>(len));
    }
    if (result < 0) return fail(native_error());
#if defined(_WIN32) || defined(__APPLE__)
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
    const GuestArena::HostWrite messageOutput(msg, sizeof(*msg));
    if (!messageOutput.Open()) return fail(NET_EFAULT);
    std::deque<GuestArena::HostWrite> destinations;
    for (int i = 0; i < msg->iov_length; ++i) {
        const auto& entry = msg->iov[i];
        if (entry.length == 0) continue;
        if (!destinations.emplace_back(entry.base, static_cast<std::size_t>(entry.length)).Open()) return fail(NET_EFAULT);
    }
    const bool datagram = socket.type != NET_SOCK_STREAM;
#if defined(_WIN32) || defined(__APPLE__)
    if (const int wait = wait_for_local_address(s, socket, total == 0, flags)) {
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
    std::int64_t received = native_transfer(*socket.native, flags, false, [&](int) {
        peer_length = sizeof(peer);
        return ::recvfrom(socket.native->value, buffer.data(), static_cast<int>(buffer.size()),
            native_message_flags(flags), reinterpret_cast<sockaddr*>(&peer), &peer_length);
    });
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
    const int error = ::shutdown(socket.native->value, how) == 0 ? 0 : native_error();
    if (how != 1) {
        std::lock_guard lk(g_mutex);
        const auto it = g_socks.find(s);
        if (it != g_socks.end() && it->second.native == socket.native) it->second.read_shut = true;
        g_cv.NotifyAll();
    }
    return error == 0 ? 0 : fail(error);
}

extern "C" int APS5_VABI setsockopt_nid_postfix(int descriptor, int level, int option, const void* value, std::uint32_t length);

int APS5_VABI sceNetSetsockopt(int s, int level, int optname, const void* optval, uint32_t optlen) {
    Sock socket;
    bool found = false;
    {
        std::lock_guard lk(g_mutex);
        auto it = g_socks.find(s);
        if (it != g_socks.end()) {
            socket = it->second;
            found = true;
        }
    }
    if (!found) {
        if (!GuestSocketIsOpen_nid_no_patch(s)) return fail(NET_EBADF);
        return setsockopt_nid_postfix(s, level, optname, optval, optlen) == 0 ? 0 : fail(*__error_nid_postfix());
    }
    if (level == NET_IPPROTO_TCP && socket.type != NET_SOCK_STREAM) return fail(NET_EINVAL);
    if (level == NET_IPPROTO_TCP && optname == NET_TCP_NODELAY) {
        if (!optval || optlen < sizeof(int)) return fail(NET_EINVAL);
        int value = 0;
        std::memcpy(&value, optval, sizeof(value));
        const int enabled = value != 0;
        if (::setsockopt(socket.native->value, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&enabled), sizeof(enabled)) != 0)
            return fail(native_error());
        return 0;
    }
    if (level == NET_IPPROTO_TCP) return fail(NET_ENOPROTOOPT);
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
#ifndef _WIN32
        const int old_flags = fcntl(socket.native->value, F_GETFL, 0);
        if (old_flags < 0 || fcntl(socket.native->value, F_SETFL,
            value ? old_flags | O_NONBLOCK : old_flags & ~O_NONBLOCK) != 0) return fail(native_error());
#endif
    } else if (optname == NET_SO_RCVTIMEO || optname == NET_SO_SNDTIMEO) {
        if (value < 0) return fail(NET_EINVAL);
        if (set_native_timeout(socket.native->value, optname, value) != 0) return fail(native_error());
    } else if (optname == NET_SO_REUSEADDR || optname == NET_SO_BROADCAST || optname == NET_SO_SNDBUF || optname == NET_SO_RCVBUF) {
        if ((optname == NET_SO_SNDBUF || optname == NET_SO_RCVBUF) && value < 1) return fail(NET_EINVAL);
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
        if (optname == NET_SO_NBIO) it->second.native->nonblock = value != 0;
        if (optname == NET_SO_RCVTIMEO) it->second.native->receiveTimeout = value;
        if (optname == NET_SO_SNDTIMEO) it->second.native->sendTimeout = value;
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
        const int microseconds = optname == NET_SO_RCVTIMEO ? socket.native->receiveTimeout.load() : socket.native->sendTimeout.load();
        const KernelTimeval timeout{microseconds / 1000000, microseconds % 1000000};
        *optlen = std::min(*optlen, static_cast<std::uint32_t>(sizeof(timeout)));
        std::memcpy(optval, &timeout, *optlen);
        return 0;
    }
    if (*optlen < sizeof(int)) return fail(NET_EINVAL);
    if (level == NET_IPPROTO_TCP && socket.type != NET_SOCK_STREAM) return fail(NET_EINVAL);
    if (level == NET_IPPROTO_TCP && optname == NET_TCP_NODELAY) {
        int enabled = 0;
        NativeLength size = sizeof(enabled);
        if (::getsockopt(socket.native->value, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&enabled), &size) != 0)
            return fail(native_error());
        const int value = enabled != 0 ? NET_TF_NODELAY : 0;
        std::memcpy(optval, &value, sizeof(value));
        *optlen = sizeof(value);
        return 0;
    }
    if (level == NET_IPPROTO_TCP) return fail(NET_ENOPROTOOPT);
    if (level != NET_SOL_SOCKET) return fail(NET_EOPNOTSUPP);
    int value = 0;
    switch (timeout_option(optname)) {
        case NET_SO_NBIO: value = socket.native->nonblock ? 1 : 0; break;
        case NET_SO_RCVTIMEO: value = socket.native->receiveTimeout; break;
        case NET_SO_SNDTIMEO: value = socket.native->sendTimeout; break;
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

constexpr int NET_SOCKINFO_F_SELF = 0x1;
constexpr int NET_SOCKINFO_F_NONBLOCK = 0x200000;
constexpr int NET_SOCKINFO_TIME_WAIT = 0x40;

struct NetSockInfo {
    char name[32];
    std::int32_t pid;
    std::int32_t s;
    std::int8_t socket_type;
    std::int8_t policy;
    std::int8_t priority;
    std::int8_t reserved8;
    std::int32_t recv_queue_length;
    std::int32_t send_queue_length;
    std::uint32_t local_adr;
    std::uint32_t remote_adr;
    std::uint16_t local_port;
    std::uint16_t remote_port;
    std::uint16_t local_vport;
    std::uint16_t remote_vport;
    std::int32_t state;
    std::int32_t flags;
    std::int32_t tx_bps;
    std::int32_t rx_bps;
    std::int32_t max_tx_bps;
    std::int32_t max_rx_bps;
    std::int32_t tx_vbps;
    std::int32_t rx_vbps;
    std::int32_t recv_buffer_size;
    std::int32_t send_buffer_size;
    std::int32_t reserved6[8];
    std::int32_t tx_drops;
    std::int32_t rx_drops;
    std::int32_t tx_wait;
    std::int32_t reserved[2];
};
static_assert(sizeof(NetSockInfo) == 0xA0 && offsetof(NetSockInfo, s) == 0x24 && offsetof(NetSockInfo, local_adr) == 0x34 &&
    offsetof(NetSockInfo, local_port) == 0x3C && offsetof(NetSockInfo, recv_buffer_size) == 0x64);

static int sock_buffer_size(const Sock& socket, bool send, std::int32_t& value) {
    value = send ? socket.snd_buffer : socket.rcv_buffer;
    if (value != 0) return 0;
    NativeLength size = sizeof(value);
    return ::getsockopt(socket.native->value, SOL_SOCKET, send ? SO_SNDBUF : SO_RCVBUF, reinterpret_cast<char*>(&value), &size) == 0 ? 0 : native_error();
}

static int fill_sock_info(int s, NetSockInfo& record) {
    Sock socket;
    {
        std::lock_guard lk(g_mutex);
        const auto it = g_socks.find(s);
        if (it == g_socks.end()) return NET_EBADF;
        socket = it->second;
    }
    if (socket.family != NET_AF_INET) return NET_EAFNOSUPPORT;
    sockaddr_in local{};
    NativeLength length = sizeof(local);
    if (::getsockname(socket.native->value, reinterpret_cast<sockaddr*>(&local), &length) != 0) {
#ifdef _WIN32
        if (WSAGetLastError() != WSAEINVAL) return native_error();
#else
        return native_error();
#endif
    }
    sockaddr_in remote{};
    length = sizeof(remote);
    if (::getpeername(socket.native->value, reinterpret_cast<sockaddr*>(&remote), &length) != 0) {
        const int error = native_error();
        if (error != NET_ENOTCONN) return error;
    }
    record = {};
    if (const int error = sock_buffer_size(socket, false, record.recv_buffer_size)) return error;
    if (const int error = sock_buffer_size(socket, true, record.send_buffer_size)) return error;
    record.s = s;
    record.socket_type = static_cast<std::int8_t>(socket.type);
    record.local_adr = local.sin_addr.s_addr;
    record.remote_adr = remote.sin_addr.s_addr;
    record.local_port = local.sin_port;
    record.remote_port = remote.sin_port;
    record.flags = NET_SOCKINFO_F_SELF | (socket.native->nonblock ? NET_SOCKINFO_F_NONBLOCK : 0);
    return 0;
}

int APS5_VABI sceNetGetSockInfo(int s, void* info, int n, int flags) {
    if ((flags & ~NET_SOCKINFO_TIME_WAIT) != 0) return fail(NET_EINVAL);
    std::vector<int> listed;
    if (s < 0) {
        std::lock_guard lk(g_mutex);
        for (const auto& [descriptor, socket] : g_socks) {
            if (socket.family == NET_AF_INET) listed.push_back(descriptor);
        }
    }
    if (!info) return s < 0 ? static_cast<int>(listed.size()) : fail(NET_EINVAL);
    if (n < 1) return fail(NET_EINVAL);
    NetSockInfo record;
    if (s >= 0) {
        if (const int error = fill_sock_info(s, record)) return fail(error);
        std::memcpy(info, &record, sizeof(record));
        return 1;
    }
    int written = 0;
    for (const int descriptor : listed) {
        if (written == n) break;
        const int error = fill_sock_info(descriptor, record);
        if (error == NET_EBADF || error == NET_EAFNOSUPPORT) continue;
        if (error != 0) return fail(error);
        std::memcpy(static_cast<char*>(info) + static_cast<std::size_t>(written) * sizeof(record), &record, sizeof(record));
        ++written;
    }
    return written;
}

int APS5_VABI sceNetEpollCreate(const char* name, int flags) {
    (void)name;
    (void)flags;
    std::lock_guard lk(g_mutex);
    const int id = g_next_epoll++;
    g_epolls.insert(id);
    g_epoll_socks.emplace(id, EpollRegistrations{});
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
            sockets.push_back({id, socket.family, socket.type, socket.native->nonblock, socket.bound, socket.listening});
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
    if (g_socks.count(id) == 0) return fail(NET_EBADF);
    auto& registrations = g_epoll_socks[eid];
    if (op == NET_EPOLL_CTL_ADD) {
        if (registrations.count(id) != 0) return fail(NET_EADDRINUSE);
        registrations.emplace(id, std::make_shared<const NetEpollEvent>(*event));
    } else if (op == NET_EPOLL_CTL_DEL) {
        if (registrations.erase(id) == 0) return fail(NET_ENOENT);
    } else {
        auto registration = registrations.find(id);
        if (registration == registrations.end()) return fail(NET_ENOENT);
        registration->second = std::make_shared<const NetEpollEvent>(*event);
    }
    g_cv.NotifyAll();
    return 0;
}

int APS5_VABI sceNetEpollWait(int eid, NetEpollEvent* events, int maxevents, int timeout) {
    if (!events || maxevents <= 0) return fail(NET_EINVAL);
    struct Watch {
        int descriptor;
        std::shared_ptr<const NetEpollEvent> registration;
        std::shared_ptr<NativeSocketHandle> native;
    };
    const auto deadline = timeout > 0
        ? std::chrono::steady_clock::now() + std::chrono::microseconds(timeout)
        : std::chrono::steady_clock::time_point::max();
    for (;;) {
        std::vector<Watch> watches;
        {
            std::lock_guard lk(g_mutex);
            if (g_epolls.count(eid) == 0) return fail(NET_EBADF);
            if (g_epoll_aborted.erase(eid) != 0) return fail(NET_ECONNABORTED);
            const auto registration = g_epoll_socks.find(eid);
            if (registration != g_epoll_socks.end()) {
                for (const auto& entry : registration->second) {
                    const auto socket = g_socks.find(entry.first);
                    if (socket == g_socks.end()) continue;
                    watches.push_back({entry.first, entry.second, socket->second.native});
                }
            }
        }
        if (watches.empty()) {
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
        std::vector<WSAPOLLFD> descriptors(watches.size());
        for (std::size_t index = 0; index < descriptors.size(); ++index) {
            descriptors[index].fd = watches[index].native->value;
            if (watches[index].registration->events & 1) descriptors[index].events |= POLLRDNORM;
            if (watches[index].registration->events & 2) descriptors[index].events |= POLLWRNORM;
        }
#else
        std::vector<pollfd> descriptors(watches.size());
        for (std::size_t index = 0; index < descriptors.size(); ++index) {
            descriptors[index].fd = watches[index].native->value;
            if (watches[index].registration->events & 1) descriptors[index].events |= POLLIN;
            if (watches[index].registration->events & 2) descriptors[index].events |= POLLOUT;
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
            if (ready_count > 0) {
                int count = 0;
                const auto& registrations = g_epoll_socks.at(eid);
                for (std::size_t index = 0; index < descriptors.size() && count < maxevents; ++index) {
                    const auto registration = registrations.find(watches[index].descriptor);
                    if (registration == registrations.end() || registration->second != watches[index].registration) continue;
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
                    events[count] = *watches[index].registration;
                    events[count].ident = static_cast<std::uint64_t>(watches[index].descriptor);
                    events[count].reserved = 0;
                    events[count].events = guest_events;
                    ++count;
                }
                if (count > 0) return count;
            }
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
    if (src == nullptr) {
        return fail(NET_EINVAL);
    }
    if (af != NET_AF_INET && af != NET_AF_INET6) {
        return fail(NET_EAFNOSUPPORT);
    }
    if (dst == nullptr && af == NET_AF_INET6) {
        return fail(NET_EINVAL);
    }
    const auto convert = [](int family, const std::string& text, unsigned char* address) {
#ifdef _WIN32
        return InetPtonA(family, text.c_str(), address);
#else
        return ::inet_pton(family, text.c_str(), address);
#endif
    };
    const std::string head(src, std::strcspn(src, " \t\n\v\f\r"));
    const auto colon = head.rfind(':');
    const bool dottedTail = af == NET_AF_INET6 && colon != std::string::npos && head.find('.', colon) != std::string::npos;
    std::array<unsigned char, 16> address{};
    if (dottedTail && convert(AF_INET, head.substr(colon + 1), address.data()) != 1) {
        return 0;
    }
    const std::string text = af == NET_AF_INET || dottedTail ? head : std::string(src);
    const int converted = convert(af == NET_AF_INET ? AF_INET : AF_INET6, text, address.data());
    if (converted == 1 && dst != nullptr) {
        std::memcpy(dst, address.data(), af == NET_AF_INET ? 4 : 16);
    }
    return converted;
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

int APS5_VABI sceNetEtherStrton(const char* text, NetEtherAddr* address) {
    if (text == nullptr || address == nullptr) return fail(NET_EINVAL);
    for (std::size_t index = 0; index < 17; ++index) {
        if (text[index] == '\0') return fail(NET_EINVAL);
    }
    const auto hexDigit = [](char character) {
        if (character >= '0' && character <= '9') return character - '0';
        if (character >= 'a' && character <= 'z') return character - 'a' + 10;
        if (character >= 'A' && character <= 'Z') return character - 'A' + 10;
        return 0x7f;
    };
    NetEtherAddr parsed{};
    for (std::size_t index = 0; index < sizeof(parsed.data); ++index) {
        if (static_cast<unsigned char>(text[index * 3]) >= 128 || static_cast<unsigned char>(text[index * 3 + 1]) >= 128) {
            NotImplemented_nid_no_patch(__func__);
        }
        parsed.data[index] = static_cast<std::uint8_t>(hexDigit(text[index * 3]) * 16 + hexDigit(text[index * 3 + 1]));
    }
    *address = parsed;
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
    if (const auto lookup = g_resolverLookupForTests.load()) {
        if (!lookup(hostname, family, addresses) || addresses.empty()) {
            set_errno(NET_ENOENT);
            log_soft(func, "host DNS lookup failed");
            return end_lookup(rid, generation, NET_ERROR_RESOLVER_ENODNS);
        }
        return end_lookup(rid, generation, 0);
    }
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

int reverse_lookup(int rid, const sockaddr* address, NativeLength addressLength, char* hostname, int len, const char* func) {
    if (!initialize_sockets()) return fail(5);
    std::uint64_t generation = 0;
    if (const int error = begin_lookup(rid, &Resolver::abort_aton, generation)) return error;
    std::vector<char> name(static_cast<std::size_t>(len));
    const int result = ::getnameinfo(address, addressLength, name.data(), static_cast<NativeLength>(len), nullptr, 0, NI_NAMEREQD);
    if (result != 0) {
        set_errno(result == EAI_AGAIN ? NET_ETIMEDOUT : NET_ENOENT);
        log_soft(func, "host reverse DNS lookup failed");
        return end_lookup(rid, generation, NET_ERROR_RESOLVER_ENODNS);
    }
    if (const int error = end_lookup(rid, generation, 0)) return error;
    std::memcpy(hostname, name.data(), std::strlen(name.data()) + 1);
    return 0;
}

int APS5_VABI sceNetResolverStartAton(int rid, const void* addr, char* hostname, int len, int timeout, int retry, int flags) {
    (void)timeout;
    (void)retry;
    (void)flags;
    if (!addr || !hostname || len <= 0) return fail(NET_EINVAL);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    std::memcpy(&address.sin_addr, addr, sizeof(address.sin_addr));
    return reverse_lookup(rid, reinterpret_cast<const sockaddr*>(&address), sizeof(address), hostname, len, __func__);
}

int APS5_VABI sceNetResolverStartAton6(int rid, const void* addr, char* hostname, int len, int timeout, int retry, int flags) {
    (void)timeout;
    (void)retry;
    if (flags != 0) throw std::runtime_error("sceNetResolverStartAton6: flags " + std::to_string(flags) + " are not supported");
    if (!addr || !hostname || len <= 0) return fail(NET_EINVAL);
    sockaddr_in6 address{};
    address.sin6_family = AF_INET6;
    std::memcpy(&address.sin6_addr, addr, sizeof(address.sin6_addr));
    return reverse_lookup(rid, reinterpret_cast<const sockaddr*>(&address), sizeof(address), hostname, len, __func__);
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
    std::lock_guard lock(g_mutex);
    if (!g_net_inited) return fail(NET_ENOTINIT);
    throw std::runtime_error(__func__);
}

extern const std::uint8_t in6addr_any_nid_postfix[16] = {};
extern const std::uint8_t in6addr_loopback_nid_postfix[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};

int APS5_VABI sceNetClearDnsCache(int flags) {
    std::lock_guard lock(g_mutex);
    if (!g_net_inited) return fail(NET_ENOTINIT);
    if (flags != 0) return fail(NET_EINVAL);
    return 0;
}

int APS5_VABI sceNetGetStatisticsInfo(NetStatisticsInfo* info, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetGetInterfaceStats(NetInterfaceStats* stats, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetSetDnsInfo(const NetDnsInfo* info, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetShowIfconfigWithMemory(std::int32_t poolId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetShowPolicy(void) {
    return 0;
}

int APS5_VABI sceNetShowPolicyWithMemory(std::int32_t poolId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetShowRoute(void) {
    return 0;
}

int APS5_VABI sceNetShowRouteWithMemory(std::int32_t poolId) {
    std::lock_guard lk(g_mutex);
    if (g_pools.find(poolId) == g_pools.end()) return fail(NET_EBADF);
    return 0;
}

int APS5_VABI sceNetResolverConnectAbort(NetResolverConnection* connection, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetResolverConnectCreate(const char* name, std::int32_t socketType, NetResolverConnection* connection, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetResolverConnect(NetResolverConnection* connection, const char* hostname, std::uint16_t port, const NetResolverConnectOption* option) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetResolverConnectDestroy(NetResolverConnection* connection) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetGetSockInfo6(std::int32_t socket, NetSockInfo6* info, std::uint32_t capacity, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetSetDns6Info(const NetDns6Info* info, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetShowRoute6(void) {
    return 0;
}

int APS5_VABI sceNetShowRoute6WithMemory(std::int32_t poolId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

NetRouteInfo* APS5_VABI sceNetAllocateAllRouteInfo(std::int32_t interfaceId, std::int32_t* count) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

NetRouteInfo* APS5_VABI sceNetAllocateAllRouteInfo6(std::int32_t interfaceId, std::int32_t* count) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetControl(std::uint32_t interfaceId, std::int32_t interfaceIndex, std::uint32_t command, void* data, std::uint32_t dataSize) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetDbgInit(NetDebugParameter* parameter, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetEventCallbackCreate(std::uint32_t interfaceId, NetEventCallback callback, void* userData, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetEventCallbackDestroy(std::uint32_t interfaceId, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetEventCallbackGetError(std::uint32_t interfaceId, std::int32_t* error, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetEventCallbackWaitCB(std::int32_t timeout, void* waitData, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetFreeAllRouteInfo(void* routes) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetGetArpInfo(NetArpInfo* info, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetGetAppMemInfo(NetAppMemoryInfo* info, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetGetDnsInfo(NetDnsInfo* info, std::uint32_t flags) {
    if (flags != 0) throw std::runtime_error("sceNetGetDnsInfo: flags " + std::to_string(flags) + " are not supported");
    if (!info) throw std::runtime_error("sceNetGetDnsInfo: a null info is not supported");
    std::array<std::uint32_t, 2> servers{};
    const std::size_t count = std::min(NetCtlHostDnsServers_nid_no_patch(servers.data(), servers.size()), servers.size());
    std::memcpy(info, servers.data(), sizeof(servers));
    return static_cast<int>(count);
}

int APS5_VABI sceNetGetIfList(std::int32_t interfaceId, NetInterfaceInfo* interfaces, std::uint32_t capacity) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetGetIfListEx(std::int32_t interfaceId, NetInterfaceInfo* interfaces, std::uint32_t capacity, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetGetIfListOnce(std::int32_t interfaceId, NetInterfaceStatistics* statistics, std::uint32_t count) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

const char* APS5_VABI sceNetGetIfName(std::uint32_t index) {
    static constexpr std::array<const char*, 10> names = {
        "lo0", "eth0", "eth1", "dbg0", "wlan0", "wlan1", "gbe0", "bt0", "phone0", "pppoe0"};
    if (index >= names.size()) {
        set_errno(NET_EINVAL);
        return "";
    }
    return names[index];
}

int APS5_VABI sceNetGetIfnameNumList(std::uint32_t* interfaceIds, std::int32_t capacity, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetGetNameToIndex(std::uint32_t interfaceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetGetRandom(std::uint32_t* output) {
    if (!g_net_inited) return fail(NET_ENOTINIT);
    if (output == nullptr) return fail(NET_EINVAL);
    static std::mutex mutex;
    static std::random_device device;
    const std::lock_guard<std::mutex> lock(mutex);
    *output = static_cast<std::uint32_t>(device());
    return 0;
}

int APS5_VABI sceNetGetRouteInfo(std::int32_t family, NetRouteInfo* routes, std::int32_t capacity, std::uint8_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetGetsockoptEx(int socketId, int level, int option, void* value, std::uint32_t* valueSize) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI sceNetGetSystemTime(std::uint64_t* output) {
    if (output != nullptr) *output = sceKernelGetProcessTime();
}

int APS5_VABI sceNetGetSystemMemInfo(NetSystemMemoryInfo* info, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetInitParam(const NetInitializeParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetInetPtonEx(int af, const char* src, void* dst, int) {
    if (af != NET_AF_INET) {
        return sceNetInetPton(af, src, dst);
    }
    if (src == nullptr) {
        return fail(NET_EINVAL);
    }
    std::uint32_t address = 0;
    if (!parse_ipv4_parts(src, address)) {
        return 0;
    }
    if (dst == nullptr) {
        return 1;
    }
    const std::array<unsigned char, 4> bytes{static_cast<unsigned char>(address >> 24), static_cast<unsigned char>(address >> 16),
                                             static_cast<unsigned char>(address >> 8), static_cast<unsigned char>(address)};
    std::memcpy(dst, bytes.data(), bytes.size());
    return 1;
}

extern "C" int APS5_VABI ioctl_nid_postfix(int descriptor, std::uint64_t request, void* argument);

int APS5_VABI sceNetIoctl(int socketId, std::uint32_t command, void* argument) {
    constexpr std::uint32_t nonblockingRequest = 0x8004667e;
    constexpr std::uint32_t readableRequest = 0x4004667f;
    std::shared_ptr<NativeSocketHandle> native;
    {
        std::lock_guard lk(g_mutex);
        const auto it = g_socks.find(socketId);
        if (it != g_socks.end()) native = it->second.native;
    }
    if (!native) {
        if (!GuestSocketIsOpen_nid_no_patch(socketId)) return fail(NET_EBADF);
        return ioctl_nid_postfix(socketId, command, argument) == 0 ? 0 : fail(*__error_nid_postfix());
    }
    if (command != nonblockingRequest && command != readableRequest) return fail(NET_ENOTTY);
    if (!argument) return fail(NET_EFAULT);
    if (command == nonblockingRequest) {
        const int enabled = *static_cast<const int*>(argument) != 0;
        return sceNetSetsockopt(socketId, NET_SOL_SOCKET, NET_SO_NBIO, &enabled, sizeof(enabled));
    }
#ifdef _WIN32
    u_long available = 0;
    if (ioctlsocket(native->value, FIONREAD, &available) != 0) return fail(native_error());
#else
    int available = 0;
    if (::ioctl(native->value, FIONREAD, &available) != 0) return fail(native_error());
#endif
    *static_cast<int*>(argument) = static_cast<int>(available);
    return 0;
}

void* APS5_VABI sceNetMemoryAllocate(std::int32_t size, int flags) {
    constexpr int zeroFill = 2;
    constexpr std::size_t blockAlignment = 64;
    if (size <= 0) return nullptr;
    void* memory = nullptr;
    try {
        memory = GuestHeap::GuestHeapAlign_nid_postfix(blockAlignment, static_cast<std::size_t>(size));
    } catch (const std::bad_alloc&) {
        return nullptr;
    } catch (const std::length_error&) {
        return nullptr;
    }
    if ((flags & zeroFill) != 0) std::memset(memory, 0, static_cast<std::size_t>(size));
    return memory;
}

void APS5_VABI sceNetMemoryFree(void* memory) {
    GuestHeap::GuestHeapFree_nid_postfix(memory);
}

int APS5_VABI sceNetSetDnsInfoToKernel(const NetDnsInfo* info, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetSetsockoptEx(int socketId, int level, int option, const void* value, std::uint32_t valueSize) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetSocketInternal(const char* name, int domain, int type, int protocol) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetSyncCreate(NetSyncObject* sync, std::uint32_t signals, const char* name) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetSyncDestroy(NetSyncObject* sync) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

NetSyncObject* APS5_VABI sceNetSyncGet(std::uint32_t index) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetSyncSignal(NetSyncObject* sync, std::uint32_t signals) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetSyncWait(NetSyncObject* sync, std::uint32_t* timeout) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetSysctl(const int* name, std::uint32_t nameLength, void* oldValue, std::size_t* oldSize, const void* newValue, std::size_t newSize) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetThreadCreate(Pthread* thread, const NetThreadParameter* parameter, const char* name) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetThreadExit(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetThreadJoin(void* thread) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetUsleep(int microseconds) {
    if (!g_net_inited) return fail(NET_ENOTINIT);
    if (microseconds < 0) return fail(NET_EINVAL);
    const int result = sceKernelUsleep_nid_postfix(static_cast<KernelUseconds>(microseconds));
    if (result < 0) set_errno((result & 0xFF00) == 0x100 ? result & 0xFF : 205);
    return result;
}

int APS5_VABI sceNetShowIfconfigForBuffer(char* buffer, std::int32_t bufferSize, std::int32_t poolId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetShowRouteForBuffer(char* buffer, std::int32_t bufferSize, std::int32_t poolId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetShowNetstatForBuffer(char* buffer, std::int32_t bufferSize, std::int32_t poolId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetShowNetstatEx(std::uint32_t flags) {
    (void)flags;
    return 0;
}

int APS5_VABI sceNetShowNetstatExForBuffer(char* buffer, std::int32_t bufferSize, std::int32_t poolId, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

const char* APS5_VABI sceNetInetNtopWithScopeId(std::int32_t family, const void* address, char* text, std::uint32_t size, std::uint32_t scopeId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetInetPtonWithScopeId(std::int32_t family, const char* text, void* address, std::uint32_t* scopeId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetGetDns6Info(NetDns6Info* info, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetSetDns6InfoToKernel(const NetDns6Info* info, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetShowRoute6ForBuffer(char* buffer, std::int32_t bufferSize, std::int32_t poolId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigGetIfFlags(std::uint32_t interfaceId, std::uint32_t* flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigSetIfFlags(std::uint32_t interfaceId, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigUnsetIfFlags(std::uint32_t interfaceId, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigDelIfaddr(int interfaceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigDownInterface(int interfaceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigSetDefaultRoute(const std::uint32_t* gateway) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigSetDefaultRouteDev(const std::uint32_t* gateway) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigSetIfaddr(int interfaceId, const std::uint32_t* address, const std::uint32_t* broadcastAddress, const std::uint32_t* netmask) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigSetIfLinkLocalAddr6(std::uint32_t interfaceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigUpInterface(int interfaceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigUpInterfaceWithFlags(std::uint32_t interfaceId, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigCleanUpAllInterfaces(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigFlushRoute(std::uint32_t interfaceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigFlushRoute6(std::uint32_t interfaceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigAddIfaddr(int interfaceId, const std::uint32_t* address, const std::uint32_t* broadcastAddress, const std::uint32_t* netmask) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigGetIfaddr(std::int32_t interfaceId, NetIpv4InterfaceAddress* addresses, std::int32_t capacity) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigSetIfmtu(std::uint32_t interfaceId, int mtu) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigAddArp(const NetEtherAddr* etherAddress, const std::uint32_t* address) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigAddArpWithInterface(int interfaceId, const NetEtherAddr* etherAddress, const std::uint32_t* address) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigAddMRoute(const std::uint32_t* address) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigAddRoute(const std::uint32_t* destination, const std::uint32_t* gateway, const std::uint32_t* netmask, int hostRoute) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigAddRouteWithInterface(int interfaceId, const std::uint32_t* destination, const std::uint32_t* gateway, const std::uint32_t* netmask, int hostRoute) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigDelArp(const NetEtherAddr* etherAddress, const std::uint32_t* address) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigDelArpWithInterface(int interfaceId, const NetEtherAddr* etherAddress, const std::uint32_t* address) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigDelDefaultRoute(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigDelDefaultRouteDev(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigDelMRoute(const std::uint32_t* address) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigDelRoute(const std::uint32_t* destination, const std::uint32_t* gateway, const std::uint32_t* netmask) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigGetDefaultRoute(std::uint32_t* gateway) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetDhcpGetAutoipInfo(std::int32_t interfaceId, NetAutoIpInfo* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetDhcpGetInfo(std::int32_t interfaceId, NetDhcpInfo* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetDhcpGetInfoEx(std::int32_t interfaceId, std::int32_t type, NetDhcpOptionInfo* info, std::uint32_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetDhcpStart(std::uint32_t interfaceId, NetAddressCallback callback, void* userArg, std::int32_t threadParameter1, std::int32_t threadParameter2, NetDhcpStartParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetDhcpStop(int interfaceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigAddIfaddr6(int interfaceId, const NetIpv6Address* address, std::uint8_t prefixLength) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigDelIfaddr6(int interfaceId, const NetIpv6Address* address) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigDelDefaultRoute6(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigSetIfaddr6(int interfaceId, const NetIpv6Address* address, std::uint8_t prefixLength) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigSetIfaddr6WithFlags(int interfaceId, const NetIpv6Address* address, std::uint8_t prefixLength, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigSetDefaultRoute6(int interfaceId, const NetIpv6Address* gateway) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigGetIfaddr6(int interfaceId, NetIpv6InterfaceAddress* addresses, std::uint32_t count) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigGetDefaultRoute6(NetIpv6Address* gateway) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigDelRoute6(const NetIpv6Address* destination, std::uint8_t prefixLength) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigAddRoute6(const NetIpv6Address* destination, const NetIpv6Address* gateway, std::uint8_t prefixLength) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetAddrConfig6Start(std::uint32_t interfaceId, NetAddressCallback callback, void* userArg, std::int32_t threadParameter1, std::int32_t threadParameter2, const NetAddress6StartParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetAddrConfig6Stop(int interfaceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetAddrConfig6GetInfo(std::int32_t interfaceId, NetAddress6Info* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetInfoDumpStart(std::int32_t threadParameter1, std::int32_t threadParameter2, const NetInfoDumpParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetInfoDumpStop(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetDhcpdStart(std::uint32_t interfaceId, NetAddressCallback callback, void* userArg, std::int32_t threadParameter1, std::int32_t threadParameter2, const NetDhcpdStartParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetDhcpdStop(int interfaceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetDuplicateIpStart(std::int32_t interfaceId, NetAddressCallback callback, void* userArg, std::int32_t threadParameter1, std::int32_t threadParameter2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetDuplicateIpStop(int interfaceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigWlanAdhocClearWakeOnWlan(std::uint32_t interfaceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigWlanAdhocCreate(std::int32_t interfaceId, NetWlanAdhocParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigWlanAdhocGetWakeOnWlanInfo(std::int32_t interfaceId, NetWlanWakeInfo* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigWlanAdhocJoin(std::int32_t interfaceId, NetWlanAdhocParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigWlanAdhocLeave(int interfaceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigWlanAdhocPspEmuClearWakeOnWlan(std::uint32_t interfaceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigWlanAdhocPspEmuGetWakeOnWlanInfo(std::int32_t interfaceId, NetWlanPspWakeInfo* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigWlanAdhocPspEmuSetWakeOnWlan(std::int32_t interfaceId, NetWlanPspWakeParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigWlanAdhocScanJoin(std::int32_t interfaceId, const NetWlanAdhocScanParameter* parameter, void* buffer, std::size_t bufferSize, NetWlanJoinResult** result) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigWlanAdhocSetExtInfoElement(std::int32_t interfaceId, NetWlanInformationElement* parameter, std::int32_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigWlanAdhocSetWakeOnWlan(std::int32_t interfaceId, NetWlanWakeParameter* parameter, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigWlanApStart(std::int32_t interfaceId, NetWlanApParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigWlanApStop(int interfaceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigWlanBackgroundScanQuery(std::int32_t interfaceId, void* buffer, std::int32_t size, NetWlanScanResult** results, std::size_t* count) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigWlanBackgroundScanStart(std::int32_t interfaceId, NetWlanScanParameter* parameter, std::uint32_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigWlanBackgroundScanStop(int interfaceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigWlanDiagGetDeviceInfo(std::int32_t interfaceId, NetWlanDeviceInfo* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigWlanDiagSetAntenna(std::int32_t interfaceId, NetWlanAntennaParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigWlanDiagSetTxFixedRate(std::int32_t interfaceId, NetWlanRateParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigWlanGetDeviceConfig(std::int32_t interfaceId, NetWlanDeviceConfiguration* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigWlanInfraGetRssiInfo(std::int32_t interfaceId, NetWlanRssiInfo* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigWlanInfraLeave(int interfaceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigWlanInfraScanJoin(std::int32_t interfaceId, const NetWlanInfraScanParameter* parameter, void* buffer, std::size_t bufferSize, NetWlanJoinResult** result) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigWlanScan(std::int32_t interfaceId, const NetWlanScanParameter* parameter, std::size_t parameterSize, void* buffer, std::size_t bufferSize, NetWlanScanResult** results, std::size_t* count) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigWlanSetDeviceConfig(std::int32_t interfaceId, NetWlanDeviceConfiguration* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigEtherGetLinkMode(std::uint32_t interfaceId, int* mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigEtherPostPlugInOutEvent(std::uint32_t interfaceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigEtherSetLinkMode(std::uint32_t interfaceId, int mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetPppoeStart(std::uint32_t interfaceId, std::int32_t pppoeInterfaceId, NetAddressCallback callback, void* userArg, std::int32_t threadParameter1, std::int32_t threadParameter2, const NetPppoeStartParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetPppoeStop(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetBandwidthControlSetDefaultParam(const NetBandwidthParameter* parameter, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetBandwidthControlGetDefaultParam(NetBandwidthParameter* parameter, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetBandwidthControlGetDataTraffic(std::int32_t interfaceId, NetDataTraffic* traffic, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigSetDefaultScope(std::uint32_t interfaceId, const char* interfaceName, std::int32_t scopeId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigRoutingStart(std::uint32_t firstInterfaceId, std::uint32_t secondInterfaceId, const NetRoutingAddressParameter* addressParameter, NetRoutingServerParameter* serverParameter, const NetRoutingRuleParameter* ruleParameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigRoutingStop(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigRoutingShowRoutingConfig(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetConfigRoutingShowtCtlVar(const char* name) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetGetStatisticsInfoInternal(NetStatisticsInfoInternal* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetDumpAbort(std::int32_t dumpId, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetDumpCreate(const char* name, std::int32_t bufferSize, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetDumpSetFilterParam(std::int32_t socket, const NetDumpFilterParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetDumpDestroy(std::int32_t dumpId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetDumpRead(std::int32_t dumpId, void* buffer, std::int32_t bufferSize, std::uint32_t* flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetEmulationDebugSettingsSet(const NetEmulationParameter* parameter, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetEmulationDebugSettingsGet(NetEmulationParameter* parameter, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetEmulationSet(const NetEmulationParameter* parameter, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetEmulationGet(NetEmulationParameter* parameter, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
