#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
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
int APS5_VABI sceNetSetsockopt(int, int, int, const void*, std::uint32_t);
int* APS5_VABI sceNetErrnoLoc(void);
int APS5_VABI sceNetEpollCreate(const char*, int);
int APS5_VABI sceNetEpollControl(int, int, int, const NetEpollEvent*);
int APS5_VABI sceNetEpollWait(int, NetEpollEvent*, int, int);
int APS5_VABI sceNetEpollDestroy(int);
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

static bool ipv6Unavailable = false;

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
    Require(sceNetSendto(sender, payload, sizeof(payload), 0, address.data(), address.size()) == sizeof(payload));
    char received[sizeof(payload)]{};
    std::array<std::uint8_t, 28> peer{};
    size = peer.size();
    Require(sceNetRecvfrom(receiver, received, sizeof(received), 0, peer.data(), &size) == sizeof(received));
    Require(std::memcmp(received, payload, sizeof(payload)) == 0 && size == peer.size() && peer[1] == 28);
    Require(std::memcmp(peer.data() + 8, address.data() + 8, 16) == 0);
    Require(std::memcmp(sce_net_in6addr_any, unspecified.data(), unspecified.size()) == 0);
    Require(sceNetSocketClose(sender) == 0);
    Require(sceNetSocketClose(receiver) == 0);
}

int main() {
    for (int i = 0; i < 16; ++i) {
        Require(in6addr_any_nid_postfix[i] == 0);
        Require(in6addr_loopback_nid_postfix[i] == (i == 15 ? 1 : 0));
    }
    Require(sceNetInit_nid_postfix() == 0);
    CheckPoolStats();
    CheckAddressText(2, "127.0.0.1");
    CheckAddressText(2, "255.255.255.255");
    CheckAddressText(28, "::1");
    CheckAddressText(28, "1234:5678:9abc:def0:1234:5678:9abc:def0");
    CheckUnspecifiedIpv6();

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

    const int epoll = sceNetEpollCreate("guest-sce-net", 0);
    Require(epoll >= 0);
    NetEpollEvent registration{};
    registration.events = 1;
    registration.ident = static_cast<std::uint64_t>(accepted);
    registration.data.u32 = 77;
    Require(sceNetEpollControl(epoll, 1, accepted, &registration) == 0);
    const char request[] = "guest tcp loopback";
    char response[sizeof(request)]{};
    Require(sceNetSend(client, request, sizeof(request), 0) == sizeof(request));
    NetEpollEvent ready{};
    const int ready_count = sceNetEpollWait(epoll, &ready, 1, 1000000);
    Require(ready_count == 1 && (ready.events & 1) && ready.ident == static_cast<std::uint64_t>(accepted));
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
    Require(sceNetSocketClose(listener) == static_cast<int>(0x80410109) && *sceNetErrnoLoc() == 9);

    const int udp_receiver = sceNetSocket(nullptr, 2, 2, 17);
    const int udp_sender = sceNetSocket(nullptr, 2, 2, 17);
    Require(udp_receiver >= 0 && udp_sender >= 0);
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
    CheckMessages(udp_receiver, udp_sender, address);
    Require(sceNetSocketClose(udp_sender) == 0);
    Require(sceNetSocketClose(udp_receiver) == 0);

    const int resolver = sceNetResolverCreate("guest-sce-net", 0, 0);
    Require(resolver >= 0);
    int resolver_error = -1;
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 && resolver_error == 0);
    std::array<std::uint8_t, 4> ipv4{};
    Require(sceNetResolverStartNtoa(resolver, "guest-sce-net.invalid", ipv4.data(), 5000000, 1, 0) ==
        static_cast<int>(0x804101E1));
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 &&
        resolver_error == static_cast<int>(0x804101E1));
    Require(sceNetResolverStartNtoa(resolver, nullptr, ipv4.data(), 5000000, 1, 0) == static_cast<int>(0x80410116));
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 &&
        resolver_error == static_cast<int>(0x804101E1));
    Require(sceNetResolverStartNtoa(resolver, "localhost", ipv4.data(), 5000000, 1, 0) == 0);
    Require(ipv4[0] == 127);
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
    Require(sceNetResolverStartNtoaMultipleRecordsEx(resolver, "guest-sce-net.invalid", records.data(), 5000000, 1, 0) ==
        static_cast<int>(0x804101E1) && records == resolved);
    Require(sceNetResolverGetError(resolver, nullptr) == static_cast<int>(0x80410116) && *sceNetErrnoLoc() == 22);
    Require(sceNetResolverDestroy(resolver) == 0);
    resolver_error = -1;
    Require(sceNetResolverGetError(resolver, &resolver_error) == static_cast<int>(0x80410109) &&
        *sceNetErrnoLoc() == 9 && resolver_error == -1);
    Require(sceNetResolverStartNtoaMultipleRecordsEx(resolver, "localhost", records.data(), 5000000, 1, 0) ==
        static_cast<int>(0x80410109) && *sceNetErrnoLoc() == 9 && records == resolved);

    std::array<std::uint8_t, 16> ipv6{};
    Require(sceNetInetPton(28, "::1", ipv6.data()) == 1);
    char text[64]{};
    Require(sceNetInetNtop(28, ipv6.data(), text, sizeof(text)) == text);
    Require(std::strcmp(text, "::1") == 0);

    int state = -1;
    Require(sceNetCtlGetState(&state) == 0);
    Require(state == 0 || state == 3);
    if (ipv6Unavailable) {
        std::puts("skipped the IPv6 socket checks, the host has no IPv6");
        return 77;
    }
}
