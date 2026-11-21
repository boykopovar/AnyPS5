extern "C" {
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceNet/include/Net.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#ifdef __APPLE__
#include <netinet/tcp_fsm.h>
#endif
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <poll.h>
#include <unistd.h>
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
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
#include <optional>
#include <random>
#include <vector>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include "prx/libkernel/Socket/include/SocketError.hpp"
#include "prx/libkernel/Socket/include/SocketPoll.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"
#include "prx/libkernel/Socket/include/SocketOptions.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/Pthread/include/ThreadLifecycle.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "src/ResolverTestHooks.hpp"


static_assert(sizeof(NetDhcpOptionInfo) == 24);
static_assert(offsetof(NetDhcpOptionInfo, capacity) == 4);
static_assert(offsetof(NetDhcpOptionInfo, data) == 8);
static_assert(offsetof(NetDhcpOptionInfo, size) == 16);

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

static_assert(sizeof(NetInfoDumpParameter) == 72);
static_assert(offsetof(NetInfoDumpParameter, path) == 8);

static_assert(sizeof(NetResolverConnectOption) == 16);
static_assert(offsetof(NetResolverConnectOption, resolverTimeout) == 4);
static_assert(offsetof(NetResolverConnectOption, resolverRetryCount) == 8);
static_assert(offsetof(NetResolverConnectOption, reserved) == 12);

static_assert(sizeof(NetSyncObject) == 24);

static_assert(sizeof(NetIpv6Address) == 16);

static_assert(sizeof(NetDnsInfo) == 8);
static_assert(sizeof(NetDns6Info) == 32);

static_assert(sizeof(NetIpv6InterfaceAddress) == 20);

namespace {
constexpr int NET_ENOENT = 2;
constexpr int NET_EINTR = 4;
constexpr int NET_EBADF = 9;
constexpr int NET_EACCES = 13;
constexpr int NET_EFAULT = 14;
constexpr int NET_EBUSY = 16;
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
constexpr int NET_ENODATA = 0xA4;
constexpr int NET_ERROR_BASE = static_cast<int>(0x80410100u);
constexpr int NET_ERROR_RESOLVER_ENODNS = static_cast<int>(0x804101E1u);
constexpr int NET_CONFIG_ERROR_INVALID_ADDRESS = static_cast<int>(0x8041130Eu);
constexpr int NET_CONFIG_ERROR_NO_ROUTE = static_cast<int>(0x8041130Cu);
constexpr int NET_ENAMETOOLONG = 63;
constexpr int NET_ERROR_DAEMON_INVALID = static_cast<int>(0x80411506u);
constexpr int NET_ERROR_DAEMON_NOT_STARTED = static_cast<int>(0x8041150Au);
constexpr int NET_ERROR_DUPLICATE_IP_INVALID = static_cast<int>(0x80411A01u);
constexpr int NET_ERROR_DUPLICATE_IP_NOT_STARTED = static_cast<int>(0x80411A03u);
constexpr int NET_ERROR_PPPOE_NOT_STARTED = static_cast<int>(0x80411406u);
constexpr int NET_KERNEL_ERROR_TIMEDOUT = static_cast<int>(0x8002003Cu);
constexpr std::uint32_t NET_ANY_INTERFACE = 0xFFFFFFFFu;
constexpr std::int32_t NET_SHOW_BUFFER_SIZE = 0x2000;
constexpr std::size_t NET_RESOLVER_CONNECTION_FLAGS = 0x1D8;
constexpr int NET_RESOLVER_ABORT_FLAG_NTOA_PRESERVATION = 0x1;
constexpr int NET_RESOLVER_ABORT_FLAG_ATON_PRESERVATION = 0x2;
constexpr std::uint32_t NET_IFF_UP = 0x1;
constexpr std::uint32_t NET_IFF_BROADCAST = 0x2;
constexpr std::uint32_t NET_IFF_LOOPBACK = 0x8;
constexpr std::uint32_t NET_IFF_RUNNING = 0x40;
constexpr std::uint32_t NET_IFF_MULTICAST = 0x8000;
constexpr std::uint32_t NET_IF_LO0 = 0;
constexpr std::array<const char*, 10> NET_INTERFACE_NAMES = {
    "lo0", "eth0", "eth1", "dbg0", "wlan0", "wlan1", "gbe0", "bt0", "phone0", "pppoe0"};

constexpr int NET_AF_INET = 2;
constexpr int NET_AF_INET6 = 28;
constexpr int NET_SOCK_STREAM = 1;
constexpr int NET_SOCK_DGRAM = 2;
constexpr int NET_SOCK_RAW = 3;
constexpr int NET_EPOLL_CTL_ADD = 1;
constexpr int NET_EPOLL_CTL_MOD = 2;
constexpr int NET_EPOLL_CTL_DEL = 3;
constexpr std::uint32_t NET_EPOLLIN = 0x1;
constexpr std::uint32_t NET_EPOLLHUP = 0x10;
constexpr std::uint32_t NET_EPOLLDESCID = 0x10000;
constexpr int NET_RESOLVER_ASYNC = 0x1;
constexpr int NET_SOL_SOCKET = 0xFFFF;
constexpr int NET_IPPROTO_TCP = 6;
constexpr int NET_IPPROTO_IPV6 = 41;
constexpr int NET_TCP_NODELAY = 1;
constexpr int NET_TCP_MAXSEG = 2;
constexpr int NET_TCP_KEEPIDLE = 0x100;
constexpr int NET_TCP_KEEPINTVL = 0x200;
constexpr int NET_TCP_KEEPCNT = 0x400;
constexpr int NET_TF_NODELAY = 4;
constexpr int NET_TCP_MINIMUM_SEGMENT = 216;
constexpr int NET_TCP_DEFAULT_MAXSEG = 536;
constexpr int NET_TCP_DEFAULT_MAXSEG6 = 1220;
constexpr std::uint32_t NET_TCP_TICKS_PER_SECOND = 1000;
constexpr std::uint32_t NET_TCP_DEFAULT_KEEPIDLE = 7200;
constexpr std::uint32_t NET_TCP_DEFAULT_KEEPINTVL = 75;
constexpr std::uint32_t NET_TCP_DEFAULT_KEEPCNT = 8;
constexpr int NET_SO_SNDTIMEO = 0x1005;
constexpr int NET_SO_RCVTIMEO = 0x1006;
constexpr int NET_SO_ERROR = 0x1007;
constexpr int NET_SO_PROTOCOL = 0x1016;
constexpr int NET_SO_NBIO = 0x1200;
constexpr int NET_SO_REUSEADDR = 0x4;
constexpr int NET_SO_REUSEPORT = 0x200;
constexpr int NET_SO_KEEPALIVE = 0x8;
constexpr int NET_SO_DONTROUTE = 0x10;
constexpr int NET_SO_BROADCAST = 0x20;
constexpr int NET_SO_LINGER = 0x80;
constexpr int NET_SO_ONESBCAST = 0x10000;
constexpr int NET_SO_OOBINLINE = 0x100;
constexpr int NET_SO_SNDBUF = 0x1001;
constexpr int NET_SO_RCVBUF = 0x1002;
constexpr int NET_SO_SCE_SNDTIMEO = 0x1105;
constexpr int NET_SO_SCE_RCVTIMEO = 0x1106;
constexpr int NET_SO_SCE_CONNECTTIMEO = 0x1109;
constexpr int NET_SO_TYPE = 0x1008;
constexpr int NET_SO_ACCEPTCONN = 0x2;
constexpr int NET_MSG_PEEK = 0x2;
constexpr int NET_MSG_TRUNC = 0x10;
constexpr int NET_MSG_DONTWAIT = 0x80;
constexpr int NET_MSG_NOSIGNAL = 0x20000;
constexpr int NET_UIO_MAXIOV = 1024;

bool RawProtocolEntry(int family, int protocol) {
    if (family == NET_AF_INET6) {
        switch (protocol) {
        case 4: case 41: case 43: case 44: case 47: case 58: case 60: case 103: case 255: return true;
        default: return false;
        }
    }
    switch (protocol) {
    case 1: case 2: case 4: case 41: case 46: case 47: case 55: case 97: case 103: case 255: return true;
    default: return false;
    }
}

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
    std::atomic<int> connectTimeout{0};
    std::atomic<int> connectExpired{0};
    NativeSocketHandle(NativeSocket socket, bool stream) : value(socket), stream(stream) {
#ifdef _WIN32
        transfer = &TransferSocketForKernel;
#endif
    }
    ~NativeSocketHandle() {
        if (value != INVALID_NATIVE_SOCKET) close_socket(value);
    }
};

int ReadGuestAddress(const void* address, std::uint32_t length,
                     std::array<std::uint8_t, sizeof(sockaddr_storage)>& snapshot) {
    if (!address || length < 2) return NET_EINVAL;
    if (!GuestSocketReadGuest_nid_no_patch(snapshot.data(), address, std::min<std::size_t>(length, snapshot.size())))
        return NET_EFAULT;
    return 0;
}

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
    return true;
}

