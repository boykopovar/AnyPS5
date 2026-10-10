#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <future>
#include <stdexcept>
#include <string>
#include <thread>

extern "C" {
extern const std::uint8_t in6addr_any_nid_postfix[16];
extern const std::uint8_t in6addr_loopback_nid_postfix[16];
int APS5_VABI sceNetInit_nid_postfix(void);
int APS5_VABI sceNetSocket(const char*, int, int, int);
int APS5_VABI sceNetBind_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI sceNetListen(int, int);
int APS5_VABI sceNetGetsockname(int, void*, std::uint32_t*);
int APS5_VABI sceNetConnect(int, const void*, std::uint32_t);
int APS5_VABI sceNetAccept(int, void*, std::uint32_t*);
std::int64_t APS5_VABI sceNetSend(int, const void*, std::size_t, int);
std::int64_t APS5_VABI sceNetRecv(int, void*, std::size_t, int);
std::int64_t APS5_VABI sceNetSendto(int, const void*, std::size_t, int, const void*, std::uint32_t);
std::int64_t APS5_VABI sceNetRecvfrom(int, void*, std::size_t, int, void*, std::uint32_t*);
int APS5_VABI sceNetSocketClose(int);
int APS5_VABI sceNetShutdown(int, int);
int APS5_VABI sceNetSetsockopt(int, int, int, const void*, std::uint32_t);
int* APS5_VABI sceNetErrnoLoc(void);
int APS5_VABI sceNetEpollCreate(const char*, int);
int APS5_VABI sceNetEpollControl(int, int, int, const NetEpollEvent*);
int APS5_VABI sceNetEpollWait(int, NetEpollEvent*, int, int);
int APS5_VABI sceNetEpollDestroy(int);
int APS5_VABI sceNetEpollAbort(int, int);
extern const std::uint32_t sce_net_in6addr_any[4];
int APS5_VABI sceNetResolverCreate(const char*, int, int);
int APS5_VABI sceNetResolverStartNtoa(int, const char*, void*, int, int, int);
int APS5_VABI sceNetResolverStartNtoaMultipleRecordsEx(int, const char*, void*, int, int, int);
int APS5_VABI sceNetResolverDestroy(int);
int APS5_VABI sceNetResolverGetError(int, int*);
int APS5_VABI sceNetCtlGetState(int*);
int APS5_VABI select_nid_postfix(int, void*, void*, void*, const void*);
int* APS5_VABI __error_nid_postfix();
int APS5_VABI sceNetInetPton(int, const char*, void*);
const char* APS5_VABI sceNetInetNtop(int, const void*, char*, std::uint32_t);
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

struct NetMemoryPoolStats {
    std::size_t pool_size;
    std::size_t max_inuse_size;
    std::size_t current_inuse_size;
};

extern "C" {
std::int64_t APS5_VABI sceNetSendmsg(int, const NetMsghdr*, int);
std::int64_t APS5_VABI sceNetRecvmsg(int, NetMsghdr*, int);
int APS5_VABI sceNetPoolCreate(const char*, int, int);
int APS5_VABI sceNetPoolDestroy(int);
int APS5_VABI sceNetGetMemoryPoolStats(int, NetMemoryPoolStats*);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;

constexpr int afInet = 2;
constexpr int afInet6 = 28;
constexpr int sockStream = 1;
constexpr int sockDgram = 2;
constexpr int ipprotoTcp = 6;
constexpr int ipprotoUdp = 17;
constexpr int errBadf = 9;
constexpr int errFault = 14;
constexpr int errInval = 22;
constexpr int errNospc = 28;
constexpr int errPipe = 32;
constexpr int errWouldBlock = 35;
constexpr int errInProgress = 36;
constexpr int errMsgSize = 40;
constexpr int errOpNotSupp = 45;
constexpr int errAfNoSupport = 47;
constexpr int errConnAborted = 53;
constexpr int errIsConn = 56;
constexpr int resolverHostNotFound = static_cast<int>(0x804101E1);
constexpr int resolverInvalid = static_cast<int>(0x80410116);
constexpr int resolverBadDescriptor = static_cast<int>(0x80410109);
constexpr std::chrono::seconds waiterDeadline{5};

using Address = std::array<std::uint8_t, 16>;

int NetError(int error) {
    return static_cast<int>(0x80410100u | static_cast<unsigned>(error));
}

void RequireNetFailure(std::int64_t result, int error, const std::string& context) {
    RequireEqual(result, static_cast<std::int64_t>(NetError(error)), context + ": result");
    RequireEqual(*sceNetErrnoLoc(), error, context + ": errno");
}

void InitializeNet() {
    RequireEqual(sceNetInit_nid_postfix(), 0, "sceNetInit");
}

Address LoopbackAddress() {
    return Address{16, 2, 0, 0, 127, 0, 0, 1};
}

class Socket {
public:
    Socket(const char* name, int family, int type, int protocol) : id(sceNetSocket(name, family, type, protocol)) {}

    ~Socket() {
        if (id >= 0) sceNetSocketClose(id);
    }

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    int Close() {
        const int closing = id;
        id = -1;
        return sceNetSocketClose(closing);
    }

    int id;
};

class Epoll {
public:
    explicit Epoll(const char* name) : id(sceNetEpollCreate(name, 0)) {
        Require(id >= 0, std::string("create epoll ") + name);
    }

    ~Epoll() {
        if (id >= 0) sceNetEpollDestroy(id);
    }

    Epoll(const Epoll&) = delete;
    Epoll& operator=(const Epoll&) = delete;

    int Destroy() {
        const int destroying = id;
        id = -1;
        return sceNetEpollDestroy(destroying);
    }

    int id;
};

class Resolver {
public:
    Resolver() : id(sceNetResolverCreate("guest-sce-net", 0, 0)) {
        Require(id >= 0, "create the resolver");
    }

    ~Resolver() {
        if (id >= 0) sceNetResolverDestroy(id);
    }

    Resolver(const Resolver&) = delete;
    Resolver& operator=(const Resolver&) = delete;

    int Destroy() {
        destroyedId = id;
        id = -1;
        return sceNetResolverDestroy(destroyedId);
    }

    int id;
    int destroyedId = -1;
};

class TcpConnection {
public:
    TcpConnection() {
        InitializeNet();
        Require(listener.id >= 0, "create the listener");
        address = LoopbackAddress();
        RequireEqual(sceNetBind_nid_postfix(listener.id, address.data(), address.size()), 0, "bind the listener");
        RequireEqual(sceNetListen(listener.id, 4), 0, "listen");
        std::uint32_t addressSize = address.size();
        RequireEqual(sceNetGetsockname(listener.id, address.data(), &addressSize), 0, "listener address");
        RequireEqual(addressSize, 16u, "listener address size");
        Require(address[2] != 0 || address[3] != 0, "the listener has no port");
        Require(client.id >= 0, "create the client");
        RequireEqual(sceNetConnect(client.id, address.data(), address.size()), 0, "connect");
        Address peer{};
        std::uint32_t peerSize = peer.size();
        accepted.id = sceNetAccept(listener.id, peer.data(), &peerSize);
        Require(accepted.id >= 0, "accept");
        RequireEqual(peerSize, 16u, "peer address size");
    }

    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;

    Socket listener{nullptr, afInet, sockStream, ipprotoTcp};
    Socket client{nullptr, afInet, sockStream, ipprotoTcp};
    Socket accepted{nullptr, afInet, sockStream, ipprotoTcp};
    Address address{};
};

class UdpPair {
public:
    UdpPair() {
        InitializeNet();
        Require(receiver.id >= 0, "create the receiver");
        Require(sender.id >= 0, "create the sender");
        address = LoopbackAddress();
        RequireEqual(sceNetBind_nid_postfix(receiver.id, address.data(), address.size()), 0, "bind the receiver");
        std::uint32_t addressSize = address.size();
        RequireEqual(sceNetGetsockname(receiver.id, address.data(), &addressSize), 0, "receiver address");
    }

    UdpPair(const UdpPair&) = delete;
    UdpPair& operator=(const UdpPair&) = delete;

    Socket receiver{nullptr, afInet, sockDgram, ipprotoUdp};
    Socket sender{nullptr, afInet, sockDgram, ipprotoUdp};
    Address address{};
};

struct ScatterGather {
    char head[9] = "scatter ";
    char tail[7] = "gather";
    NetIovec out[3] = {{head, 8}, {nullptr, 0}, {tail, 6}};
    Address destination{};
    NetMsghdr send{};

    explicit ScatterGather(const Address& address) : destination(address) {
        send = NetMsghdr{destination.data(), 16, out, 3, nullptr, 0, 0};
    }
};

struct ReceiveBuffers {
    char first[5]{};
    char second[20]{};
    NetIovec in[2] = {{first, sizeof(first)}, {second, sizeof(second)}};
    std::array<std::uint8_t, 28> source{};
    char control[16]{};
    NetMsghdr receive{source.data(), static_cast<std::uint32_t>(source.size()), in, 2, control, sizeof(control), -1};
};

void SetBit(std::uint64_t* set, int descriptor) {
    set[descriptor / 64] |= std::uint64_t{1} << (descriptor % 64);
}

bool HasBit(const std::uint64_t* set, int descriptor) {
    return ((set[descriptor / 64] >> (descriptor % 64)) & 1u) != 0;
}

void SendDatagram(const UdpPair& pair, const char* datagram, std::size_t size) {
    RequireEqual(sceNetSendto(pair.sender.id, datagram, size, 0, pair.address.data(), pair.address.size()), static_cast<std::int64_t>(size), "send the datagram");
}

const char tcpRequest[] = "guest tcp loopback";
const char udpDatagram[] = "guest udp loopback";

const Case in6Constants{"In6AddrConstants_Exported_AreAnyAndLoopback", [] {
    for (int index = 0; index < 16; ++index) {
        RequireEqual(in6addr_any_nid_postfix[index], std::uint8_t{0}, "in6addr_any byte " + std::to_string(index));
        RequireEqual(in6addr_loopback_nid_postfix[index], std::uint8_t{index == 15 ? 1 : 0}, "in6addr_loopback byte " + std::to_string(index));
    }
}};

const Case init{"Init_Called_Succeeds", [] {
    InitializeNet();
}};

const Case poolStats{"GetMemoryPoolStats_FreshPool_ReportsSizeAndNoUsage", [] {
    InitializeNet();
    const int pool = sceNetPoolCreate("stats", 0x4000, 0);
    Require(pool > 0, "create the pool");
    NetMemoryPoolStats stats{1, 1, 1};
    RequireEqual(sceNetGetMemoryPoolStats(pool, &stats), 0, "get pool stats");
    RequireEqual(stats.pool_size, std::size_t{0x4000}, "pool size");
    RequireEqual(stats.max_inuse_size, std::size_t{0}, "max in-use size");
    RequireEqual(stats.current_inuse_size, std::size_t{0}, "current in-use size");
    RequireNetFailure(sceNetGetMemoryPoolStats(pool, nullptr), errInval, "stats with a null output");
    RequireEqual(sceNetPoolDestroy(pool), 0, "destroy the pool");
    RequireNetFailure(sceNetGetMemoryPoolStats(pool, &stats), errBadf, "stats of a destroyed pool");
}};

const Case epollEmptyPoll{"EpollWait_EmptyEpollZeroTimeout_ReturnsNoEvents", [] {
    InitializeNet();
    const Epoll epoll("empty-epoll");
    NetEpollEvent event{};
    RequireEqual(sceNetEpollWait(epoll.id, &event, 1, 0), 0, "poll an empty epoll");
}};

const Case epollEmptyTimeout{"EpollWait_EmptyEpollOneMillisecond_WaitsForTheTimeout", [] {
    InitializeNet();
    const Epoll epoll("empty-epoll");
    NetEpollEvent event{};
    std::array<std::chrono::steady_clock::duration, 9> elapsed{};
    for (std::size_t index = 0; index < elapsed.size(); ++index) {
        const auto start = std::chrono::steady_clock::now();
        RequireEqual(sceNetEpollWait(epoll.id, &event, 1, 1000), 0, "wait " + std::to_string(index));
        elapsed[index] = std::chrono::steady_clock::now() - start;
        Require(elapsed[index] >= std::chrono::microseconds(1000), "wait " + std::to_string(index) + " returned before the timeout");
    }
#ifdef _WIN32
    if (std::getenv("APS5_NO_TIMER_RESOLUTION") == nullptr) {
        std::sort(elapsed.begin(), elapsed.end());
        const auto median = std::chrono::duration_cast<std::chrono::microseconds>(elapsed[elapsed.size() / 2]);
        Require(median < std::chrono::milliseconds(8), "median 1 ms wait took " + std::to_string(median.count()) + " us");
    }
#endif
}};

const Case epollAbort{"EpollWait_AfterAbort_FailsOnceWithConnAborted", [] {
    InitializeNet();
    Epoll epoll("empty-epoll");
    NetEpollEvent event{};
    RequireEqual(sceNetEpollAbort(epoll.id, 0), 0, "abort");
    RequireNetFailure(sceNetEpollWait(epoll.id, &event, 1, 1000), errConnAborted, "wait after abort");
    RequireEqual(sceNetEpollWait(epoll.id, &event, 1, 0), 0, "poll after the aborted wait");
    RequireEqual(epoll.Destroy(), 0, "destroy");
}};

const Case epollAbortWaiter{"EpollWait_BlockedWaiterAborted_FailsWithConnAborted", [] {
    InitializeNet();
    std::future<std::int64_t> waiter;
    const Epoll epoll("waiting-epoll");
    waiter = std::async(std::launch::async, [id = epoll.id] {
        NetEpollEvent ready{};
        return static_cast<std::int64_t>(sceNetEpollWait(id, &ready, 1, -1));
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    RequireEqual(sceNetEpollAbort(epoll.id, 0), 0, "abort");
    Require(waiter.wait_for(waiterDeadline) == std::future_status::ready, "the aborted waiter did not return");
    RequireEqual(waiter.get(), static_cast<std::int64_t>(NetError(errConnAborted)), "aborted wait result");
}};

const Case epollDestroyWaiter{"EpollWait_BlockedWaiterDestroyed_FailsWithBadDescriptor", [] {
    InitializeNet();
    std::future<std::int64_t> waiter;
    Epoll epoll("waiting-epoll");
    waiter = std::async(std::launch::async, [id = epoll.id] {
        NetEpollEvent ready{};
        return static_cast<std::int64_t>(sceNetEpollWait(id, &ready, 1, -1));
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    RequireEqual(epoll.Destroy(), 0, "destroy");
    Require(waiter.wait_for(waiterDeadline) == std::future_status::ready, "the waiter on a destroyed epoll did not return");
    RequireEqual(waiter.get(), static_cast<std::int64_t>(NetError(errBadf)), "destroyed wait result");
}};

struct AddressText {
    int family;
    const char* text;
};

constexpr AddressText addressTexts[] = {
    {afInet, "127.0.0.1"},
    {afInet, "255.255.255.255"},
    {afInet6, "::1"},
    {afInet6, "1234:5678:9abc:def0:1234:5678:9abc:def0"},
};

const Case ntopRoundTrip{"InetNtop_ParsedAddress_RoundTripsOnlyWithRoomForTerminator", [] {
    for (const auto& entry : addressTexts) {
        const std::string context = entry.text;
        std::array<std::uint8_t, 16> address{};
        RequireEqual(sceNetInetPton(entry.family, entry.text, address.data()), 1, context + ": pton");
        std::array<char, 64> output{};
        const auto length = static_cast<std::uint32_t>(std::strlen(entry.text));
        for (std::uint32_t size = 0; size <= length; ++size) {
            *sceNetErrnoLoc() = 123;
            Require(sceNetInetNtop(entry.family, address.data(), output.data(), size) == nullptr, context + ": ntop into " + std::to_string(size) + " bytes succeeded");
            RequireEqual(*sceNetErrnoLoc(), errNospc, context + ": errno for " + std::to_string(size) + " bytes");
        }
        output.fill('x');
        *sceNetErrnoLoc() = 123;
        Require(sceNetInetNtop(entry.family, address.data(), output.data(), length + 1) == output.data(), context + ": ntop with exact room");
        RequireEqual(std::string(output.data()), context, context + ": text");
        RequireEqual(output[length + 1], 'x', context + ": byte after the terminator");
        RequireEqual(*sceNetErrnoLoc(), 123, context + ": errno after success");
    }
}};

const Case ntopInvalid{"InetNtop_InvalidFamilyOrPointer_FailsWithErrno", [] {
    for (const auto& entry : addressTexts) {
        const std::string context = entry.text;
        std::array<std::uint8_t, 16> address{};
        RequireEqual(sceNetInetPton(entry.family, entry.text, address.data()), 1, context + ": pton");
        std::array<char, 64> output{};
        Require(sceNetInetNtop(99, address.data(), output.data(), output.size()) == nullptr, context + ": family 99 succeeded");
        RequireEqual(*sceNetErrnoLoc(), errAfNoSupport, context + ": errno for family 99");
        Require(sceNetInetNtop(entry.family, nullptr, output.data(), output.size()) == nullptr, context + ": null source succeeded");
        RequireEqual(*sceNetErrnoLoc(), errInval, context + ": errno for a null source");
        Require(sceNetInetNtop(entry.family, address.data(), nullptr, output.size()) == nullptr, context + ": null destination succeeded");
        RequireEqual(*sceNetErrnoLoc(), errInval, context + ": errno for a null destination");
    }
}};

const Case ntopLoopback{"InetNtop_Ipv6Loopback_FormatsShortForm", [] {
    std::array<std::uint8_t, 16> ipv6{};
    RequireEqual(sceNetInetPton(afInet6, "::1", ipv6.data()), 1, "pton");
    char text[64]{};
    Require(sceNetInetNtop(afInet6, ipv6.data(), text, sizeof(text)) == text, "ntop");
    RequireEqual(std::string(text), std::string("::1"), "text");
}};

const Case ipv6Unspecified{"In6AddrAny_UnspecifiedAddress_MatchesPtonAndFormatsShortForm", [] {
    std::array<std::uint8_t, 16> unspecified{};
    RequireEqual(sceNetInetPton(afInet6, "::", unspecified.data()), 1, "pton of ::");
    Require(std::memcmp(sce_net_in6addr_any, unspecified.data(), unspecified.size()) == 0, "sce_net_in6addr_any differs from ::");
    char text[4] = {'x', 'x', 'x', 'x'};
    Require(sceNetInetNtop(afInet6, sce_net_in6addr_any, text, 3) == text, "ntop of sce_net_in6addr_any");
    RequireEqual(std::string(text), std::string("::"), "text");
    RequireEqual(text[3], 'x', "byte after the terminator");
}};

const Case ipv6Wildcard{"Ipv6WildcardSocket_LoopbackDatagram_IsReceived", [] {
    InitializeNet();
    Socket receiver("ipv6-any", afInet6, sockDgram, ipprotoUdp);
    if (receiver.id < 0 && *sceNetErrnoLoc() == errAfNoSupport) Testing::Skip("the host has no IPv6");
    const Socket sender("ipv6-loopback", afInet6, sockDgram, ipprotoUdp);
    Require(receiver.id >= 0, "create the IPv6 receiver");
    Require(sender.id >= 0, "create the IPv6 sender");
    std::array<std::uint8_t, 16> unspecified{};
    std::array<std::uint8_t, 28> address{28, 28};
    std::memcpy(address.data() + 8, sce_net_in6addr_any, 16);
    RequireEqual(sceNetBind_nid_postfix(receiver.id, address.data(), address.size()), 0, "bind to the wildcard");
    std::uint32_t size = address.size();
    address.fill(0xa5);
    RequireEqual(sceNetGetsockname(receiver.id, address.data(), &size), 0, "receiver address");
    RequireEqual(size, static_cast<std::uint32_t>(address.size()), "receiver address size");
    RequireEqual(address[0], std::uint8_t{28}, "address length byte");
    RequireEqual(address[1], std::uint8_t{28}, "address family byte");
    Require(address[2] != 0 || address[3] != 0, "the receiver has no port");
    Require(std::memcmp(address.data() + 8, unspecified.data(), unspecified.size()) == 0, "the receiver is not bound to the wildcard");
    RequireEqual(sceNetInetPton(afInet6, "::1", address.data() + 8), 1, "pton of ::1");
    const char payload[] = "IPv6 wildcard receive";
    RequireEqual(sceNetSendto(sender.id, payload, sizeof(payload), 0, address.data(), address.size()), static_cast<std::int64_t>(sizeof(payload)), "send to ::1");
    char received[sizeof(payload)]{};
    std::array<std::uint8_t, 28> peer{};
    size = peer.size();
    RequireEqual(sceNetRecvfrom(receiver.id, received, sizeof(received), 0, peer.data(), &size), static_cast<std::int64_t>(sizeof(received)), "receive");
    Require(std::memcmp(received, payload, sizeof(payload)) == 0, "received payload differs");
    RequireEqual(size, static_cast<std::uint32_t>(peer.size()), "peer address size");
    RequireEqual(peer[1], std::uint8_t{28}, "peer family");
    Require(std::memcmp(peer.data() + 8, address.data() + 8, 16) == 0, "the peer is not ::1");
    Require(std::memcmp(sce_net_in6addr_any, unspecified.data(), unspecified.size()) == 0, "sce_net_in6addr_any was modified");
    RequireEqual(receiver.Close(), 0, "close the receiver");
}};

const Case tcpConnectTwice{"TcpConnect_AlreadyConnected_FailsWithIsConn", [] {
    const TcpConnection connection;
    RequireEqual(sceNetConnect(connection.client.id, connection.address.data(), connection.address.size()), static_cast<int>(0x80410138), "second connect");
    RequireEqual(*sceNetErrnoLoc(), errIsConn, "errno");
}};

const Case tcpNonblockingRecv{"TcpRecv_NonblockingWithoutData_FailsWithWouldBlock", [] {
    const TcpConnection connection;
    const int nonblocking = 1;
    RequireEqual(sceNetSetsockopt(connection.accepted.id, 0xffff, 0x1200, &nonblocking, sizeof(nonblocking)), 0, "set nonblocking");
    char pending = 0;
    RequireEqual(sceNetRecv(connection.accepted.id, &pending, sizeof(pending), 0), static_cast<std::int64_t>(0x80410123), "receive without data");
    RequireEqual(*sceNetErrnoLoc(), errWouldBlock, "errno");
}};

const Case tcpNonblockingConnect{"TcpConnect_NonblockingSocket_CompletesOrReportsInProgress", [] {
    const TcpConnection connection;
    Socket connecting(nullptr, afInet, sockStream, ipprotoTcp);
    Require(connecting.id >= 0, "create the socket");
    const int nonblocking = 1;
    RequireEqual(sceNetSetsockopt(connecting.id, 0xffff, 0x1200, &nonblocking, sizeof(nonblocking)), 0, "set nonblocking");
    const int started = sceNetConnect(connecting.id, connection.address.data(), connection.address.size());
    if (started != 0) {
        RequireEqual(started, static_cast<int>(0x80410124), "nonblocking connect result");
        RequireEqual(*sceNetErrnoLoc(), errInProgress, "errno");
    }
    RequireEqual(connecting.Close(), 0, "close");
}};

const Case epollWakesWaiter{"EpollWait_BlockedBeforeRegistration_WakesWhenSocketBecomesReadable", [] {
    const TcpConnection connection;
    std::future<int> waiter;
    const Epoll epoll("guest-sce-net");
    NetEpollEvent ready{};
    waiter = std::async(std::launch::async, [&ready, id = epoll.id] { return sceNetEpollWait(id, &ready, 1, -1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    NetEpollEvent registration{};
    registration.events = 1;
    registration.ident = static_cast<std::uint64_t>(connection.accepted.id);
    registration.data.u32 = 77;
    RequireEqual(sceNetEpollControl(epoll.id, 1, connection.accepted.id, &registration), 0, "register the accepted socket");
    RequireEqual(sceNetSend(connection.client.id, tcpRequest, sizeof(tcpRequest), 0), static_cast<std::int64_t>(sizeof(tcpRequest)), "send");
    Require(waiter.wait_for(waiterDeadline) == std::future_status::ready, "the waiter did not wake");
    RequireEqual(waiter.get(), 1, "ready count");
    Require((ready.events & 1) != 0, "the socket was not reported readable");
    RequireEqual(ready.ident, static_cast<std::uint64_t>(connection.accepted.id), "ready socket");
    RequireEqual(ready.data.u32, 77u, "registration data");
}};

const Case epollReadable{"EpollWait_PendingData_ReportsSocketUntilDrained", [] {
    const TcpConnection connection;
    const Epoll epoll("guest-sce-net");
    NetEpollEvent registration{};
    registration.events = 1;
    registration.ident = static_cast<std::uint64_t>(connection.accepted.id);
    RequireEqual(sceNetEpollControl(epoll.id, 1, connection.accepted.id, &registration), 0, "register the accepted socket");
    RequireEqual(sceNetSend(connection.client.id, tcpRequest, sizeof(tcpRequest), 0), static_cast<std::int64_t>(sizeof(tcpRequest)), "send");
    NetEpollEvent ready{};
    RequireEqual(sceNetEpollWait(epoll.id, &ready, 1, 1000000), 1, "wait for data");
    RequireEqual(sceNetEpollWait(epoll.id, &ready, 1, 1000000), 1, "wait again before draining");
    char response[sizeof(tcpRequest)]{};
    RequireEqual(sceNetRecv(connection.accepted.id, response, sizeof(response), 0), static_cast<std::int64_t>(sizeof(response)), "receive");
    RequireEqual(std::string(response), std::string(tcpRequest), "received text");
    RequireEqual(sceNetSend(connection.client.id, tcpRequest, sizeof(tcpRequest), 0), static_cast<std::int64_t>(sizeof(tcpRequest)), "send again");
    ready = {};
    RequireEqual(sceNetEpollWait(epoll.id, &ready, 1, 1000000), 1, "wait for the second message");
    Require((ready.events & 1) != 0, "the socket was not reported readable");
    RequireEqual(ready.ident, static_cast<std::uint64_t>(connection.accepted.id), "ready socket");
}};

const Case streamSendmsg{"Sendmsg_StreamSocket_GathersIntoOneReceive", [] {
    const TcpConnection connection;
    char reply[] = "stream reply";
    NetIovec replyOut[] = {{reply, 7}, {reply + 7, sizeof(reply) - 7}};
    const NetMsghdr replySend{nullptr, 0, replyOut, 2, nullptr, 0, 0};
    RequireEqual(sceNetSendmsg(connection.accepted.id, &replySend, 0), static_cast<std::int64_t>(sizeof(reply)), "sendmsg");
    char replyResult[sizeof(reply)]{};
    NetIovec replyIn[] = {{replyResult, sizeof(replyResult)}};
    std::array<std::uint8_t, 16> replyName{};
    NetMsghdr replyReceive{replyName.data(), 16, replyIn, 1, nullptr, 0, -1};
    RequireEqual(sceNetRecvmsg(connection.client.id, &replyReceive, 0), static_cast<std::int64_t>(sizeof(reply)), "recvmsg");
    RequireEqual(std::string(replyResult), std::string(reply), "received text");
    RequireEqual(replyReceive.flags, 0, "message flags");
    RequireEqual(replyReceive.name_length, 0u, "name length on a stream socket");
}};

const Case shutdownWrite{"Shutdown_WriteSide_LaterSendFailsWithBrokenPipe", [] {
    const TcpConnection connection;
    RequireEqual(sceNetShutdown(connection.accepted.id, 1), 0, "shutdown");
    RequireNetFailure(sceNetSend(connection.accepted.id, tcpRequest, sizeof(tcpRequest), 0), errPipe, "send after shutdown");
}};

const Case peerClosed{"Send_AfterPeerClosed_EventuallyFails", [] {
    TcpConnection connection;
    RequireEqual(connection.accepted.Close(), 0, "close the accepted socket");
    bool sendFailed = false;
    for (int attempt = 0; attempt < 100 && !sendFailed; ++attempt) {
        sendFailed = sceNetSend(connection.client.id, tcpRequest, sizeof(tcpRequest), 0) < 0;
        if (!sendFailed) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(sendFailed, "sending to a closed peer kept succeeding");
}};

const Case closeTwice{"SocketClose_AlreadyClosed_FailsWithBadDescriptor", [] {
    TcpConnection connection;
    const int listener = connection.listener.id;
    RequireEqual(connection.listener.Close(), 0, "first close");
    RequireEqual(sceNetSocketClose(listener), resolverBadDescriptor, "second close");
    RequireEqual(*sceNetErrnoLoc(), errBadf, "errno");
}};

const Case udpLoopback{"SendtoRecvfrom_UdpLoopback_DeliversDatagramWithSource", [] {
    const UdpPair pair;
    SendDatagram(pair, udpDatagram, sizeof(udpDatagram));
    char result[sizeof(udpDatagram)]{};
    std::array<std::uint8_t, 16> source{};
    std::uint32_t sourceSize = source.size();
    RequireEqual(sceNetRecvfrom(pair.receiver.id, result, sizeof(result), 0, source.data(), &sourceSize), static_cast<std::int64_t>(sizeof(result)), "receive");
    RequireEqual(std::string(result), std::string(udpDatagram), "received text");
    RequireEqual(source[1], std::uint8_t{2}, "source family");
}};

const Case selectIdle{"Select_NoPendingDatagram_ClearsReadableSet", [] {
    const UdpPair pair;
    std::uint64_t readable[16]{};
    SetBit(readable, pair.receiver.id);
    const std::int64_t pollNow[2]{0, 0};
    RequireEqual(select_nid_postfix(pair.receiver.id + 1, readable, nullptr, nullptr, pollNow), 0, "select");
    Require(!HasBit(readable, pair.receiver.id), "the idle receiver was reported readable");
}};

const Case selectReadable{"Select_PendingDatagram_ReportsReceiverReadable", [] {
    const UdpPair pair;
    SendDatagram(pair, udpDatagram, sizeof(udpDatagram));
    std::uint64_t readable[16]{};
    SetBit(readable, pair.receiver.id);
    const std::int64_t waitSecond[2]{1, 0};
    RequireEqual(select_nid_postfix(pair.receiver.id + 1, readable, nullptr, nullptr, waitSecond), 1, "select");
    Require(HasBit(readable, pair.receiver.id), "the receiver was not reported readable");
    char result[sizeof(udpDatagram)]{};
    RequireEqual(sceNetRecvfrom(pair.receiver.id, result, sizeof(result), 0, nullptr, nullptr), static_cast<std::int64_t>(sizeof(result)), "receive without a source");
}};

const Case selectInvalid{"Select_InvalidTimeoutOrDescriptorCount_FailsWithEinval", [] {
    const UdpPair pair;
    std::uint64_t readable[16]{};
    SetBit(readable, pair.receiver.id);
    const std::int64_t badTimeout[2]{0, 1000000};
    RequireEqual(select_nid_postfix(pair.receiver.id + 1, readable, nullptr, nullptr, badTimeout), -1, "select with one million microseconds");
    RequireEqual(*__error_nid_postfix(), errInval, "errno for the timeout");
    const std::int64_t pollNow[2]{0, 0};
    RequireEqual(select_nid_postfix(1025, nullptr, nullptr, nullptr, pollNow), -1, "select with 1025 descriptors");
    RequireEqual(*__error_nid_postfix(), errInval, "errno for the descriptor count");
}};

const Case scatterGather{"SendmsgRecvmsg_Datagram_GathersAndScattersWithSource", [] {
    const UdpPair pair;
    ScatterGather message(pair.address);
    RequireEqual(sceNetSendmsg(pair.sender.id, &message.send, 0), std::int64_t{14}, "sendmsg");
    ReceiveBuffers buffers;
    RequireEqual(sceNetRecvmsg(pair.receiver.id, &buffers.receive, 2), std::int64_t{14}, "recvmsg with peek");
    Require(std::memcmp(buffers.first, "scatt", 5) == 0, "first buffer");
    Require(std::memcmp(buffers.second, "er gather", 9) == 0, "second buffer");
    RequireEqual(buffers.receive.flags, 0, "message flags");
    RequireEqual(buffers.receive.control_length, 0u, "control length");
    RequireEqual(buffers.receive.name_length, 16u, "name length");
    RequireEqual(buffers.source[0], std::uint8_t{16}, "source length byte");
    RequireEqual(buffers.source[1], std::uint8_t{2}, "source family");
    RequireEqual(buffers.source[4], std::uint8_t{127}, "source first octet");
    RequireEqual(buffers.source[7], std::uint8_t{1}, "source last octet");
}};

const Case truncatedDatagram{"Recvmsg_DatagramLargerThanBuffer_ReportsTruncation", [] {
    const UdpPair pair;
    ScatterGather message(pair.address);
    RequireEqual(sceNetSendmsg(pair.sender.id, &message.send, 0), std::int64_t{14}, "sendmsg");
    ReceiveBuffers buffers;
    buffers.receive.iov_length = 1;
    buffers.receive.flags = -1;
    RequireEqual(sceNetRecvmsg(pair.receiver.id, &buffers.receive, 0), std::int64_t{5}, "recvmsg into one buffer");
    RequireEqual(buffers.receive.flags, 0x10, "truncation flag");
    Require(std::memcmp(buffers.first, "scatt", 5) == 0, "first buffer");
    RequireEqual(buffers.second[0], '\0', "second buffer untouched");
}};

const Case messageNull{"SendmsgRecvmsg_NullMessage_FailsWithFault", [] {
    const UdpPair pair;
    RequireNetFailure(sceNetSendmsg(pair.sender.id, nullptr, 0), errFault, "sendmsg");
    RequireNetFailure(sceNetRecvmsg(pair.receiver.id, nullptr, 0), errFault, "recvmsg");
}};

const Case messageBadIov{"SendmsgRecvmsg_InvalidIovecs_FailWithMatchingErrors", [] {
    const UdpPair pair;
    ScatterGather message(pair.address);
    NetMsghdr bad = message.send;
    bad.iov_length = -1;
    RequireNetFailure(sceNetSendmsg(pair.sender.id, &bad, 0), errMsgSize, "negative iovec count");
    bad.iov_length = 1025;
    RequireNetFailure(sceNetRecvmsg(pair.receiver.id, &bad, 0), errMsgSize, "1025 iovecs");
    bad.iov_length = 1;
    bad.iov = nullptr;
    RequireNetFailure(sceNetSendmsg(pair.sender.id, &bad, 0), errFault, "null iovec array");
    NetIovec hole[] = {{nullptr, 1}};
    bad.iov = hole;
    RequireNetFailure(sceNetRecvmsg(pair.receiver.id, &bad, 0), errFault, "null iovec base");
    NetIovec huge[] = {{message.head, 0x7fffffff}, {message.tail, 1}};
    bad.iov = huge;
    bad.iov_length = 2;
    RequireNetFailure(sceNetSendmsg(pair.sender.id, &bad, 0), errInval, "total length overflow");
}};

const Case messageFlags{"SendmsgRecvmsg_UnsupportedFlagOrUnknownSocket_Fail", [] {
    const UdpPair pair;
    ScatterGather message(pair.address);
    ReceiveBuffers buffers;
    RequireNetFailure(sceNetRecvmsg(pair.receiver.id, &buffers.receive, 1), errOpNotSupp, "recvmsg with flag 1");
    RequireNetFailure(sceNetSendmsg(pair.sender.id, &message.send, 1), errOpNotSupp, "sendmsg with flag 1");
    RequireNetFailure(sceNetRecvmsg(12345, &buffers.receive, 0), errBadf, "recvmsg on an unknown socket");
    RequireNetFailure(sceNetSendmsg(12345, &message.send, 0), errBadf, "sendmsg on an unknown socket");
}};

const Case messageControl{"Sendmsg_ControlData_ThrowsRuntimeError", [] {
    const UdpPair pair;
    ScatterGather message(pair.address);
    char control[16]{};
    NetMsghdr withControl = message.send;
    withControl.control = control;
    withControl.control_length = sizeof(control);
    RequireThrows<std::runtime_error>([&] { sceNetSendmsg(pair.sender.id, &withControl, 0); }, "sendmsg with control data");
}};

const Case resolverFresh{"ResolverGetError_FreshResolver_ReportsNoError", [] {
    InitializeNet();
    const Resolver resolver;
    int error = -1;
    RequireEqual(sceNetResolverGetError(resolver.id, &error), 0, "get error");
    RequireEqual(error, 0, "resolver error");
}};

const Case resolverInvalidHost{"ResolverStartNtoa_InvalidHost_FailsAndRecordsHostNotFound", [] {
    InitializeNet();
    const Resolver resolver;
    std::array<std::uint8_t, 4> ipv4{};
    int error = -1;
    RequireEqual(sceNetResolverStartNtoa(resolver.id, "guest-sce-net.invalid", ipv4.data(), 5000000, 1, 0), resolverHostNotFound, "resolve an invalid host");
    RequireEqual(sceNetResolverGetError(resolver.id, &error), 0, "get error");
    RequireEqual(error, resolverHostNotFound, "resolver error");
    RequireEqual(sceNetResolverStartNtoa(resolver.id, nullptr, ipv4.data(), 5000000, 1, 0), resolverInvalid, "resolve a null host");
    RequireEqual(sceNetResolverGetError(resolver.id, &error), 0, "get error after the null host");
    RequireEqual(error, resolverHostNotFound, "the null host must not replace the recorded error");
}};

const Case resolverLocalhost{"ResolverStartNtoa_Localhost_ResolvesLoopbackAndClearsError", [] {
    InitializeNet();
    const Resolver resolver;
    std::array<std::uint8_t, 4> ipv4{};
    int error = -1;
    RequireEqual(sceNetResolverStartNtoa(resolver.id, "localhost", ipv4.data(), 5000000, 1, 0), 0, "resolve localhost");
    RequireEqual(ipv4[0], std::uint8_t{127}, "first octet");
    RequireEqual(sceNetResolverGetError(resolver.id, &error), 0, "get error");
    RequireEqual(error, 0, "resolver error");
}};

std::array<std::uint8_t, 512> ResolveLocalhostRecords(const Resolver& resolver) {
    std::array<std::uint8_t, 512> records{};
    records.fill(0xA5);
    RequireEqual(sceNetResolverStartNtoaMultipleRecordsEx(resolver.id, "localhost", records.data(), 5000000, 1, 0), 0, "resolve localhost records");
    return records;
}

const Case resolverRecords{"ResolverStartNtoaMultipleRecordsEx_Localhost_FillsDistinctIpv4Records", [] {
    InitializeNet();
    const Resolver resolver;
    const auto records = ResolveLocalhostRecords(resolver);
    std::int32_t family = 0;
    std::int32_t count = 0;
    std::int32_t count4 = 0;
    std::memcpy(&family, records.data() + 16, sizeof(family));
    std::memcpy(&count, records.data() + 320, sizeof(count));
    std::memcpy(&count4, records.data() + 324, sizeof(count4));
    RequireEqual(records[0], std::uint8_t{127}, "first record octet");
    RequireEqual(family, 2, "first record family");
    Require(count >= 1 && count <= 10, "record count " + std::to_string(count) + " outside 1..10");
    RequireEqual(count4, count, "IPv4 record count");
    Require(std::all_of(records.begin() + 32 * count, records.begin() + 320, [](std::uint8_t byte) { return byte == 0; }), "unused record slots are not zeroed");
    Require(std::all_of(records.begin() + 328, records.begin() + 384, [](std::uint8_t byte) { return byte == 0; }), "reserved bytes are not zeroed");
    Require(std::all_of(records.begin() + 384, records.end(), [](std::uint8_t byte) { return byte == 0xA5; }), "bytes after the record structure were written");
    for (int first = 0; first < count; ++first) {
        for (int second = first + 1; second < count; ++second) {
            Require(std::memcmp(records.data() + 32 * first, records.data() + 32 * second, 4) != 0, "records " + std::to_string(first) + " and " + std::to_string(second) + " repeat an address");
        }
    }
}};

const Case resolverRecordsInvalid{"ResolverStartNtoaMultipleRecordsEx_InvalidArguments_FailWithoutWriting", [] {
    InitializeNet();
    const Resolver resolver;
    auto records = ResolveLocalhostRecords(resolver);
    const auto resolved = records;
    RequireEqual(sceNetResolverStartNtoaMultipleRecordsEx(resolver.id, nullptr, records.data(), 5000000, 1, 0), resolverInvalid, "null host");
    RequireEqual(*sceNetErrnoLoc(), errInval, "errno for a null host");
    Require(records == resolved, "a null host modified the records");
    RequireEqual(sceNetResolverStartNtoaMultipleRecordsEx(resolver.id, "localhost", nullptr, 5000000, 1, 0), resolverInvalid, "null records");
    RequireEqual(*sceNetErrnoLoc(), errInval, "errno for null records");
    RequireEqual(sceNetResolverStartNtoaMultipleRecordsEx(resolver.id, "guest-sce-net.invalid", records.data(), 5000000, 1, 0), resolverHostNotFound, "invalid host");
    Require(records == resolved, "an invalid host modified the records");
    RequireEqual(sceNetResolverGetError(resolver.id, nullptr), resolverInvalid, "get error with a null output");
    RequireEqual(*sceNetErrnoLoc(), errInval, "errno for a null error output");
}};

const Case resolverDestroyed{"Resolver_AfterDestroy_RejectsCallsWithBadDescriptor", [] {
    InitializeNet();
    Resolver resolver;
    auto records = ResolveLocalhostRecords(resolver);
    const auto resolved = records;
    RequireEqual(resolver.Destroy(), 0, "destroy");
    int error = -1;
    RequireEqual(sceNetResolverGetError(resolver.destroyedId, &error), resolverBadDescriptor, "get error after destroy");
    RequireEqual(*sceNetErrnoLoc(), errBadf, "errno for get error");
    RequireEqual(error, -1, "error output untouched");
    RequireEqual(sceNetResolverStartNtoaMultipleRecordsEx(resolver.destroyedId, "localhost", records.data(), 5000000, 1, 0), resolverBadDescriptor, "resolve after destroy");
    RequireEqual(*sceNetErrnoLoc(), errBadf, "errno for resolve");
    Require(records == resolved, "a destroyed resolver modified the records");
}};

const Case ctlState{"CtlGetState_NoNetworkConfiguration_ReportsDisconnectedOrIpObtained", [] {
    InitializeNet();
    int state = -1;
    RequireEqual(sceNetCtlGetState(&state), 0, "get state");
    Require(state == 0 || state == 3, "state " + std::to_string(state) + " is neither 0 nor 3");
}};

} // namespace
