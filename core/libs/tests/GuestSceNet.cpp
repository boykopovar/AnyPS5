#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <io.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <cerrno>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/Socket/include/SocketSelectDeadline.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <thread>

extern "C" {
extern const std::uint8_t in6addr_any_nid_postfix[16];
extern const std::uint8_t in6addr_loopback_nid_postfix[16];
int APS5_VABI sceNetInit_nid_postfix(void);
int APS5_VABI sceNetShowNetstatWithMemory(int);
int APS5_VABI sceNetShowNetstat(void);
int APS5_VABI sceNetShowIfconfig(void);
int APS5_VABI sceNetSocket(const char*, int, int, int);
int APS5_VABI sceNetBind_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI sceNetListen(int, int);
int APS5_VABI sceNetGetsockname(int, void*, std::uint32_t*);
int APS5_VABI sceNetGetpeername(int, void*, std::uint32_t*);
int APS5_VABI sceNetConnect(int, const void*, std::uint32_t);
int APS5_VABI sceNetAccept(int, void*, std::uint32_t*);
std::int64_t APS5_VABI sceNetSend(int, const void*, std::size_t, int);
std::int64_t APS5_VABI sceNetRecv(int, void*, std::size_t, int);
std::int64_t APS5_VABI sceNetSendto(int, const void*, std::size_t, int, const void*, std::uint32_t);
std::int64_t APS5_VABI sceNetRecvfrom(int, void*, std::size_t, int, void*, std::uint32_t*);
int APS5_VABI sceNetSocketClose(int);
int APS5_VABI sceNetShutdown(int, int);
int APS5_VABI sceNetSetsockopt(int, int, int, const void*, std::uint32_t);
int APS5_VABI sceNetGetsockopt(int, int, int, void*, std::uint32_t*);
int* APS5_VABI sceNetErrnoLoc(void);
int APS5_VABI sceNetEpollCreate(const char*, int);
int APS5_VABI sceNetEpollControl(int, int, int, const NetEpollEvent*);
int APS5_VABI sceNetEpollWait(int, NetEpollEvent*, int, int);
int APS5_VABI sceNetEpollDestroy(int);
int APS5_VABI sceNetEpollAbort(int, int);
int APS5_VABI sceNetGetsockopt(int, int, int, void*, std::uint32_t*);
extern const std::uint32_t sce_net_in6addr_any[4];
extern const std::uint8_t sce_net_in6addr_linklocal_allnodes[16];
int APS5_VABI sceNetResolverCreate(const char*, int, int);
int APS5_VABI sceNetResolverStartNtoa(int, const char*, void*, int, int, int);
int APS5_VABI sceNetResolverStartNtoa6(int, const char*, void*, int, int, int);
int APS5_VABI sceNetResolverStartNtoaMultipleRecordsEx(int, const char*, void*, int, int, int);
int APS5_VABI sceNetResolverStartNtoaMultipleRecords(int, const char*, void*, int, int, int);
int APS5_VABI sceNetResolverStartAton(int, const void*, char*, int, int, int, int);
int APS5_VABI sceNetResolverAbort(int, int);
int APS5_VABI sceNetResolverDestroy(int);
int APS5_VABI sceNetResolverGetError(int, int*);
int APS5_VABI sceNetCtlGetState(int*);
int APS5_VABI sceNetCtlGetIfStat(NetCtlIfStat*);
int APS5_VABI sceNetCtlRegisterCallbackV6(NetCtlCallback, void*, int*);
int APS5_VABI sceNetCtlUnregisterCallbackV6(int);
std::uint64_t NetCtlReadByteCounter_nid_no_patch(const char*);
int APS5_VABI select_nid_postfix(int, void*, void*, void*, const void*);
int APS5_VABI open_nid_postfix(const char*, int, int);
int APS5_VABI socket_nid_postfix(int, int, int);
int APS5_VABI bind_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI close_nid_postfix(int);
int APS5_VABI setsockopt_nid_postfix(int, int, int, const void*, std::uint32_t);
int APS5_VABI fcntl_nid_postfix(int, int, ...);
struct GuestPollEntry {
    int descriptor;
    short events;
    short revents;
};
int APS5_VABI poll_nid_postfix(GuestPollEntry*, std::uint32_t, int);
int* APS5_VABI __error_nid_postfix();
int APS5_VABI sceNetInetPton(int, const char*, void*);
const char* APS5_VABI sceNetInetNtop(int, const void*, char*, std::uint32_t);
int* APS5_VABI sceNetErrnoLoc(void);
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

static void Require(bool condition) {
    if (!condition) std::abort();
}

static bool Failed(std::int64_t result, int error) {
    return result == static_cast<int>(0x80410100u | static_cast<unsigned>(error)) && *sceNetErrnoLoc() == error;
}

static void CheckStreamSender(int family) {
    const auto addressSize = family == 2 ? 16u : 28u;
    const int listener = sceNetSocket(nullptr, family, 1, 6);
    Require(listener >= 0);
    std::array<std::uint8_t, 28> destination{};
    destination[0] = static_cast<std::uint8_t>(addressSize);
    destination[1] = static_cast<std::uint8_t>(family);
    if (family == 2) { destination[4] = 127; destination[7] = 1; }
    else destination[23] = 1;
    Require(sceNetBind_nid_postfix(listener, destination.data(), addressSize) == 0 && sceNetListen(listener, 1) == 0);
    std::uint32_t size = destination.size();
    Require(sceNetGetsockname(listener, destination.data(), &size) == 0 && size == addressSize);
    const int client = sceNetSocket(nullptr, family, 1, 6);
    Require(client >= 0 && sceNetConnect(client, destination.data(), addressSize) == 0);
    const int accepted = sceNetAccept(listener, nullptr, nullptr);
    Require(accepted >= 0);
    const char payload[] = "stream";
    for (const auto endpoints : {std::array{accepted, client}, std::array{client, accepted}}) {
        const auto receiver = endpoints[0], sender = endpoints[1];
        for (const std::uint32_t capacity : {0u, 1u, 16u, 28u, UINT32_MAX}) {
            Require(sceNetSend(sender, payload, sizeof(payload), 0) == sizeof(payload));
            std::array<std::uint8_t, 28> address;
            address.fill(0xa5);
            char received[sizeof(payload)]{};
            size = capacity;
            *sceNetErrnoLoc() = 123;
            Require(sceNetRecvfrom(receiver, received, 0, 2, address.data(), &size) == 0);
            Require(size == 0 && received[0] == 0 && *sceNetErrnoLoc() == 123);
            Require(std::all_of(address.begin(), address.end(), [](auto byte) { return byte == 0xa5; }));
            for (const int flags : {2, 0}) {
                size = capacity;
                *sceNetErrnoLoc() = 123;
                Require(sceNetRecvfrom(receiver, received, sizeof(received), flags, address.data(), &size) == sizeof(payload));
                Require(size == 0 && std::memcmp(received, payload, sizeof(payload)) == 0 && *sceNetErrnoLoc() == 123);
                Require(std::all_of(address.begin(), address.end(), [](auto byte) { return byte == 0xa5; }));
            }
        }
        Require(sceNetSend(sender, payload, sizeof(payload), 0) == sizeof(payload));
        std::array<std::uint8_t, 28> address;
        address.fill(0xa5);
        char received[sizeof(payload)]{};
        Require(Failed(sceNetRecvfrom(receiver, received, sizeof(received), 0, address.data(), nullptr), 22));
        size = 123;
        Require(sceNetRecvfrom(receiver, received, sizeof(received), 0, nullptr, &size) == sizeof(payload));
        Require(size == 123 && std::memcmp(received, payload, sizeof(payload)) == 0);
        Require(sceNetShutdown(sender, 1) == 0);
        for (const int flags : {2, 0}) {
            size = address.size();
            *sceNetErrnoLoc() = 123;
            Require(sceNetRecvfrom(receiver, received, sizeof(received), flags, address.data(), &size) == 0);
            Require(size == 0 && *sceNetErrnoLoc() == 123);
            Require(std::all_of(address.begin(), address.end(), [](auto byte) { return byte == 0xa5; }));
        }
    }
    for (const int descriptor : {client, accepted, listener}) Require(sceNetSocketClose(descriptor) == 0);
}

static std::string CaptureNetstat(int pool) {
    auto* file = std::tmpfile();
    Require(file != nullptr);
    Require(std::fflush(stdout) == 0);
#ifdef _WIN32
    const int output = _fileno(stdout);
    const int saved = _dup(output);
    Require(saved >= 0 && _dup2(_fileno(file), output) == 0);
#else
    const int output = fileno(stdout);
    const int saved = dup(output);
    Require(saved >= 0 && dup2(fileno(file), output) == output);
#endif
    const int result = sceNetShowNetstatWithMemory(pool);
    Require(std::fflush(stdout) == 0);
#ifdef _WIN32
    Require(_dup2(saved, output) == 0 && _close(saved) == 0);
#else
    Require(dup2(saved, output) == output && close(saved) == 0);
#endif
    Require(result == 0 && std::fseek(file, 0, SEEK_SET) == 0);
    std::string text;
    std::array<char, 256> bytes{};
    for (std::size_t count; (count = std::fread(bytes.data(), 1, bytes.size(), file)) != 0;) text.append(bytes.data(), count);
    Require(std::ferror(file) == 0 && std::fclose(file) == 0);
    return text;
}

static void CheckNetstat() {
    Require(sceNetShowNetstat() == 0);
    Require(sceNetShowIfconfig() == 0);
    Require(Failed(sceNetShowNetstatWithMemory(-1), 9));
    const int pool = sceNetPoolCreate("netstat", 4096, 0);
    Require(pool > 0);
    const auto empty = CaptureNetstat(pool);
    Require(empty.find("bytes=4096 sockets=0") != std::string::npos);
    const int socket = sceNetSocket("netstat", 2, 1, 0);
    Require(socket >= 0);
    const int nonblocking = 1;
    Require(sceNetSetsockopt(socket, 0xffff, 0x1200, &nonblocking, sizeof(nonblocking)) == 0);
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(sceNetBind_nid_postfix(socket, address.data(), address.size()) == 0);
    Require(sceNetListen(socket, 1) == 0);
    std::uint32_t size = address.size();
    Require(sceNetGetsockname(socket, address.data(), &size) == 0 && size == address.size());
    const auto before = address;
    const auto snapshot = CaptureNetstat(pool);
    Require(snapshot.find("bytes=4096 sockets=1") != std::string::npos);
    Require(snapshot.find("socket=" + std::to_string(socket) + " family=2 type=1 nonblock=1 bound=1 listening=1") != std::string::npos);
    Require(sceNetGetsockname(socket, address.data(), &size) == 0 && address == before);
    Require(sceNetSocketClose(socket) == 0);
    Require(CaptureNetstat(pool).find("sockets=0") != std::string::npos);
    Require(sceNetPoolDestroy(pool) == 0);
    Require(Failed(sceNetShowNetstatWithMemory(pool), 9));
}

static void CheckPoolStats() {
    const int pool = sceNetPoolCreate("stats", 0x4000, 0);
    Require(pool > 0);
    NetMemoryPoolStats stats{1, 1, 1};
    Require(sceNetGetMemoryPoolStats(pool, &stats) == 0);
    Require(stats.pool_size == 0x4000 && stats.max_inuse_size == 0 && stats.current_inuse_size == 0);
    Require(Failed(sceNetGetMemoryPoolStats(pool, nullptr), 22));
    Require(sceNetPoolDestroy(pool) == 0);
    Require(Failed(sceNetGetMemoryPoolStats(pool, &stats), 9));
}

static void CheckUnboundSocketName() {
    for (const int family : {2, 28}) {
        for (const int type : {1, 2}) {
            const int socket = sceNetSocket("unbound", family, type, type == 1 ? 6 : 17);
            if (socket < 0 && family == 28 && *sceNetErrnoLoc() == 47) continue;
            Require(socket >= 0);
            const std::uint32_t addressSize = family == 2 ? 16 : 28;
            std::array<std::uint8_t, 28> address;
            address.fill(0xa5);
            std::uint32_t size = address.size();
            Require(sceNetGetsockname(socket, address.data(), &size) == 0 && size == addressSize);
            Require(address[0] == addressSize && address[1] == family);
            Require(std::all_of(address.begin() + 2, address.begin() + addressSize, [](auto byte) { return byte == 0; }));
            Require(sceNetSocketClose(socket) == 0);
        }
    }
}

static void CheckEmptyEpoll() {
    const int epoll = sceNetEpollCreate("empty-epoll", 0);
    Require(epoll >= 0);
    NetEpollEvent event{};
    Require(sceNetEpollWait(epoll, &event, 1, 0) == 0);
    std::array<std::chrono::steady_clock::duration, 9> elapsed{};
    for (auto& sample : elapsed) {
        const auto start = std::chrono::steady_clock::now();
        Require(sceNetEpollWait(epoll, &event, 1, 1000) == 0);
        sample = std::chrono::steady_clock::now() - start;
        Require(sample >= std::chrono::microseconds(1000));
    }
#ifdef _WIN32
    if (std::getenv("APS5_NO_TIMER_RESOLUTION") == nullptr) {
        std::sort(elapsed.begin(), elapsed.end());
        Require(elapsed[elapsed.size() / 2] < std::chrono::milliseconds(8));
    }
#endif
    Require(sceNetEpollAbort(epoll, 0) == 0);
    Require(Failed(sceNetEpollWait(epoll, &event, 1, 1000), 53));
    Require(sceNetEpollWait(epoll, &event, 1, 0) == 0);
    Require(sceNetEpollDestroy(epoll) == 0);

    for (const bool abort : {true, false}) {
        const int waiting_epoll = sceNetEpollCreate("waiting-epoll", 0);
        Require(waiting_epoll >= 0);
        auto waiter = std::async(std::launch::async, [waiting_epoll, abort] {
            NetEpollEvent ready{};
            Require(Failed(sceNetEpollWait(waiting_epoll, &ready, 1, -1), abort ? 53 : 9));
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        Require((abort ? sceNetEpollAbort(waiting_epoll, 0) : sceNetEpollDestroy(waiting_epoll)) == 0);
        Require(waiter.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
        waiter.get();
        if (abort) Require(sceNetEpollDestroy(waiting_epoll) == 0);
    }
}

static void CheckDontWait() {
    constexpr int DontWait = 0x80;
    constexpr int Peek = 0x2;
    const int receiver = sceNetSocket("dontwait-receiver", 2, 2, 17);
    const int sender = sceNetSocket("dontwait-sender", 2, 2, 17);
    Require(receiver >= 0 && sender >= 0);
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(sceNetBind_nid_postfix(receiver, address.data(), address.size()) == 0);
    std::uint32_t size = address.size();
    Require(sceNetGetsockname(receiver, address.data(), &size) == 0);
    const int timeout = 2000000;
    Require(sceNetSetsockopt(receiver, 0xffff, 0x1006, &timeout, sizeof(timeout)) == 0);
    char buffer[16]{};
    NetIovec in[] = {{buffer, sizeof(buffer)}};
    NetMsghdr receive{nullptr, 0, in, 1, nullptr, 0, 0};
#ifdef _WIN32
    Require(Failed(sceNetRecv(receiver, buffer, sizeof(buffer), DontWait), 45));
    Require(Failed(sceNetRecvfrom(receiver, buffer, sizeof(buffer), DontWait, nullptr, nullptr), 45));
    Require(Failed(sceNetRecvmsg(receiver, &receive, DontWait), 45));
    Require(Failed(sceNetSendto(sender, "x", 1, DontWait, address.data(), address.size()), 45));
#else
    const auto start = std::chrono::steady_clock::now();
    Require(Failed(sceNetRecv(receiver, buffer, sizeof(buffer), DontWait), 35));
    Require(Failed(sceNetRecvfrom(receiver, buffer, sizeof(buffer), DontWait, nullptr, nullptr), 35));
    Require(Failed(sceNetRecvmsg(receiver, &receive, DontWait), 35));
    Require(Failed(sceNetRecv(receiver, buffer, sizeof(buffer), DontWait | Peek), 35));
    Require(std::chrono::steady_clock::now() - start < std::chrono::seconds(1));
    const char datagram[] = "nowait";
    Require(sceNetSendto(sender, datagram, sizeof(datagram), DontWait, address.data(), address.size()) == sizeof(datagram));
    Require(sceNetRecvfrom(receiver, buffer, sizeof(buffer), Peek, nullptr, nullptr) == sizeof(datagram));
    std::memset(buffer, 0, sizeof(buffer));
    Require(sceNetRecv(receiver, buffer, sizeof(buffer), DontWait | Peek) == sizeof(datagram) && std::strcmp(buffer, datagram) == 0);
    std::memset(buffer, 0, sizeof(buffer));
    Require(sceNetRecv(receiver, buffer, sizeof(buffer), DontWait) == sizeof(datagram) && std::strcmp(buffer, datagram) == 0);
    Require(sceNetConnect(sender, address.data(), address.size()) == 0);
    Require(sceNetSend(sender, datagram, sizeof(datagram), DontWait) == sizeof(datagram));
    Require(sceNetRecvmsg(receiver, &receive, 0) == sizeof(datagram) && std::strcmp(buffer, datagram) == 0);
    Require(Failed(sceNetRecv(receiver, buffer, sizeof(buffer), DontWait), 35));
#endif
    Require(Failed(sceNetRecv(receiver, buffer, sizeof(buffer), 0x1), 45));
    Require(Failed(sceNetSend(sender, "x", 1, Peek), 45));
    Require(sceNetSocketClose(receiver) == 0 && sceNetSocketClose(sender) == 0);
}

static void CheckReceiveTimeout(int receiver, int sender, const std::array<std::uint8_t, 16>& address) {
    const int timeout = 50000;
    Require(sceNetSetsockopt(receiver, 0xffff, 0x1006, &timeout, sizeof(timeout)) == 0);
    char buffer[8]{};
    std::array<std::uint8_t, 16> source{};
    std::uint32_t size = source.size();
    NetIovec in[] = {{buffer, sizeof(buffer)}};
    NetMsghdr receive{source.data(), static_cast<std::uint32_t>(source.size()), in, 1, nullptr, 0, 0};
    Require(Failed(sceNetRecv(receiver, buffer, sizeof(buffer), 0), 35));
    Require(Failed(sceNetRecvfrom(receiver, buffer, sizeof(buffer), 0, source.data(), &size), 35));
    Require(Failed(sceNetRecvmsg(receiver, &receive, 0), 35));
    const char datagram[] = "late";
    Require(sceNetSendto(sender, datagram, sizeof(datagram), 0, address.data(), address.size()) == sizeof(datagram));
    Require(sceNetRecv(receiver, buffer, sizeof(buffer), 0) == sizeof(datagram) && std::strcmp(buffer, datagram) == 0);
    const int disabled = 0;
    Require(sceNetSetsockopt(receiver, 0xffff, 0x1006, &disabled, sizeof(disabled)) == 0);
}

static void CheckMicrosecondTimeouts(int receiver) {
    const int receiveTimeout = 50000;
    const int sendTimeout = 70000;
    Require(sceNetSetsockopt(receiver, 0xffff, 0x1106, &receiveTimeout, sizeof(receiveTimeout)) == 0);
    Require(sceNetSetsockopt(receiver, 0xffff, 0x1105, &sendTimeout, sizeof(sendTimeout)) == 0);
    for (const auto [option, expected] : {std::array{0x1106, receiveTimeout}, std::array{0x1105, sendTimeout}}) {
        int value = -1;
        std::uint32_t size = sizeof(value);
        Require(sceNetGetsockopt(receiver, 0xffff, option, &value, &size) == 0 && size == sizeof(value) && value == expected);
    }
    char buffer[8]{};
    Require(Failed(sceNetRecv(receiver, buffer, sizeof(buffer), 0), 35));
    const int disabled = 0;
    Require(sceNetSetsockopt(receiver, 0xffff, 0x1106, &disabled, sizeof(disabled)) == 0);
    Require(sceNetSetsockopt(receiver, 0xffff, 0x1105, &disabled, sizeof(disabled)) == 0);
}

static void CheckMessages(int receiver, int sender, const std::array<std::uint8_t, 16>& address) {
    char head[] = "scatter ";
    char tail[] = "gather";
    NetIovec out[] = {{head, 8}, {nullptr, 0}, {tail, 6}};
    std::array<std::uint8_t, 16> destination = address;
    NetMsghdr send{destination.data(), 16, out, 3, nullptr, 0, 0};
    Require(sceNetSendmsg(sender, &send, 0) == 14);

    char first[5]{};
    char second[20]{};
    NetIovec in[] = {{first, sizeof(first)}, {second, sizeof(second)}};
    std::array<std::uint8_t, 28> source{};
    char control[16]{};
    NetMsghdr receive{source.data(), static_cast<std::uint32_t>(source.size()), in, 2, control, sizeof(control), -1};
    Require(sceNetRecvmsg(receiver, &receive, 2) == 14);
    Require(std::memcmp(first, "scatt", 5) == 0 && std::memcmp(second, "er gather", 9) == 0);
    Require(receive.flags == 0 && receive.control_length == 0 && receive.name_length == 16);
    Require(source[0] == 16 && source[1] == 2 && source[4] == 127 && source[7] == 1);
    std::memset(second, 0, sizeof(second));
    receive.iov_length = 1;
    receive.flags = -1;
    Require(sceNetRecvmsg(receiver, &receive, 0) == 5);
    Require(receive.flags == 0x10 && std::memcmp(first, "scatt", 5) == 0 && second[0] == 0);

    NetMsghdr bad = send;
    Require(Failed(sceNetSendmsg(sender, nullptr, 0), 14));
    Require(Failed(sceNetRecvmsg(receiver, nullptr, 0), 14));
    bad.iov_length = -1;
    Require(Failed(sceNetSendmsg(sender, &bad, 0), 40));
    bad.iov_length = 1025;
    Require(Failed(sceNetRecvmsg(receiver, &bad, 0), 40));
    bad.iov_length = 1;
    bad.iov = nullptr;
    Require(Failed(sceNetSendmsg(sender, &bad, 0), 14));
    NetIovec hole[] = {{nullptr, 1}};
    bad.iov = hole;
    Require(Failed(sceNetRecvmsg(receiver, &bad, 0), 14));
    NetIovec huge[] = {{head, 0x7fffffff}, {tail, 1}};
    bad.iov = huge;
    bad.iov_length = 2;
    Require(Failed(sceNetSendmsg(sender, &bad, 0), 22));
    Require(Failed(sceNetRecvmsg(receiver, &receive, 1), 45));
    Require(Failed(sceNetSendmsg(sender, &send, 1), 45));
    Require(Failed(sceNetRecvmsg(12345, &receive, 0), 9));
    Require(Failed(sceNetSendmsg(12345, &send, 0), 9));
    NetMsghdr with_control = send;
    with_control.control = control;
    with_control.control_length = sizeof(control);
    bool threw = false;
    try {
        sceNetSendmsg(sender, &with_control, 0);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}

static void CheckEpollWritable() {
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    const int receiver = sceNetSocket("epoll-receiver", 2, 2, 17);
    const int sender = sceNetSocket("epoll-sender", 2, 2, 17);
    Require(receiver >= 0 && sender >= 0);
    Require(sceNetBind_nid_postfix(receiver, address.data(), address.size()) == 0);
    std::uint32_t size = address.size();
    Require(sceNetGetsockname(receiver, address.data(), &size) == 0 && size == address.size());
    const int epoll = sceNetEpollCreate("epoll-writable", 0);
    Require(epoll >= 0);
    NetEpollEvent registration{};
    registration.events = 2;
    registration.ident = static_cast<std::uint64_t>(sender);
    registration.data.u32 = 41;
    Require(sceNetEpollControl(epoll, 1, sender, &registration) == 0);
    NetEpollEvent ready{};
    Require(sceNetEpollWait(epoll, &ready, 1, 1000000) == 1);
    Require(ready.events == 2 && ready.ident == static_cast<std::uint64_t>(sender) && ready.data.u32 == 41);
    Require(sceNetEpollControl(epoll, 3, sender, nullptr) == 0);
    const char payload[] = "epoll";
    char peeked[sizeof(payload)]{};
    Require(sceNetSendto(sender, payload, sizeof(payload), 0, address.data(), address.size()) == sizeof(payload));
    Require(sceNetRecv(receiver, peeked, sizeof(peeked), 2) == sizeof(payload));
    registration.events = 3;
    registration.ident = static_cast<std::uint64_t>(receiver);
    Require(sceNetEpollControl(epoll, 1, receiver, &registration) == 0);
    Require(sceNetEpollWait(epoll, &ready, 1, 1000000) == 1);
    Require(ready.events == 3 && ready.ident == static_cast<std::uint64_t>(receiver) && ready.data.u32 == 41);
    Require(sceNetEpollDestroy(epoll) == 0);
    Require(sceNetSocketClose(sender) == 0 && sceNetSocketClose(receiver) == 0);
}

static void RequireTimeout(int socket, int option, int expected) {
    int microseconds = -1;
    std::uint32_t size = sizeof(microseconds);
    Require(sceNetGetsockopt(socket, 0xffff, option, &microseconds, &size) == 0 && size == sizeof(microseconds));
    if (microseconds != expected) {
        std::fprintf(stderr, "setsockopt(0x%x) with a struct timeval on a libSceNet socket set %d microseconds, expected %d\n",
            option, microseconds, expected);
        std::abort();
    }
}

static void CheckKernelTimeouts() {
    const int descriptor = sceNetSocket("kernel-timeouts", 2, 2, 17);
    Require(descriptor >= 0);
    const KernelTimeval receive{1, 250000};
    Require(setsockopt_nid_postfix(descriptor, 0xffff, 0x1006, &receive, sizeof(receive)) == 0);
    RequireTimeout(descriptor, 0x1006, 1250000);
    const KernelTimeval send{0, 500000};
    Require(setsockopt_nid_postfix(descriptor, 0xffff, 0x1005, &send, sizeof(send)) == 0);
    RequireTimeout(descriptor, 0x1005, 500000);
    const KernelTimeval invalid{0, 1000000};
    Require(setsockopt_nid_postfix(descriptor, 0xffff, 0x1006, &invalid, sizeof(invalid)) == -1 && *__error_nid_postfix() == 33);
    const KernelTimeval negative{-1, 0};
    Require(setsockopt_nid_postfix(descriptor, 0xffff, 0x1005, &negative, sizeof(negative)) == -1 && *__error_nid_postfix() == 33);
    const int microseconds = 1000;
    Require(setsockopt_nid_postfix(descriptor, 0xffff, 0x1006, &microseconds, sizeof(microseconds)) == -1 && *__error_nid_postfix() == 22);
    RequireTimeout(descriptor, 0x1006, 1250000);
    RequireTimeout(descriptor, 0x1005, 500000);
    Require(sceNetSetsockopt(descriptor, 0xffff, 0x1006, &microseconds, sizeof(microseconds)) == 0);
    RequireTimeout(descriptor, 0x1006, microseconds);
#ifndef _WIN32
    std::array<std::uint8_t, 16> loopback{16, 2, 0, 0, 127, 0, 0, 1};
    Require(sceNetBind_nid_postfix(descriptor, loopback.data(), loopback.size()) == 0);
    const KernelTimeval brief{0, 50000};
    Require(setsockopt_nid_postfix(descriptor, 0xffff, 0x1006, &brief, sizeof(brief)) == 0);
    char buffer[8]{};
    const auto start = std::chrono::steady_clock::now();
    Require(Failed(sceNetRecv(descriptor, buffer, sizeof(buffer), 0), 35));
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(40));
#endif
    Require(sceNetSocketClose(descriptor) == 0);
}

static void CheckKernelStatusFlags() {
    const int descriptor = sceNetSocket("kernel-status", 2, 1, 6);
    Require(descriptor >= 0);
    Require(fcntl_nid_postfix(descriptor, 3) == 2);
    Require(fcntl_nid_postfix(descriptor, 4, 2 | 4) == 0);
    Require(fcntl_nid_postfix(descriptor, 3) == (2 | 4));
    int enabled = 0;
    std::uint32_t length = sizeof(enabled);
    Require(sceNetGetsockopt(descriptor, 0xffff, 0x1200, &enabled, &length) == 0 && enabled == 1);
    Require(fcntl_nid_postfix(descriptor, 4, 2) == 0);
    Require(fcntl_nid_postfix(descriptor, 3) == 2);
    length = sizeof(enabled);
    Require(sceNetGetsockopt(descriptor, 0xffff, 0x1200, &enabled, &length) == 0 && enabled == 0);
    *__error_nid_postfix() = 0;
    Require(fcntl_nid_postfix(descriptor, 4, 0x100) == -1 && *__error_nid_postfix() == 45);
    *__error_nid_postfix() = 0;
    Require(fcntl_nid_postfix(descriptor, 1) == -1 && *__error_nid_postfix() == 22);
    Require(close_nid_postfix(descriptor) == 0);
}

static void CheckAddressText(int family, const char* text) {
    std::array<std::uint8_t, 16> address{};
    Require(sceNetInetPton(family, text, address.data()) == 1);
    std::array<char, 64> output{};
    const auto length = static_cast<std::uint32_t>(std::strlen(text));
    for (std::uint32_t size = 0; size <= length; ++size) {
        *sceNetErrnoLoc() = 123;
        Require(sceNetInetNtop(family, address.data(), output.data(), size) == nullptr);
        Require(*sceNetErrnoLoc() == 28);
    }
    output.fill('x');
    *sceNetErrnoLoc() = 123;
    Require(sceNetInetNtop(family, address.data(), output.data(), length + 1) == output.data());
    Require(std::strcmp(output.data(), text) == 0 && output[length + 1] == 'x');
    Require(*sceNetErrnoLoc() == 123);
    Require(sceNetInetNtop(99, address.data(), output.data(), output.size()) == nullptr && *sceNetErrnoLoc() == 47);
    Require(sceNetInetNtop(family, nullptr, output.data(), output.size()) == nullptr && *sceNetErrnoLoc() == 22);
    Require(sceNetInetNtop(family, address.data(), nullptr, output.size()) == nullptr && *sceNetErrnoLoc() == 22);
}

static void CheckEpollIdent() {
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    const int receiver = sceNetSocket("epoll-ident", 2, 2, 17);
    const int sender = sceNetSocket("epoll-ident-sender", 2, 2, 17);
    Require(receiver >= 0 && sender >= 0);
    Require(sceNetBind_nid_postfix(receiver, address.data(), address.size()) == 0);
    std::uint32_t size = address.size();
    Require(sceNetGetsockname(receiver, address.data(), &size) == 0 && size == address.size());
    const int epoll = sceNetEpollCreate("epoll-ident", 0);
    Require(epoll >= 0);
    NetEpollEvent registration{};
    registration.events = 1;
    registration.reserved = 0x5a5a5a5a;
    registration.ident = 0x123456789abcdef0;
    registration.data.u64 = 0x1122334455667788;
    Require(sceNetEpollControl(epoll, 1, receiver, &registration) == 0);
    const char payload[] = "ident";
    Require(sceNetSendto(sender, payload, sizeof(payload), 0, address.data(), address.size()) == sizeof(payload));
    NetEpollEvent ready{};
    Require(sceNetEpollWait(epoll, &ready, 1, 1000000) == 1);
    Require(ready.events == 1 && ready.reserved == 0 && ready.ident == static_cast<std::uint64_t>(receiver));
    Require(ready.data.u64 == 0x1122334455667788);
    Require(sceNetEpollDestroy(epoll) == 0);
    Require(sceNetSocketClose(sender) == 0 && sceNetSocketClose(receiver) == 0);
}

static bool ipv6Unavailable = false;

static bool HostIpv6LoopbackUnavailable() {
    const auto hostSocket = ::socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
#ifdef _WIN32
    if (hostSocket == INVALID_SOCKET) {
        const int error = WSAGetLastError();
        return error == WSAEAFNOSUPPORT || error == WSAEPROTONOSUPPORT;
    }
#else
    if (hostSocket < 0) return errno == EAFNOSUPPORT || errno == EPROTONOSUPPORT;
#endif

    sockaddr_in6 loopback{};
    loopback.sin6_family = AF_INET6;
    loopback.sin6_port = htons(9);
    loopback.sin6_addr = in6addr_loopback;
    const char payload = 0;
    const auto sent = ::sendto(hostSocket, &payload, sizeof(payload), 0,
        reinterpret_cast<const sockaddr*>(&loopback), sizeof(loopback));
#ifdef _WIN32
    const int error = sent < 0 ? WSAGetLastError() : 0;
    closesocket(hostSocket);
    return sent < 0 && (error == WSAEADDRNOTAVAIL || error == WSAENETUNREACH || error == WSAEHOSTUNREACH);
#else
    const int error = sent < 0 ? errno : 0;
    close(hostSocket);
    return sent < 0 && (error == EADDRNOTAVAIL || error == ENETUNREACH || error == EHOSTUNREACH);
#endif
}

static void CheckLinkLocalAllNodesIpv6() {
    Require(reinterpret_cast<std::uintptr_t>(sce_net_in6addr_linklocal_allnodes) % alignof(std::uint32_t) == 0);
    std::array<std::uint8_t, 16> address{};
    Require(sceNetInetPton(28, "ff02::1", address.data()) == 1);
    Require(std::memcmp(sce_net_in6addr_linklocal_allnodes, address.data(), address.size()) == 0);
    std::array<char, 9> text;
    text.fill('x');
    Require(sceNetInetNtop(28, sce_net_in6addr_linklocal_allnodes, text.data(), 7) == nullptr && *sceNetErrnoLoc() == 28);
    Require(sceNetInetNtop(28, sce_net_in6addr_linklocal_allnodes, text.data(), 8) == text.data());
    Require(std::strcmp(text.data(), "ff02::1") == 0 && text[8] == 'x');
    address.fill(0xa5);
    Require(sceNetInetPton(28, text.data(), address.data()) == 1);
    Require(std::memcmp(sce_net_in6addr_linklocal_allnodes, address.data(), address.size()) == 0);
}

template <std::size_t TSize>
static bool Untouched(const std::array<std::uint8_t, TSize>& bytes, std::uint32_t from) {
    return std::all_of(bytes.begin() + from, bytes.end(), [](auto byte) { return byte == 0xa5; });
}

static void CheckDatagramAddressLengths(int family) {
    if (family == 28 && ipv6Unavailable) return;
    const std::uint32_t addressSize = family == 2 ? 16 : 28;
    std::array<std::uint8_t, 28> destination{};
    destination[0] = static_cast<std::uint8_t>(addressSize);
    destination[1] = static_cast<std::uint8_t>(family);
    if (family == 2) { destination[4] = 127; destination[7] = 1; }
    else destination[23] = 1;
    const int receiver = sceNetSocket("length-receiver", family, 2, 17);
    const int sender = sceNetSocket("length-sender", family, 2, 17);
    Require(receiver >= 0 && sender >= 0);
    Require(sceNetBind_nid_postfix(receiver, destination.data(), addressSize) == 0);
    std::uint32_t size = destination.size();
    Require(sceNetGetsockname(receiver, destination.data(), &size) == 0 && size == addressSize);
    Require(sceNetConnect(sender, destination.data(), addressSize) == 0);
    std::array<std::uint8_t, 28> source{};
    size = source.size();
    Require(sceNetGetsockname(sender, source.data(), &size) == 0 && size == addressSize);
    const char payload[] = "address length";
    for (const std::uint32_t capacity : {0u, 1u, 2u, addressSize - 1, addressSize, 32u, UINT32_MAX}) {
        const auto copied = std::min(capacity, addressSize);
        std::array<std::uint8_t, 32> address;
        address.fill(0xa5);
        size = capacity;
        Require(sceNetGetsockname(receiver, address.data(), &size) == 0 && size == copied);
        Require(std::memcmp(address.data(), destination.data(), copied) == 0 && Untouched(address, copied));
        char received[sizeof(payload)]{};
        Require(sceNetSend(sender, payload, sizeof(payload), 0) == sizeof(payload));
        address.fill(0xa5);
        size = capacity;
        Require(sceNetRecvfrom(receiver, received, sizeof(received), 2, address.data(), &size) == sizeof(payload));
        Require(size == copied && std::memcmp(address.data(), source.data(), copied) == 0 && Untouched(address, copied));
        address.fill(0xa5);
        NetIovec in[] = {{received, sizeof(received)}};
        NetMsghdr message{address.data(), capacity, in, 1, nullptr, 0, -1};
        Require(sceNetRecvmsg(receiver, &message, 0) == sizeof(payload) && std::memcmp(received, payload, sizeof(payload)) == 0);
        Require(message.name_length == copied && std::memcmp(address.data(), source.data(), copied) == 0 && Untouched(address, copied));
    }
    Require(sceNetSocketClose(sender) == 0 && sceNetSocketClose(receiver) == 0);
}

static void CheckStreamAddressLengths() {
    const int listener = sceNetSocket("length-listener", 2, 1, 6);
    Require(listener >= 0);
    std::array<std::uint8_t, 16> listening{16, 2, 0, 0, 127, 0, 0, 1};
    Require(sceNetBind_nid_postfix(listener, listening.data(), listening.size()) == 0 && sceNetListen(listener, 1) == 0);
    std::uint32_t size = listening.size();
    Require(sceNetGetsockname(listener, listening.data(), &size) == 0 && size == 16);
    const int client = sceNetSocket("length-client", 2, 1, 6);
    Require(client >= 0 && sceNetConnect(client, listening.data(), listening.size()) == 0);
    std::array<std::uint8_t, 16> clientAddress{};
    size = clientAddress.size();
    Require(sceNetGetsockname(client, clientAddress.data(), &size) == 0 && size == 16);
    std::array<std::uint8_t, 28> peer;
    peer.fill(0xa5);
    size = 1;
    const int accepted = sceNetAccept(listener, peer.data(), &size);
    Require(accepted >= 0 && size == 1 && peer[0] == 16 && Untouched(peer, 1));
    for (const std::uint32_t capacity : {0u, 1u, 15u, 16u, 28u, UINT32_MAX}) {
        const auto copied = std::min(capacity, std::uint32_t{16});
        peer.fill(0xa5);
        size = capacity;
        Require(sceNetGetpeername(accepted, peer.data(), &size) == 0 && size == copied);
        Require(std::memcmp(peer.data(), clientAddress.data(), copied) == 0 && Untouched(peer, copied));
    }
    for (const int descriptor : {accepted, client, listener}) Require(sceNetSocketClose(descriptor) == 0);
}

static void CheckBufferSizes() {
    const int socket = sceNetSocket("buffer-sizes", 2, 2, 17);
    Require(socket >= 0);
    for (const int option : {0x1001, 0x1002}) {
        int value = -1;
        std::uint32_t size = sizeof(value);
        Require(sceNetGetsockopt(socket, 0xffff, option, &value, &size) == 0 && size == sizeof(value) && value > 0);
    }
    for (const auto [option, requested] : {std::array{0x1001, 32768}, std::array{0x1002, 65536}}) {
        Require(sceNetSetsockopt(socket, 0xffff, option, &requested, sizeof(requested)) == 0);
        int value = -1;
        std::uint32_t size = sizeof(value);
        Require(sceNetGetsockopt(socket, 0xffff, option, &value, &size) == 0 && size == sizeof(value) && value == requested);
    }
    Require(sceNetSocketClose(socket) == 0);
}

static void CheckUnspecifiedIpv6() {
    std::array<std::uint8_t, 16> unspecified{};
    Require(sceNetInetPton(28, "::", unspecified.data()) == 1);
    Require(std::memcmp(sce_net_in6addr_any, unspecified.data(), unspecified.size()) == 0);
    char text[4] = {'x', 'x', 'x', 'x'};
    Require(sceNetInetNtop(28, sce_net_in6addr_any, text, 3) == text);
    Require(std::strcmp(text, "::") == 0 && text[3] == 'x');

    const int receiver = sceNetSocket("ipv6-any", 28, 2, 17);
    if (receiver < 0 && *sceNetErrnoLoc() == 47) {
        ipv6Unavailable = true;
        return;
    }
    const int sender = sceNetSocket("ipv6-loopback", 28, 2, 17);
    Require(receiver >= 0 && sender >= 0);
    std::array<std::uint8_t, 28> address{28, 28};
    std::memcpy(address.data() + 8, sce_net_in6addr_any, 16);
    Require(sceNetBind_nid_postfix(receiver, address.data(), address.size()) == 0);
    std::uint32_t size = address.size();
    address.fill(0xa5);
    Require(sceNetGetsockname(receiver, address.data(), &size) == 0 && size == address.size());
    Require(address[0] == 28 && address[1] == 28 && (address[2] != 0 || address[3] != 0));
    Require(std::memcmp(address.data() + 8, unspecified.data(), unspecified.size()) == 0);
    Require(sceNetInetPton(28, "::1", address.data() + 8) == 1);

    const char payload[] = "IPv6 wildcard receive";
    const auto sent = sceNetSendto(sender, payload, sizeof(payload), 0, address.data(), address.size());
    if (sent < 0 && *sceNetErrnoLoc() == 51 && HostIpv6LoopbackUnavailable()) {
        Require(sceNetSocketClose(sender) == 0);
        Require(sceNetSocketClose(receiver) == 0);
        ipv6Unavailable = true;
        return;
    }
    Require(sent == sizeof(payload));
    char received[sizeof(payload)]{};
    std::array<std::uint8_t, 28> peer{};
    size = peer.size();
    Require(sceNetRecvfrom(receiver, received, sizeof(received), 0, peer.data(), &size) == sizeof(received));
    Require(std::memcmp(received, payload, sizeof(payload)) == 0 && size == peer.size() && peer[1] == 28);
    Require(std::memcmp(peer.data() + 8, address.data() + 8, 16) == 0);
    address[0] = 0;
    Require(sceNetSendto(sender, payload, sizeof(payload), 0, address.data(), address.size()) == sizeof(payload));
    Require(sceNetRecvfrom(receiver, received, sizeof(received), 0, nullptr, nullptr) == sizeof(received));
    Require(std::memcmp(sce_net_in6addr_any, unspecified.data(), unspecified.size()) == 0);
    Require(sceNetSocketClose(sender) == 0);
    Require(sceNetSocketClose(receiver) == 0);
}

static void CheckIpv6SocketsAreIpv6Only() {
    const int inet = sceNetSocket("ipv4-any", 2, 2, 17);
    Require(inet >= 0);
    std::array<std::uint8_t, 16> any{16, 2};
    Require(sceNetBind_nid_postfix(inet, any.data(), any.size()) == 0);
    std::uint32_t size = any.size();
    Require(sceNetGetsockname(inet, any.data(), &size) == 0 && size == any.size());
    const int inet6 = sceNetSocket("ipv6-any-same-port", 28, 2, 17);
    Require(inet6 >= 0);
    std::array<std::uint8_t, 28> any6{28, 28, any[2], any[3]};
    if (sceNetBind_nid_postfix(inet6, any6.data(), any6.size()) != 0) {
        std::fprintf(stderr, "sceNetBind of [::]:%d failed with errno %d while an IPv4 socket holds 0.0.0.0:%d\n",
            (any[2] << 8) | any[3], *sceNetErrnoLoc(), (any[2] << 8) | any[3]);
        std::abort();
    }
    Require(sceNetSocketClose(inet6) == 0);
    Require(sceNetSocketClose(inet) == 0);
}

static void CheckSocketError() {
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    const int listener = sceNetSocket(nullptr, 2, 1, 6);
    Require(listener >= 0 && sceNetBind_nid_postfix(listener, address.data(), address.size()) == 0 && sceNetListen(listener, 1) == 0);
    std::uint32_t size = address.size();
    Require(sceNetGetsockname(listener, address.data(), &size) == 0 && size == address.size());
    const int connected = sceNetSocket(nullptr, 2, 1, 6);
    Require(connected >= 0 && sceNetConnect(connected, address.data(), address.size()) == 0);
    int error = -1;
    size = sizeof(error);
    Require(sceNetGetsockopt(connected, 0xffff, 0x1007, &error, &size) == 0 && size == sizeof(error) && error == 0);
    for (const int descriptor : {connected, listener}) Require(sceNetSocketClose(descriptor) == 0);

    const int refused = sceNetSocket(nullptr, 2, 1, 6);
    const int nonblocking = 1;
    Require(refused >= 0 && sceNetSetsockopt(refused, 0xffff, 0x1200, &nonblocking, sizeof(nonblocking)) == 0);
    const int started = sceNetConnect(refused, address.data(), address.size());
    Require(started == static_cast<int>(0x80410124) && *sceNetErrnoLoc() == 36);
    error = 0;
    for (int attempt = 0; attempt < 500 && error == 0; ++attempt) {
        size = sizeof(error);
        Require(sceNetGetsockopt(refused, 0xffff, 0x1007, &error, &size) == 0 && size == sizeof(error));
        if (error == 0) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    Require(error == 61);
    size = sizeof(error);
    Require(sceNetGetsockopt(refused, 0xffff, 0x1007, &error, &size) == 0 && error == 0);
    Require(sceNetSocketClose(refused) == 0);
}

static void CheckDatagramDisconnect() {
    const int peer = sceNetSocket("disconnect-peer", 2, 2, 17);
    const int other = sceNetSocket("disconnect-other", 2, 2, 17);
    const int dissolved = sceNetSocket("disconnect", 2, 2, 17);
    Require(peer >= 0 && other >= 0 && dissolved >= 0);
    std::array<std::uint8_t, 16> peerAddress{16, 2, 0, 0, 127, 0, 0, 1};
    std::array<std::uint8_t, 16> dissolvedAddress{16, 2, 0, 0, 127, 0, 0, 1};
    Require(sceNetBind_nid_postfix(peer, peerAddress.data(), peerAddress.size()) == 0);
    Require(sceNetBind_nid_postfix(dissolved, dissolvedAddress.data(), dissolvedAddress.size()) == 0);
    std::uint32_t size = peerAddress.size();
    Require(sceNetGetsockname(peer, peerAddress.data(), &size) == 0);
    size = dissolvedAddress.size();
    Require(sceNetGetsockname(dissolved, dissolvedAddress.data(), &size) == 0);
    Require(sceNetConnect(dissolved, peerAddress.data(), peerAddress.size()) == 0);
    std::array<std::uint8_t, 16> name{};
    size = name.size();
    Require(sceNetGetpeername(dissolved, name.data(), &size) == 0 && name[2] == peerAddress[2] && name[3] == peerAddress[3]);
    const std::array<std::uint8_t, 16> unspecified{16, 0};
    Require(Failed(sceNetConnect(dissolved, unspecified.data(), unspecified.size()), 47));
    size = name.size();
    Require(Failed(sceNetGetpeername(dissolved, name.data(), &size), 57));
    std::array<std::uint8_t, 16> local{};
    size = local.size();
    Require(sceNetGetsockname(dissolved, local.data(), &size) == 0 && local[2] == dissolvedAddress[2] && local[3] == dissolvedAddress[3]);
#ifndef _WIN32
    Require(local[4] == 0 && local[5] == 0 && local[6] == 0 && local[7] == 0);
#endif
    const int timeout = 2000000;
    Require(sceNetSetsockopt(dissolved, 0xffff, 0x1006, &timeout, sizeof(timeout)) == 0);
    const char unrelated[] = "unrelated sender";
    Require(sceNetSendto(other, unrelated, sizeof(unrelated), 0, dissolvedAddress.data(), dissolvedAddress.size()) == sizeof(unrelated));
    char buffer[32]{};
    Require(sceNetRecvfrom(dissolved, buffer, sizeof(buffer), 0, nullptr, nullptr) == sizeof(unrelated));
    Require(std::strcmp(buffer, unrelated) == 0);
    for (const int descriptor : {peer, other, dissolved}) Require(sceNetSocketClose(descriptor) == 0);
}

int main() {
    const auto epoch = std::chrono::steady_clock::time_point{};
    Require(KernelSocketSelect::DeadlineAfter(epoch, 2, 250000) == epoch + std::chrono::milliseconds(2250));
    Require(KernelSocketSelect::DeadlineAfter(epoch, INT64_MAX, 0) == std::chrono::steady_clock::time_point::max());
    Require(KernelSocketSelect::DeadlineAfter(epoch, 10000000000LL, 0) == std::chrono::steady_clock::time_point::max());
    const auto almostMax = std::chrono::steady_clock::time_point::max() - std::chrono::microseconds(1);
    Require(KernelSocketSelect::DeadlineAfter(almostMax, 0, 1) == std::chrono::steady_clock::time_point::max());
    Require(KernelSocketSelect::DeadlineAfter(almostMax, 0, 2) == std::chrono::steady_clock::time_point::max());
    for (int i = 0; i < 16; ++i) {
        Require(in6addr_any_nid_postfix[i] == 0);
        Require(in6addr_loopback_nid_postfix[i] == (i == 15 ? 1 : 0));
    }
    Require(sceNetInit_nid_postfix() == 0);
    CheckEpollWritable();
    CheckPoolStats();
    CheckNetstat();
    CheckEmptyEpoll();
    CheckDontWait();
    CheckAddressText(2, "127.0.0.1");
    CheckAddressText(2, "255.255.255.255");
    CheckAddressText(28, "::1");
    CheckAddressText(28, "1234:5678:9abc:def0:1234:5678:9abc:def0");
    CheckEpollIdent();
    CheckLinkLocalAllNodesIpv6();
    CheckUnboundSocketName();
    CheckUnspecifiedIpv6();
    if (!ipv6Unavailable) CheckIpv6SocketsAreIpv6Only();
    CheckDatagramDisconnect();

    const int closed = sceNetSocket("closed-by-kernel", 2, 2, 17);
    Require(closed >= 0);
    const int enabled = 1;
    Require(setsockopt_nid_postfix(closed, 0xffff, 0x20, &enabled, sizeof(enabled)) == 0);
    Require(sceNetSetsockopt(closed, 0xffff, 0x20, &enabled, sizeof(enabled)) == 0);
    Require(sceNetSetsockopt(closed, 0xffff, 0x4, &enabled, sizeof(enabled)) == 0);
    Require(close_nid_postfix(closed) == 0);
    Require(Failed(sceNetSocketClose(closed), 9));
    CheckKernelTimeouts();
    CheckSocketError();
    CheckKernelStatusFlags();

    const int listener = sceNetSocket(nullptr, 2, 1, 6);
    Require(listener >= 0);
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(sceNetBind_nid_postfix(listener, address.data(), address.size()) == 0);
    Require(sceNetListen(listener, 4) == 0);
    std::uint32_t address_size = address.size();
    Require(sceNetGetsockname(listener, address.data(), &address_size) == 0 && address_size == 16);
    Require(address[2] != 0 || address[3] != 0);

    const int client = sceNetSocket(nullptr, 2, 1, 6);
    Require(client >= 0);
    Require(sceNetConnect(client, address.data(), address.size()) == 0);
    std::array<std::uint8_t, 16> peer{};
    address_size = peer.size();
    const int accepted = sceNetAccept(listener, peer.data(), &address_size);
    Require(accepted >= 0 && address_size == 16);
    Require(sceNetConnect(client, address.data(), address.size()) == static_cast<int>(0x80410138) && *sceNetErrnoLoc() == 56);
    const int nonblocking = 1;
    Require(sceNetSetsockopt(accepted, 0xffff, 0x1200, &nonblocking, sizeof(nonblocking)) == 0);
    char pending = 0;
    Require(sceNetRecv(accepted, &pending, sizeof(pending), 0) == static_cast<int>(0x80410123) && *sceNetErrnoLoc() == 35);
    const int connecting = sceNetSocket(nullptr, 2, 1, 6);
    Require(connecting >= 0);
    Require(sceNetSetsockopt(connecting, 0xffff, 0x1200, &nonblocking, sizeof(nonblocking)) == 0);
    const int started = sceNetConnect(connecting, address.data(), address.size());
    Require(started == 0 || (started == static_cast<int>(0x80410124) && *sceNetErrnoLoc() == 36));
    Require(sceNetSocketClose(connecting) == 0);

    CheckDatagramAddressLengths(2);
    CheckDatagramAddressLengths(28);
    CheckStreamAddressLengths();
    CheckBufferSizes();

    const int epoll = sceNetEpollCreate("guest-sce-net", 0);
    Require(epoll >= 0);
    NetEpollEvent registration{};
    registration.events = 1;
    registration.ident = static_cast<std::uint64_t>(accepted);
    registration.data.u32 = 77;
    NetEpollEvent ready{};
    auto waiter = std::async(std::launch::async, [&] { return sceNetEpollWait(epoll, &ready, 1, -1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    Require(sceNetEpollControl(epoll, 1, accepted, &registration) == 0);
    const char request[] = "guest tcp loopback";
    char response[sizeof(request)]{};
    Require(sceNetSend(client, request, sizeof(request), 0) == sizeof(request));
    Require(waiter.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    const int ready_count = waiter.get();
    Require(ready_count == 1 && (ready.events & 1) && ready.ident == static_cast<std::uint64_t>(accepted) && ready.data.u32 == 77);
    Require(sceNetEpollWait(epoll, &ready, 1, 1000000) == 1);
    Require(sceNetRecv(accepted, response, sizeof(response), 0) == sizeof(response));
    Require(std::strcmp(request, response) == 0);
    char reply[] = "stream reply";
    NetIovec reply_out[] = {{reply, 7}, {reply + 7, sizeof(reply) - 7}};
    NetMsghdr reply_send{nullptr, 0, reply_out, 2, nullptr, 0, 0};
    Require(sceNetSendmsg(accepted, &reply_send, 0) == sizeof(reply));
    char reply_result[sizeof(reply)]{};
    NetIovec reply_in[] = {{reply_result, sizeof(reply_result)}};
    std::array<std::uint8_t, 16> reply_name{};
    NetMsghdr reply_receive{reply_name.data(), 16, reply_in, 1, nullptr, 0, -1};
    Require(sceNetRecvmsg(client, &reply_receive, 0) == sizeof(reply));
    Require(std::strcmp(reply, reply_result) == 0 && reply_receive.flags == 0 && reply_receive.name_length == 0);
    Require(sceNetSend(client, request, sizeof(request), 0) == sizeof(request));
    ready = {};
    Require(sceNetEpollWait(epoll, &ready, 1, 1000000) == 1 && (ready.events & 1) && ready.ident == static_cast<std::uint64_t>(accepted));
    Require(sceNetShutdown(accepted, 1) == 0);
    Require(Failed(sceNetSend(accepted, request, sizeof(request), 0), 32));
    registration.data.u32 = 78;
    Require(sceNetEpollControl(epoll, 2, accepted, &registration) == 0);
    Require(Failed(sceNetEpollControl(epoll, 2, accepted, nullptr), 22));
    Require(sceNetEpollControl(epoll, 3, accepted, nullptr) == 0);
    Require(Failed(sceNetEpollControl(epoll, 3, accepted, nullptr), 2));
    Require(Failed(sceNetEpollControl(epoll, 2, accepted, &registration), 2));
    Require(sceNetEpollDestroy(epoll) == 0);
    Require(sceNetSocketClose(accepted) == 0);
    bool send_failed = false;
    for (int attempt = 0; attempt < 100 && !send_failed; ++attempt) {
        send_failed = sceNetSend(client, request, sizeof(request), 0) < 0;
        if (!send_failed) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(send_failed);
    Require(sceNetSocketClose(client) == 0);
    Require(sceNetSocketClose(listener) == 0);
    *__error_nid_postfix() = 0;
    Require(sceNetSocketClose(listener) == static_cast<int>(0x80410109) && *sceNetErrnoLoc() == 9);
    Require(*__error_nid_postfix() == 9);

    const int udp_receiver = sceNetSocket(nullptr, 2, 2, 17);
    const int udp_sender = sceNetSocket(nullptr, 2, 2, 17);
    Require(udp_receiver >= 0 && udp_sender >= 0 && udp_receiver < 1024 && udp_sender < 1024);
    address = {16, 2, 0, 0, 127, 0, 0, 1};
    Require(sceNetBind_nid_postfix(udp_receiver, address.data(), address.size()) == 0);
    address_size = address.size();
    Require(sceNetGetsockname(udp_receiver, address.data(), &address_size) == 0);
    const char datagram[] = "guest udp loopback";
    char datagram_result[sizeof(datagram)]{};
    Require(sceNetSendto(udp_sender, datagram, sizeof(datagram), 0, address.data(), address.size()) == sizeof(datagram));
    std::array<std::uint8_t, 16> source{};
    address_size = source.size();
    Require(sceNetRecvfrom(udp_receiver, datagram_result, sizeof(datagram_result), 0,
        source.data(), &address_size) == sizeof(datagram_result));
    Require(std::strcmp(datagram, datagram_result) == 0 && source[1] == 2);
    std::array<std::uint8_t, 16> unsized = address;
    unsized[0] = 0;
    Require(sceNetSendto(udp_sender, datagram, sizeof(datagram), 0, unsized.data(), unsized.size()) == sizeof(datagram));
    Require(sceNetRecvfrom(udp_receiver, datagram_result, sizeof(datagram_result), 0, nullptr, nullptr) == sizeof(datagram_result));
    std::uint64_t readable[16]{};
    readable[udp_receiver / 64] |= std::uint64_t{1} << (udp_receiver % 64);
    const std::int64_t poll_now[2]{0, 0};
    Require(select_nid_postfix(udp_receiver + 1, readable, nullptr, nullptr, poll_now) == 0);
    Require(readable[udp_receiver / 64] == 0);
    Require(sceNetSendto(udp_sender, datagram, sizeof(datagram), 0, address.data(), address.size()) == sizeof(datagram));
    readable[udp_receiver / 64] |= std::uint64_t{1} << (udp_receiver % 64);
    const std::int64_t wait_second[2]{1, 0};
    Require(select_nid_postfix(udp_receiver + 1, readable, nullptr, nullptr, wait_second) == 1);
    Require(((readable[udp_receiver / 64] >> (udp_receiver % 64)) & 1u) != 0);
    Require(sceNetRecvfrom(udp_receiver, datagram_result, sizeof(datagram_result), 0, nullptr, nullptr) == sizeof(datagram_result));
    const std::int64_t bad_timeout[2]{0, 1000000};
    Require(select_nid_postfix(udp_receiver + 1, readable, nullptr, nullptr, bad_timeout) == -1 && *__error_nid_postfix() == 22);
    Require(select_nid_postfix(1025, nullptr, nullptr, nullptr, poll_now) == -1 && *__error_nid_postfix() == 22);
    const int random = open_nid_postfix("/dev/urandom", 0, 0);
    Require(random >= 0 && random < 1024);
    std::uint64_t random_readable[16]{};
    std::uint64_t random_urgent[16]{};
    random_readable[random / 64] |= std::uint64_t{1} << (random % 64);
    random_readable[udp_receiver / 64] |= std::uint64_t{1} << (udp_receiver % 64);
    random_urgent[random / 64] |= std::uint64_t{1} << (random % 64);
    Require(select_nid_postfix(std::max(random, udp_receiver) + 1, random_readable, nullptr, random_urgent, nullptr) == 1);
    Require(((random_readable[random / 64] >> (random % 64)) & 1u) != 0);
    Require(((random_readable[udp_receiver / 64] >> (udp_receiver % 64)) & 1u) == 0);
    Require(random_urgent[random / 64] == 0);
    Require(close_nid_postfix(random) == 0);
    const int kernel_socket = socket_nid_postfix(2, 2, 0);
    Require(kernel_socket >= 0 && kernel_socket < 1024 && kernel_socket != udp_receiver && kernel_socket != udp_sender);
    std::array<std::uint8_t, 16> kernel_address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(kernel_socket, kernel_address.data(), kernel_address.size()) == 0);
    std::uint64_t mixed_writable[16]{};
    mixed_writable[kernel_socket / 64] |= std::uint64_t{1} << (kernel_socket % 64);
    mixed_writable[udp_sender / 64] |= std::uint64_t{1} << (udp_sender % 64);
    Require(select_nid_postfix(std::max(kernel_socket, udp_sender) + 1, nullptr, mixed_writable, nullptr, wait_second) == 2);
    Require(((mixed_writable[kernel_socket / 64] >> (kernel_socket % 64)) & 1u) != 0);
    Require(((mixed_writable[udp_sender / 64] >> (udp_sender % 64)) & 1u) != 0);
    GuestPollEntry net_read{udp_receiver, 1, 0};
    Require(poll_nid_postfix(&net_read, 1, 0) == 0 && net_read.revents == 0);
    Require(sceNetSendto(udp_sender, datagram, sizeof(datagram), 0, address.data(), address.size()) == sizeof(datagram));
    Require(poll_nid_postfix(&net_read, 1, 1000) == 1 && net_read.revents == 1);
    Require(sceNetRecvfrom(udp_receiver, datagram_result, sizeof(datagram_result), 0, nullptr, nullptr) == sizeof(datagram_result));
    GuestPollEntry mixed_poll[2]{{kernel_socket, 4, 0}, {udp_sender, 4, 0}};
    Require(poll_nid_postfix(mixed_poll, 2, 1000) == 2 && mixed_poll[0].revents == 4 && mixed_poll[1].revents == 4);
    Require(close_nid_postfix(kernel_socket) == 0);
    std::uint64_t stdio_readable[16]{1};
    std::uint64_t stdio_writable[16]{6};
    Require(select_nid_postfix(3, stdio_readable, stdio_writable, nullptr, poll_now) == 2);
    Require(stdio_readable[0] == 0 && stdio_writable[0] == 6);
    stdio_readable[0] = 1;
    Require(select_nid_postfix(1, stdio_readable, nullptr, nullptr, poll_now) == 0 && stdio_readable[0] == 0);
    CheckReceiveTimeout(udp_receiver, udp_sender, address);
    CheckMicrosecondTimeouts(udp_receiver);
    CheckMessages(udp_receiver, udp_sender, address);
    Require(sceNetSocketClose(udp_sender) == 0);
    Require(sceNetSocketClose(udp_receiver) == 0);

    const int resolver = sceNetResolverCreate("guest-sce-net", 0, 0);
    Require(resolver >= 0);
    int resolver_error = -1;
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 && resolver_error == 0);
    std::array<std::uint8_t, 4> ipv4{};
    Require(sceNetResolverStartNtoa(resolver, "guest-sce-net.invalid.", ipv4.data(), 5000000, 1, 0) ==
        static_cast<int>(0x804101E1));
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 &&
        resolver_error == static_cast<int>(0x804101E1));
    Require(sceNetResolverStartNtoa(resolver, nullptr, ipv4.data(), 5000000, 1, 0) == static_cast<int>(0x80410116));
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 &&
        resolver_error == static_cast<int>(0x804101E1));
    Require(sceNetResolverStartNtoa(resolver, "localhost", ipv4.data(), 5000000, 1, 0) == 0);
    Require(ipv4[0] == 127);
    std::array<std::uint8_t, 16> resolved6{};
    resolved6.fill(0xA5);
    Require(sceNetResolverStartNtoa6(resolver, "::1", resolved6.data(), 5000000, 1, 0) == 0);
    Require(std::all_of(resolved6.begin(), resolved6.end() - 1, [](std::uint8_t byte) { return byte == 0; }) && resolved6[15] == 1);
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 && resolver_error == 0);
    Require(sceNetResolverStartNtoa6(resolver, nullptr, resolved6.data(), 5000000, 1, 0) == static_cast<int>(0x80410116));
    Require(sceNetResolverStartNtoa6(resolver, "127.0.0.1", resolved6.data(), 5000000, 1, 0) == static_cast<int>(0x804101E1));
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 && resolver_error == static_cast<int>(0x804101E1));
    Require(sceNetResolverStartNtoa(resolver, "localhost", ipv4.data(), 5000000, 1, 0) == 0);
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 && resolver_error == 0);
    std::array<std::uint8_t, 512> records{};
    records.fill(0xA5);
    Require(sceNetResolverStartNtoaMultipleRecordsEx(resolver, "localhost", records.data(), 5000000, 1, 0) == 0);
    std::int32_t record_family = 0;
    std::int32_t record_count = 0;
    std::int32_t record_count4 = 0;
    std::memcpy(&record_family, records.data() + 16, sizeof(record_family));
    std::memcpy(&record_count, records.data() + 320, sizeof(record_count));
    std::memcpy(&record_count4, records.data() + 324, sizeof(record_count4));
    Require(records[0] == 127 && record_family == 2 && record_count >= 1 && record_count <= 10 && record_count4 == record_count);
    Require(std::all_of(records.begin() + 32 * record_count, records.begin() + 320, [](std::uint8_t byte) { return byte == 0; }));
    Require(std::all_of(records.begin() + 328, records.begin() + 384, [](std::uint8_t byte) { return byte == 0; }));
    Require(std::all_of(records.begin() + 384, records.end(), [](std::uint8_t byte) { return byte == 0xA5; }));
    for (int first = 0; first < record_count; ++first) {
        for (int second = first + 1; second < record_count; ++second) {
            Require(std::memcmp(records.data() + 32 * first, records.data() + 32 * second, 4) != 0);
        }
    }
    const auto resolved = records;
    Require(sceNetResolverStartNtoaMultipleRecordsEx(resolver, nullptr, records.data(), 5000000, 1, 0) ==
        static_cast<int>(0x80410116) && *sceNetErrnoLoc() == 22 && records == resolved);
    Require(sceNetResolverStartNtoaMultipleRecordsEx(resolver, "localhost", nullptr, 5000000, 1, 0) ==
        static_cast<int>(0x80410116) && *sceNetErrnoLoc() == 22);
    Require(sceNetResolverStartNtoaMultipleRecordsEx(resolver, "guest-sce-net.invalid.", records.data(), 5000000, 1, 0) ==
        static_cast<int>(0x804101E1) && records == resolved);

    std::array<std::uint8_t, 512> plain{};
    plain.fill(0xA5);
    Require(sceNetResolverStartNtoaMultipleRecords(resolver, "localhost", plain.data(), 5000000, 1, 0) == 0);
    Require(plain == resolved);
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 && resolver_error == 0);
    Require(sceNetResolverStartNtoaMultipleRecords(resolver, nullptr, plain.data(), 5000000, 1, 0) ==
        static_cast<int>(0x80410116) && *sceNetErrnoLoc() == 22 && plain == resolved);
    Require(sceNetResolverStartNtoaMultipleRecords(resolver, "localhost", nullptr, 5000000, 1, 0) ==
        static_cast<int>(0x80410116) && *sceNetErrnoLoc() == 22);
    Require(sceNetResolverStartNtoaMultipleRecords(resolver, "guest-sce-net.invalid", plain.data(), 5000000, 1, 0) ==
        static_cast<int>(0x804101E1) && plain == resolved);
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 && resolver_error == static_cast<int>(0x804101E1));
    for (const int flags : {0x1, 0x10000}) {
        bool threw = false;
        try { sceNetResolverStartNtoaMultipleRecords(resolver, "localhost", plain.data(), 5000000, 1, flags); }
        catch (const std::runtime_error&) { threw = true; }
        Require(threw && plain == resolved);
    }

    const std::array<std::uint8_t, 4> loopback{127, 0, 0, 1};
    char reverse[256]{};
    Require(sceNetResolverAbort(resolver, 0) == 0);
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 && resolver_error == static_cast<int>(0x804101E1));
    Require(sceNetResolverStartNtoa(resolver, "localhost", ipv4.data(), 5000000, 1, 0) == 0);
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 && resolver_error == 0);
    Require(sceNetResolverAbort(resolver, 1) == 0);
    Require(sceNetResolverAbort(resolver, 1) == 0);
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 && resolver_error == 0);
    const int reverse_result = sceNetResolverStartAton(resolver, loopback.data(), reverse, sizeof(reverse), 5000000, 1, 0);
    Require(reverse_result == 0 || reverse_result == static_cast<int>(0x804101E1));
    Require(sceNetResolverStartNtoa(resolver, nullptr, ipv4.data(), 5000000, 1, 0) == static_cast<int>(0x80410116));
    ipv4.fill(0xA5);
    Require(sceNetResolverStartNtoa(resolver, "localhost", ipv4.data(), 5000000, 1, 0) == static_cast<int>(0x80410104) &&
        *sceNetErrnoLoc() == 4 && ipv4[0] == 0xA5);
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 && resolver_error == static_cast<int>(0x80410104));
    Require(sceNetResolverStartNtoa(resolver, "localhost", ipv4.data(), 5000000, 1, 0) == 0 && ipv4[0] == 127);
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 && resolver_error == 0);
    Require(sceNetResolverAbort(resolver, 1) == 0);
    Require(sceNetResolverStartNtoaMultipleRecords(resolver, "localhost", plain.data(), 5000000, 1, 0) ==
        static_cast<int>(0x80410104) && *sceNetErrnoLoc() == 4);
    Require(sceNetResolverStartNtoaMultipleRecords(resolver, "localhost", plain.data(), 5000000, 1, 0) == 0 && plain == resolved);
    Require(sceNetResolverAbort(resolver, 2) == 0);
    Require(sceNetResolverStartNtoaMultipleRecordsEx(resolver, "localhost", records.data(), 5000000, 1, 0) == 0 && records == resolved);
    std::memset(reverse, 0x5A, sizeof(reverse));
    Require(sceNetResolverStartAton(resolver, loopback.data(), reverse, sizeof(reverse), 5000000, 1, 0) ==
        static_cast<int>(0x80410104) && *sceNetErrnoLoc() == 4 && reverse[0] == 0x5A);
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 && resolver_error == static_cast<int>(0x80410104));
    Require(sceNetResolverStartAton(resolver, loopback.data(), reverse, sizeof(reverse), 5000000, 1, 0) == reverse_result);
    Require(sceNetResolverAbort(resolver, 3) == 0);
    Require(sceNetResolverStartAton(resolver, loopback.data(), reverse, sizeof(reverse), 5000000, 1, 0) ==
        static_cast<int>(0x80410104) && *sceNetErrnoLoc() == 4);
    Require(sceNetResolverStartNtoaMultipleRecordsEx(resolver, "localhost", records.data(), 5000000, 1, 0) ==
        static_cast<int>(0x80410104) && *sceNetErrnoLoc() == 4 && records == resolved);
    Require(sceNetResolverStartNtoa(resolver, "localhost", ipv4.data(), 5000000, 1, 0) == 0);
    for (const int flags : {0x4, -1}) {
        bool threw = false;
        try { sceNetResolverAbort(resolver, flags); } catch (const std::runtime_error&) { threw = true; }
        Require(threw);
    }
    Require(sceNetResolverStartNtoa(resolver, "localhost", ipv4.data(), 5000000, 1, 0) == 0);

    const int other = sceNetResolverCreate("guest-sce-net-other", 0, 0);
    Require(other >= 0 && other != resolver);
    Require(sceNetResolverAbort(other, 3) == 0);
    Require(sceNetResolverStartNtoaMultipleRecords(resolver, "localhost", plain.data(), 5000000, 1, 0) == 0);
    Require(sceNetResolverDestroy(other) == 0);
    Require(sceNetResolverAbort(other, 0) == static_cast<int>(0x80410109) && *sceNetErrnoLoc() == 9);
    Require(sceNetResolverAbort(-1, 1) == static_cast<int>(0x80410109) && *sceNetErrnoLoc() == 9);
    Require(sceNetResolverGetError(resolver, nullptr) == static_cast<int>(0x80410116) && *sceNetErrnoLoc() == 22);
    Require(sceNetResolverDestroy(resolver) == 0);
    resolver_error = -1;
    Require(sceNetResolverGetError(resolver, &resolver_error) == static_cast<int>(0x80410109) &&
        *sceNetErrnoLoc() == 9 && resolver_error == -1);
    Require(sceNetResolverStartNtoaMultipleRecordsEx(resolver, "localhost", records.data(), 5000000, 1, 0) ==
        static_cast<int>(0x80410109) && *sceNetErrnoLoc() == 9 && records == resolved);
    Require(sceNetResolverStartNtoaMultipleRecords(resolver, "localhost", plain.data(), 5000000, 1, 0) ==
        static_cast<int>(0x80410109) && *sceNetErrnoLoc() == 9 && plain == resolved);
    Require(sceNetResolverAbort(resolver, 1) == static_cast<int>(0x80410109) && *sceNetErrnoLoc() == 9);

    std::array<std::uint8_t, 16> ipv6{};
    Require(sceNetInetPton(28, "::1", ipv6.data()) == 1);
    char text[64]{};
    Require(sceNetInetNtop(28, ipv6.data(), text, sizeof(text)) == text);
    Require(std::strcmp(text, "::1") == 0);

    CheckStreamSender(2);
    if (!ipv6Unavailable) CheckStreamSender(28);

    int state = -1;
    Require(sceNetCtlGetState(&state) == 0);
    Require(state == 0 || state == 3);
    Require(sceNetCtlGetIfStat(nullptr) == static_cast<int>(0x80412107));
    {
        const char* counterPath = "guest_sce_net_byte_counter.txt";
        std::FILE* counter = std::fopen(counterPath, "w");
        Require(counter != nullptr);
        Require(std::fputs("1099511627776", counter) >= 0);
        Require(std::fclose(counter) == 0);
        Require(NetCtlReadByteCounter_nid_no_patch(counterPath) == 1099511627776ull);
        Require(std::remove(counterPath) == 0);
        bool missing = false;
        try {
            NetCtlReadByteCounter_nid_no_patch(counterPath);
        } catch (const std::runtime_error&) {
            missing = true;
        }
        Require(missing);
    }
    NetCtlIfStat first{};
    first.device = 7;
    first.reserved[7] = 7;
    const int statResult = sceNetCtlGetIfStat(&first);
    Require(statResult == (state == 3 ? 0 : static_cast<int>(0x80412108)));
    if (statResult == 0) {
        Require(first.device == 0 && first.reserved[7] == 0);
        NetCtlIfStat second{};
        Require(sceNetCtlGetIfStat(&second) == 0);
        Require(second.tx_bytes >= first.tx_bytes && second.rx_bytes >= first.rx_bytes);
    }
    {
        const auto dummyCallback = [](int, void*) {};
        int cid = -1;
        Require(sceNetCtlRegisterCallbackV6(nullptr, nullptr, &cid) == static_cast<int>(0x80412107));
        Require(sceNetCtlRegisterCallbackV6(dummyCallback, nullptr, nullptr) == static_cast<int>(0x80412107));
        int cids[8] = {};
        for (int i = 0; i < 8; ++i) {
            Require(sceNetCtlRegisterCallbackV6(dummyCallback, nullptr, &cids[i]) == 0);
            Require(cids[i] == i);
        }
        Require(sceNetCtlRegisterCallbackV6(dummyCallback, nullptr, &cid) == static_cast<int>(0x80412103));
        Require(sceNetCtlUnregisterCallbackV6(-1) == static_cast<int>(0x80412105));
        Require(sceNetCtlUnregisterCallbackV6(8) == static_cast<int>(0x80412105));
        Require(sceNetCtlUnregisterCallbackV6(3) == 0);
        Require(sceNetCtlRegisterCallbackV6(dummyCallback, nullptr, &cid) == 0);
        Require(cid == 3);
        for (int i = 0; i < 8; ++i) {
            Require(sceNetCtlUnregisterCallbackV6(i) == 0);
        }
    }
    if (ipv6Unavailable) {
        std::puts("skipped the IPv6 socket checks, the host has no usable IPv6 loopback");
        return 77;
    }
}