struct Sock {
    int family = 0;
    int type = 0;
    int protocol = 0;
    std::shared_ptr<NativeSocketHandle> native;
    bool bound = false;
    bool listening = false;
    bool connectAttempted = false;
#ifdef _WIN32
    bool connectPending = false;
    bool connectRefused = false;
#endif
    bool aborted = false;
    bool unboundReceiveStopped = false;
    bool oobInline = false;
    int reported_error = 0;
    int snd_buffer = 0;
    int rcv_buffer = 0;
    int onesbcast = 0;
    int reuseFlags = 0;
    int keepalive = 0;
    int tcpMaximumSegment = NET_TCP_DEFAULT_MAXSEG;
    std::uint32_t tcpKeepIdle = 0;
    std::uint32_t tcpKeepInterval = 0;
    std::uint32_t tcpKeepCount = 0;
    std::string name;
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
std::map<int, std::string> g_epoll_names;
using EpollRegistrations = std::map<int, std::shared_ptr<const NetEpollEvent>>;
std::map<int, EpollRegistrations> g_epoll_socks;
std::set<int> g_epoll_aborted;
std::map<int, std::size_t> g_pools;

struct Resolver {
    int error = 0;
    int running = 0;
    std::uint64_t aborts = 0;
    bool abort_ntoa = false;
    bool abort_aton = false;
    std::uint64_t completions = 0;
    bool hangup = false;
};
std::map<int, Resolver> g_resolvers;
std::map<int, std::map<int, std::uint64_t>> g_epoll_resolvers_reported;
int g_next_sock = 32;
int g_next_epoll = 0x4000;
int g_next_pool = 1;
constexpr int NET_RESOLVER_ID_FIRST = 1024;
constexpr int NET_RESOLVER_ID_END = 0x4000;
bool g_net_inited = false;
bool g_net_init_param_set = false;
std::array<std::uint8_t, 32> g_net_init_param{};
std::atomic<GuestSceNetTest::ResolverLookup> g_resolverLookupForTests{nullptr};
std::atomic<GuestSceNetTest::ResolverReverseLookup> g_resolverReverseLookupForTests{nullptr};

int* errno_slot() {
    static thread_local int e = 0;
    return &e;
}

extern "C" std::size_t NetCtlHostDnsServers_nid_no_patch(std::uint32_t* servers, std::size_t capacity);
extern "C" bool NetCtlHostIpv4Address_nid_no_patch(std::uint32_t* address, std::uint32_t* netmask, std::uint32_t* mtu);
extern "C" bool NetCtlHostIpv4Gateway_nid_no_patch(std::uint32_t* gateway);

std::mutex g_dnsLock;
bool g_dnsOverridden = false;
NetDnsInfo g_dnsOverride{};
NetDns6Info g_dns6Override{};

bool IsZeroAddress(const NetIpv6Address& address) {
    return std::all_of(std::begin(address.bytes), std::end(address.bytes), [](std::uint8_t byte) { return byte == 0; });
}

void set_errno(int err) {
}

int fail(int err) {
    set_errno(err);
    return NET_ERROR_BASE | err;
}

int fail_with_status(int result) {
    set_errno((result & 0xFF00) == 0x100 ? result & 0xFF : 205);
    return result;
}

bool net_initialized() {
    std::lock_guard lk(g_mutex);
    return g_net_inited;
}

std::array<NetSyncObject, 5> g_net_syncs{};
bool g_net_syncs_created = false;

int create_sync(NetSyncObject* sync, std::uint32_t signals, const char* name) {
    sync->mutex = nullptr;
    int result = scePthreadMutexInit(&sync->mutex, nullptr, name);
    if (result < 0) return result;
    sync->condition = nullptr;
    result = scePthreadCondInit(&sync->condition, nullptr, name);
    if (result < 0) {
        scePthreadMutexDestroy(&sync->mutex);
        return result;
    }
    sync->signals = signals;
    return 0;
}

void destroy_sync(NetSyncObject* sync) {
    scePthreadCondDestroy(&sync->condition);
    scePthreadMutexDestroy(&sync->mutex);
}

int create_net_syncs() {
    static constexpr std::array<const char*, 5> names = {"SceNetDhcpSync", "SceNetDhcpdSync", "SceNetDupIpDetSync", "SceNetAddrconfig6Sync", "SceNetInfoSync"};
    if (g_net_syncs_created) return 0;
    for (std::size_t created = 0; created < names.size(); ++created) {
        const int result = create_sync(&g_net_syncs[created], 1, names[created]);
        if (result >= 0) continue;
        while (created-- > 0) destroy_sync(&g_net_syncs[created]);
        return result;
    }
    g_net_syncs_created = true;
    return 0;
}

std::optional<std::uint32_t> HostInterfaceByRole(bool loopback) {
#ifdef _WIN32
    const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG size = 0;
    if (GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, nullptr, &size) != ERROR_BUFFER_OVERFLOW) return std::nullopt;
    std::vector<std::uint8_t> storage(size);
    auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data());
    if (GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, adapters, &size) != NO_ERROR) return std::nullopt;
    std::optional<std::uint32_t> primary;
    for (auto* adapter = adapters; adapter != nullptr; adapter = adapter->Next) {
        if (adapter->IfIndex == 0) continue;
        if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
            if (loopback) return static_cast<std::uint32_t>(adapter->IfIndex);
            continue;
        }
        if (!primary && adapter->OperStatus == IfOperStatusUp && adapter->FirstUnicastAddress != nullptr)
            primary = static_cast<std::uint32_t>(adapter->IfIndex);
    }
    return loopback ? std::nullopt : primary;
#else
    ifaddrs* list = nullptr;
    if (getifaddrs(&list) != 0) return std::nullopt;
    std::uint32_t loop = 0;
    std::optional<std::uint32_t> primary;
    for (auto* item = list; item != nullptr; item = item->ifa_next) {
        if (item->ifa_name == nullptr || !(item->ifa_flags & IFF_UP)) continue;
        const unsigned int index = if_nametoindex(item->ifa_name);
        if (index == 0) continue;
        if ((item->ifa_flags & IFF_LOOPBACK) != 0) {
            if (loop == 0) loop = index;
            continue;
        }
        if (!primary && item->ifa_addr != nullptr &&
            (item->ifa_addr->sa_family == AF_INET || item->ifa_addr->sa_family == AF_INET6))
            primary = static_cast<std::uint32_t>(index);
    }
    freeifaddrs(list);
    if (loopback) return loop == 0 ? std::nullopt : std::optional<std::uint32_t>(loop);
    return primary;
#endif
}

std::optional<std::uint32_t> HostInterfaceIndex(std::uint32_t interfaceId) {
    if (interfaceId >= NET_INTERFACE_NAMES.size()) return std::nullopt;
    if (interfaceId == 0) return HostInterfaceByRole(true);
    if (interfaceId == 1) return HostInterfaceByRole(false);
    const unsigned int index = if_nametoindex(NET_INTERFACE_NAMES[interfaceId]);
    return index == 0 ? std::nullopt : std::optional<std::uint32_t>(index);
}

void* APS5_VABI RunNetThread(void* param) {
    const auto* thread = static_cast<const NetThreadParameter*>(param);
    thread->entry(thread->arg);
    return nullptr;
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

int complete_async_lookup(int rid, int error) {
    std::lock_guard lk(g_mutex);
    const auto resolver = g_resolvers.find(rid);
    if (resolver == g_resolvers.end()) return error;
    resolver->second.error = error;
    resolver->second.hangup = error == (NET_ERROR_BASE | NET_EINTR);
    ++resolver->second.completions;
    g_cv.NotifyAll();
    return 0;
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

static_assert(sizeof(NetMsghdr) == 48 && offsetof(NetMsghdr, iov) == 16 && offsetof(NetMsghdr, control) == 32 &&
    offsetof(NetMsghdr, flags) == 44);

int timeout_option(int optname) {
    if (optname == NET_SO_SCE_SNDTIMEO) return NET_SO_SNDTIMEO;
    if (optname == NET_SO_SCE_RCVTIMEO) return NET_SO_RCVTIMEO;
    return optname;
}

#ifndef _WIN32
constexpr int CONNECT_EXPIRED = -1;

#ifdef __APPLE__
bool GuestHoldsUnlistenedLoopback(const sockaddr_storage& target) {
    const bool v4 = target.ss_family == AF_INET;
    if (!v4 && target.ss_family != AF_INET6) return false;
    if (v4) {
        if ((ntohl(reinterpret_cast<const sockaddr_in*>(&target)->sin_addr.s_addr) >> 24) != 127) return false;
    } else if (!IN6_IS_ADDR_LOOPBACK(&reinterpret_cast<const sockaddr_in6*>(&target)->sin6_addr)) {
        return false;
    }
    const std::uint16_t port = v4 ? reinterpret_cast<const sockaddr_in*>(&target)->sin_port
                                  : reinterpret_cast<const sockaddr_in6*>(&target)->sin6_port;
    if (port == 0) return false;
    std::lock_guard lk(g_mutex);
    for (const auto& entry : g_socks) {
        const Sock& candidate = entry.second;
        if (!candidate.bound || candidate.listening || candidate.type != NET_SOCK_STREAM) continue;
        if (candidate.family != (v4 ? NET_AF_INET : NET_AF_INET6)) continue;
        sockaddr_storage local{};
        NativeLength length = sizeof(local);
        if (::getsockname(candidate.native->value, reinterpret_cast<sockaddr*>(&local), &length) != 0) continue;
        std::uint16_t boundPort = 0;
        bool wildcard = false;
        if (v4) {
            boundPort = reinterpret_cast<const sockaddr_in*>(&local)->sin_port;
            wildcard = reinterpret_cast<const sockaddr_in*>(&local)->sin_addr.s_addr == htonl(INADDR_ANY);
        } else {
            boundPort = reinterpret_cast<const sockaddr_in6*>(&local)->sin6_port;
            wildcard = IN6_IS_ADDR_UNSPECIFIED(&reinterpret_cast<const sockaddr_in6*>(&local)->sin6_addr);
        }
        if (boundPort != port) continue;
        if (wildcard) return true;
        if (v4) {
            if ((ntohl(reinterpret_cast<const sockaddr_in*>(&local)->sin_addr.s_addr) >> 24) == 127) return true;
        } else if (IN6_IS_ADDR_LOOPBACK(&reinterpret_cast<const sockaddr_in6*>(&local)->sin6_addr)) {
            return true;
        }
    }
    return false;
}
#endif

int connect_within(NativeSocket native, const sockaddr* address, NativeLength length, int microseconds) {
    const int flags = fcntl(native, F_GETFL, 0);
    if (flags < 0 || fcntl(native, F_SETFL, flags | O_NONBLOCK) != 0) return native_error();
    int error = ::connect(native, address, length) == 0 ? 0 : errno;
    if (error == EINPROGRESS) {
        pollfd descriptor{native, POLLOUT, 0};
        const int ready = ::poll(&descriptor, 1, static_cast<int>((static_cast<std::int64_t>(microseconds) + 999) / 1000));
        NativeLength size = sizeof(error);
        if (ready < 0 || (ready > 0 && ::getsockopt(native, SOL_SOCKET, SO_ERROR, &error, &size) != 0)) error = errno;
        else if (ready == 0) error = CONNECT_EXPIRED;
    }
    fcntl(native, F_SETFL, flags);
    return error == 0 || error == CONNECT_EXPIRED ? error : net_error(error);
}
#endif

#ifdef _WIN32
short closed_stream_events(const Sock& socket, short native, short requested) {
    if (socket.type != NET_SOCK_STREAM || (native & (POLLHUP | POLLERR)) != POLLHUP) return native;
    native = static_cast<short>(native | (requested & POLLRDNORM));
    return (socket.native->shutdownFlags.load() & KernelSocketPoll::SendShut) ? native : static_cast<short>(native & ~POLLHUP);
}
#elif defined(__APPLE__)
short closed_stream_events(const Sock& socket, short native, short requested) {
    if (socket.type != NET_SOCK_STREAM || (native & (POLLHUP | POLLERR)) != POLLHUP || (socket.native->shutdownFlags.load() & KernelSocketPoll::SendShut)) return native;
    return static_cast<short>((native & ~POLLHUP) | (requested & POLLOUT));
}
#endif

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

#ifdef __APPLE__
template <typename TSend>
std::int64_t send_stream_in_chunks(const NativeSocketHandle& socket, int flags, const char* data, std::size_t length, TSend sendChunk) {
    if (!socket.stream || (flags & NET_MSG_DONTWAIT) != 0 || socket.nonblock.load()) return sendChunk(data, length);
    if (length == 0) return sendChunk(data, 0);
    constexpr std::size_t Chunk = 65536;
    std::size_t transferred = 0;
    do {
        const auto request = std::min(Chunk, length - transferred);
        const auto result = sendChunk(data + transferred, request);
        if (result < 0) return transferred > 0 ? static_cast<std::int64_t>(transferred) : -1;
        transferred += static_cast<std::size_t>(result);
        if (static_cast<std::size_t>(result) < request) break;
    } while (transferred < length);
    return static_cast<std::int64_t>(transferred);
}
#endif

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
#ifdef __APPLE__
    constexpr int XnuMessageNonblocking = 0x20000;
    if (flags & NET_MSG_DONTWAIT) native |= XnuMessageNonblocking;
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
    if (socket.type == NET_SOCK_STREAM) {
#ifdef _WIN32
        if (!empty) return 0;
        sockaddr_storage peer{};
        NativeLength length = sizeof(peer);
        if (::getpeername(socket.native->value, reinterpret_cast<sockaddr*>(&peer), &length) != 0) {
            const int error = native_error();
            const bool readShut = (socket.native->shutdownFlags.load() & KernelSocketPoll::ReceiveShut) != 0;
            return !socket.connectAttempted && !socket.aborted && !readShut && error == NET_ENOTCONN ? error : 0;
        }
        WSAPOLLFD state{};
        state.fd = socket.native->value;
        state.events = POLLRDNORM;
        return WSAPoll(&state, 1, 0) >= 0 && (state.revents & (POLLERR | POLLHUP | POLLNVAL)) == 0 ? RECEIVE_NOTHING : 0;
#else
        return 0;
#endif
    }
    if (socket.bound) return 0;
    const bool named = has_local_address(socket.native->value);
    if (!named && (socket.unboundReceiveStopped || (socket.native->shutdownFlags.load() & KernelSocketPoll::ReceiveShut) || empty)) return RECEIVE_NOTHING;
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
        if (it->second.unboundReceiveStopped || (it->second.native->shutdownFlags.load() & KernelSocketPoll::ReceiveShut)) return RECEIVE_NOTHING;
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

std::optional<std::uint32_t> InterfaceIdOfName(std::string_view name) {
    for (std::uint32_t interfaceId = 0; interfaceId < NET_INTERFACE_NAMES.size(); ++interfaceId) {
        if (name == NET_INTERFACE_NAMES[interfaceId]) return interfaceId;
    }
    return std::nullopt;
}

std::optional<std::uint32_t> InterfaceIdOfHostIndex(std::uint32_t index) {
    for (std::uint32_t interfaceId = 0; interfaceId < NET_INTERFACE_NAMES.size(); ++interfaceId) {
        if (HostInterfaceIndex(interfaceId) == index) return interfaceId;
    }
    return std::nullopt;
}

}

void GuestSceNetTest::SetResolverLookupForTests_nid_no_patch(ResolverLookup lookup) noexcept {
    g_resolverLookupForTests.store(lookup);
}

void GuestSceNetTest::SetResolverReverseLookupForTests_nid_no_patch(ResolverReverseLookup lookup) noexcept {
    g_resolverReverseLookupForTests.store(lookup);
}


extern const std::uint32_t sce_net_in6addr_any[4] = {};

alignas(std::uint32_t) extern const std::uint8_t sce_net_in6addr_loopback_nid_postfix[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
alignas(std::uint32_t) extern const std::uint8_t sce_net_in6addr_nodelocal_allnodes_nid_postfix[16] = {0xff, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
alignas(std::uint32_t) extern const std::uint8_t sce_net_in6addr_linklocal_allrouters_nid_postfix[16] = {0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2};
extern const std::uint32_t sce_net_dummy_nid_postfix = 0;

alignas(std::uint32_t) extern const std::uint8_t sce_net_in6addr_linklocal_allnodes[16] = {0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};

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
    return NET_ERROR_PPPOE_NOT_STARTED;
}

int APS5_VABI sceNetBandwidthControlSetDefaultParam(const NetBandwidthParameter* parameter, std::uint32_t flags) {
    if (!net_initialized()) return fail(NET_ENOTINIT);
    if (parameter == nullptr || flags != 0) return fail(NET_EINVAL);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNetBandwidthControlGetDefaultParam(NetBandwidthParameter* parameter, std::uint32_t flags) {
    if (!net_initialized()) return fail(NET_ENOTINIT);
    if (parameter == nullptr || flags != 0) return fail(NET_EINVAL);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNetBandwidthControlGetDataTraffic(std::int32_t interfaceId, NetDataTraffic* traffic, std::uint32_t flags) {
    if (!net_initialized()) return fail(NET_ENOTINIT);
    if (interfaceId != -1 || traffic == nullptr || flags != 0) return fail(NET_EINVAL);
    NotImplemented_nid_no_patch(__func__);
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
    if (!net_initialized()) return fail(NET_ENOTINIT);
    if (info == nullptr) return fail(NET_EINVAL);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNetDumpAbort(std::int32_t dumpId, std::uint32_t flags) {
    if (!net_initialized()) return fail(NET_ENOTINIT);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNetDumpCreate(const char* name, std::int32_t bufferSize, std::uint32_t flags) {
    if (!net_initialized()) return fail(NET_ENOTINIT);
    if (name == nullptr) return fail(NET_EINVAL);
    std::string guestName;
    if (const int error = CopyGuestString_nid_no_patch(name, 32, guestName)) return fail(error);
    if (bufferSize < 0x800 || flags > 4 || flags == 3) return fail(NET_EINVAL);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNetDumpSetFilterParam(std::int32_t socket, const NetDumpFilterParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetDumpDestroy(std::int32_t dumpId) {
    if (!net_initialized()) return fail(NET_ENOTINIT);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNetDumpRead(std::int32_t dumpId, void* buffer, std::int32_t bufferSize, std::uint32_t* flags) {
    if (!net_initialized()) return fail(NET_ENOTINIT);
    if (buffer == nullptr || bufferSize < 0) return fail(NET_EINVAL);
    std::uint32_t requested = 0;
    if (flags != nullptr && !GuestCopyIn_nid_no_patch(&requested, flags, sizeof(requested))) return fail(NET_EFAULT);
    if ((requested & ~0x20u) != 0) return fail(NET_EINVAL);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNetEmulationDebugSettingsSet(const NetEmulationParameter* parameter, std::uint32_t flags) {
    (void)parameter;
    (void)flags;
    return fail(NET_EOPNOTSUPP);
}

int APS5_VABI sceNetEmulationDebugSettingsGet(NetEmulationParameter* parameter, std::uint32_t flags) {
    (void)parameter;
    (void)flags;
    return fail(NET_EOPNOTSUPP);
}

int APS5_VABI sceNetEmulationSet(const NetEmulationParameter*, std::uint32_t) {
    return fail(NET_EOPNOTSUPP);
}

int APS5_VABI sceNetEmulationGet(NetEmulationParameter*, std::uint32_t) {
    return fail(NET_EOPNOTSUPP);
}

}
