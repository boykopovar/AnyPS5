#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <net/if.h>
#include <net/route.h>
#include <vector>
#endif
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>

#ifdef __APPLE__
struct NetCtlDefaultRoute {
    char name[IF_NAMESIZE];
    in_addr gateway;
    std::uint32_t metric;
};
#endif

union NetCtlInfoV6;

extern "C" {
int APS5_VABI sceNetCtlInit(void);
int APS5_VABI sceNetCtlCheckCallback(void);
int APS5_VABI sceNetCtlGetInfo(int, NetCtlInfo*);
int APS5_VABI sceNetCtlGetNatInfo(NetCtlNatInfo*);
int APS5_VABI sceNetCtlGetState(int*);
int APS5_VABI sceNetCtlGetStateV6(int*);
int APS5_VABI sceNetCtlGetInfoV6(std::uint32_t, NetCtlInfoV6*);
int APS5_VABI sceNetCtlGetResult(int, int*);
int APS5_VABI sceNetCtlGetResultV6(int, int*);
#ifdef __APPLE__
std::size_t NetCtlDefaultRoutes_nid_no_patch(const char*, std::size_t, NetCtlDefaultRoute*, std::size_t);
#endif
int APS5_VABI sceNetCtlRegisterCallback(NetCtlCallback, void*, int*);
int APS5_VABI sceNetCtlRegisterCallbackV6(NetCtlCallback, void*, int*);
void APS5_VABI sceNetCtlTerm(void);
int APS5_VABI sceNetCtlUnregisterCallback(int);
int APS5_VABI sceNetCtlUnregisterCallbackV6(int);
int APS5_VABI sceNetCtlRegisterCallbackForNpToolkit(NetCtlCallback, void*, int*);
int APS5_VABI sceNetCtlUnregisterCallbackForNpToolkit(int);
int APS5_VABI sceNetCtlCheckCallbackForNpToolkit(void);
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kErrInvalidCode = static_cast<int>(0x80412106);
constexpr int kErrInvalidAddr = static_cast<int>(0x80412107);
constexpr int kErrNotConnected = static_cast<int>(0x80412108);
constexpr int kStateIpObtained = 3;
constexpr int kEventDisconnected = 1;
constexpr int kEventIpObtained = 3;
constexpr std::uint8_t kFill = 0xa5;
constexpr std::size_t kAddressLength = 16;

struct Delivery {
    int calls = 0;
    int event = 0;
    void* arg = nullptr;
};

Delivery g_first;
Delivery g_second;
Delivery g_v6;
Delivery g_self;
int g_selfUnregister = -1;

void APS5_VABI RecordFirst(int event, void* arg) {
    ++g_first.calls;
    g_first.event = event;
    g_first.arg = arg;
}

void APS5_VABI RecordSecond(int event, void* arg) {
    ++g_second.calls;
    g_second.event = event;
    g_second.arg = arg;
}

void APS5_VABI RecordV6(int event, void* arg) {
    ++g_v6.calls;
    g_v6.event = event;
    g_v6.arg = arg;
}

void APS5_VABI UnregisterSelf(int event, void* arg) {
    ++g_self.calls;
    g_self.event = event;
    g_self.arg = arg;
    g_selfUnregister = sceNetCtlUnregisterCallback(*static_cast<int*>(arg));
}

void CheckCallbacks() {
    int state = -1;
    Require(sceNetCtlGetState(&state) == 0);
    const int event = state == kStateIpObtained ? kEventIpObtained : kEventDisconnected;
    int firstProbe = 0;
    int secondProbe = 0;
    int v6Probe = 0;

    int v6 = -1;
    Require(sceNetCtlRegisterCallbackV6(RecordV6, &v6Probe, &v6) == 0 && v6 == 0);

    int first = -1;
    Require(sceNetCtlRegisterCallback(RecordFirst, &firstProbe, &first) == 0 && first == 0);
    Require(sceNetCtlCheckCallback() == 0);
    Require(g_first.calls == 1 && g_first.event == event && g_first.arg == &firstProbe);
    Require(sceNetCtlCheckCallback() == 0);
    Require(g_first.calls == 1);

    int second = -1;
    Require(sceNetCtlRegisterCallback(RecordSecond, &secondProbe, &second) == 0 && second == 1);
    Require(sceNetCtlCheckCallback() == 0);
    Require(g_first.calls == 1);
    Require(g_second.calls == 1 && g_second.event == event && g_second.arg == &secondProbe);

    Require(sceNetCtlUnregisterCallback(second) == 0);
    Require(sceNetCtlRegisterCallback(RecordSecond, &secondProbe, &second) == 0 && second == 1);
    Require(sceNetCtlUnregisterCallback(second) == 0);
    Require(sceNetCtlCheckCallback() == 0);
    Require(g_first.calls == 1 && g_second.calls == 1);

    int self = -1;
    Require(sceNetCtlRegisterCallback(UnregisterSelf, &self, &self) == 0 && self == 1);
    Require(sceNetCtlCheckCallback() == 0);
    Require(g_self.calls == 1 && g_self.event == event && g_self.arg == &self && g_selfUnregister == 0);
    Require(g_first.calls == 1 && g_second.calls == 1);
    Require(sceNetCtlCheckCallback() == 0);
    Require(g_self.calls == 1);

    Require(sceNetCtlRegisterCallback(RecordSecond, &secondProbe, &second) == 0 && second == 1);
    sceNetCtlTerm();
    Require(sceNetCtlCheckCallback() == 0);
    Require(g_first.calls == 1 && g_second.calls == 2);
    Require(sceNetCtlInit() == 0);
    Require(sceNetCtlUnregisterCallback(first) == 0);
    Require(sceNetCtlUnregisterCallback(second) == 0);
    Require(sceNetCtlUnregisterCallbackV6(v6) == 0);
    Require(sceNetCtlUnregisterCallback(first) == static_cast<int>(0x80412104));
    Require(sceNetCtlUnregisterCallbackV6(v6) == static_cast<int>(0x80412104));

    Require(sceNetCtlRegisterCallbackV6(RecordV6, &v6Probe, &v6) == 0 && v6 == 0);
    Require(sceNetCtlRegisterCallback(RecordFirst, &secondProbe, &first) == 0 && first == 0);
    Require(sceNetCtlCheckCallback() == 0);
    Require(g_first.calls == 2 && g_first.event == event && g_first.arg == &secondProbe);
    Require(g_second.calls == 2 && g_self.calls == 1 && g_v6.calls == 0);
    Require(sceNetCtlUnregisterCallback(first) == 0);
    Require(sceNetCtlUnregisterCallbackV6(v6) == 0);
}

NetCtlInfo Query(int code) {
    NetCtlInfo info;
    std::memset(&info, kFill, sizeof(info));
    Require(sceNetCtlGetInfo(code, &info) == 0);
    return info;
}

void RequireUntouchedFrom(const NetCtlInfo& info, std::size_t offset) {
    for (std::size_t i = offset; i < sizeof(info.opaque); ++i) Require(info.opaque[i] == kFill);
}

std::uint32_t QueryWord(int code) {
    const auto info = Query(code);
    std::uint32_t value = 0;
    std::memcpy(&value, info.opaque, sizeof(value));
    RequireUntouchedFrom(info, sizeof(value));
    return value;
}

in_addr QueryAddress(int code) {
    const auto info = Query(code);
    char text[kAddressLength];
    std::memcpy(text, info.opaque, sizeof(text));
    Require(std::memchr(text, '\0', sizeof(text)) != nullptr);
    RequireUntouchedFrom(info, kAddressLength);
    in_addr address{};
    Require(inet_pton(AF_INET, text, &address) == 1);
    return address;
}

bool Binds(in_addr address) {
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr = address;
#ifdef _WIN32
    const SOCKET descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    Require(descriptor != INVALID_SOCKET);
    const bool bound = bind(descriptor, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) == 0;
    closesocket(descriptor);
#else
    const int descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    Require(descriptor >= 0);
    const bool bound = bind(descriptor, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) == 0;
    close(descriptor);
#endif
    return bound;
}

}

#ifdef __APPLE__
void AppendRoute(std::vector<char>& table, std::uint32_t destination, std::uint32_t gateway, int flags, unsigned short index) {
    rt_msghdr header{};
    header.rtm_msglen = static_cast<unsigned short>(sizeof(header) + 2 * sizeof(sockaddr_in) + sizeof(std::uint32_t));
    header.rtm_version = RTM_VERSION;
    header.rtm_type = RTM_GET;
    header.rtm_index = index;
    header.rtm_flags = flags;
    header.rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK;
    sockaddr_in addresses[2]{};
    for (auto& address : addresses) {
        address.sin_len = sizeof(address);
        address.sin_family = AF_INET;
    }
    addresses[0].sin_addr.s_addr = htonl(destination);
    addresses[1].sin_addr.s_addr = htonl(gateway);
    const std::uint32_t emptyMask = 0;
    const auto* bytes = reinterpret_cast<const char*>(&header);
    table.insert(table.end(), bytes, bytes + sizeof(header));
    bytes = reinterpret_cast<const char*>(addresses);
    table.insert(table.end(), bytes, bytes + sizeof(addresses));
    bytes = reinterpret_cast<const char*>(&emptyMask);
    table.insert(table.end(), bytes, bytes + sizeof(emptyMask));
}

void CheckDefaultRoutes() {
    const auto loopback = static_cast<unsigned short>(if_nametoindex("lo0"));
    Require(loopback != 0);
    constexpr int up = RTF_UP | RTF_GATEWAY | RTF_STATIC;
    std::vector<char> table;
    AppendRoute(table, 0, 0x0A000001, up, loopback);
    AppendRoute(table, 0x01020304, 0x0A000009, up | RTF_HOST, loopback);
    AppendRoute(table, 0, 0x0A000009, up | RTF_HOST, loopback);
    AppendRoute(table, 0, 0x0A000009, up | RTF_REJECT, loopback);
    AppendRoute(table, 0, 0x0A000002, up | RTF_IFSCOPE, loopback);
    AppendRoute(table, 0, 0x0A000009, RTF_GATEWAY | RTF_STATIC, loopback);
    AppendRoute(table, 0, 0x0A000009, up, 0xFFFF);
    const std::size_t complete = table.size();
    AppendRoute(table, 0, 0x0A000009, up, loopback);
    table.resize(table.size() - sizeof(std::uint32_t));

    NetCtlDefaultRoute routes[4]{};
    Require(NetCtlDefaultRoutes_nid_no_patch(table.data(), table.size(), routes, 4) == 2);
    Require(std::strcmp(routes[0].name, "lo0") == 0 && ntohl(routes[0].gateway.s_addr) == 0x0A000001 && routes[0].metric == 0);
    Require(std::strcmp(routes[1].name, "lo0") == 0 && ntohl(routes[1].gateway.s_addr) == 0x0A000002 && routes[1].metric == 1);

    NetCtlDefaultRoute first[1]{};
    Require(NetCtlDefaultRoutes_nid_no_patch(table.data(), complete, first, 1) == 2);
    Require(ntohl(first[0].gateway.s_addr) == 0x0A000001);
    Require(NetCtlDefaultRoutes_nid_no_patch(table.data(), 0, nullptr, 0) == 0);
}
#endif

int main() {
#ifdef _WIN32
    WSADATA data{};
    Require(WSAStartup(MAKEWORD(2, 2), &data) == 0);
#endif
#ifdef __APPLE__
    CheckDefaultRoutes();
#endif
    constexpr int invalidId = static_cast<int>(0x80412105);
    Require(sceNetCtlCheckCallback() == 0);
    Require(sceNetCtlGetState(nullptr) == kErrInvalidAddr);
    Require(sceNetCtlGetStateV6(nullptr) == kErrInvalidAddr);
    Require(sceNetCtlGetInfo(0, nullptr) == kErrInvalidAddr);
    Require(sceNetCtlGetResult(0, nullptr) == kErrInvalidAddr);
    Require(sceNetCtlGetNatInfo(nullptr) == kErrInvalidAddr);
    Require(sceNetCtlRegisterCallback(nullptr, nullptr, nullptr) == kErrInvalidAddr);
    Require(sceNetCtlRegisterCallbackV6(nullptr, nullptr, nullptr) == kErrInvalidAddr);
    Require(sceNetCtlUnregisterCallback(-1) == invalidId);
    Require(sceNetCtlUnregisterCallbackV6(-1) == invalidId);
    Require(sceNetCtlRegisterCallbackForNpToolkit(nullptr, nullptr, nullptr) == kErrInvalidAddr);
    Require(sceNetCtlUnregisterCallbackForNpToolkit(-1) == invalidId);
    Require(sceNetCtlCheckCallbackForNpToolkit() == 0);
    CheckCallbacks();

    Require(sceNetCtlGetStateV6(nullptr) == kErrInvalidAddr);
    int stateV6 = -1;
    Require(sceNetCtlGetStateV6(&stateV6) == 0);
    Require(stateV6 == 0 || stateV6 == kStateIpObtained);
    Require(sceNetCtlGetResult(0, nullptr) == kErrInvalidAddr);
    Require(sceNetCtlGetResult(kEventDisconnected, nullptr) == kErrInvalidAddr);
    for (const int eventType : {kEventDisconnected, 2, kEventIpObtained}) {
        int errorCode = -1;
        Require(sceNetCtlGetResult(eventType, &errorCode) == 0 && errorCode == 0);
    }
    Require(sceNetCtlGetResultV6(kEventIpObtained, nullptr) == kErrInvalidAddr);
    for (const int eventType : {kEventDisconnected, 2, kEventIpObtained}) {
        int errorCodeV6 = -1;
        Require(sceNetCtlGetResultV6(eventType, &errorCodeV6) == 0 && errorCodeV6 == 0);
    }
    NetCtlInfo info{};
    Require(sceNetCtlGetInfo(0, &info) == kErrInvalidCode);
    Require(sceNetCtlGetInfo(22, &info) == kErrInvalidCode);
    Require(sceNetCtlGetInfo(14, nullptr) == kErrInvalidAddr);

    std::uint8_t infoV6[256] = {};
    Require(sceNetCtlGetInfoV6(0, reinterpret_cast<NetCtlInfoV6*>(infoV6)) == kErrInvalidCode);
    Require(sceNetCtlGetInfoV6(6, reinterpret_cast<NetCtlInfoV6*>(infoV6)) == kErrInvalidCode);
    Require(sceNetCtlGetInfoV6(1, nullptr) == kErrInvalidAddr);
    if (stateV6 != kStateIpObtained) Require(sceNetCtlGetInfoV6(1, reinterpret_cast<NetCtlInfoV6*>(infoV6)) == kErrNotConnected);

    const int result = sceNetCtlGetInfo(14, &info);
    if (result == kErrNotConnected) {
        std::puts("skipped, the host has no IPv4 address");
        return 77;
    }
    Require(result == 0);
    int state = -1;
    Require(sceNetCtlGetState(&state) == 0 && state == kStateIpObtained);

    Require(QueryWord(1) == 0);
    const auto ether = Query(2);
    const std::uint8_t mac[6] = {0x02, 0x50, 0x53, 0x35, 0x00, 0x01};
    Require(std::memcmp(ether.opaque, mac, sizeof(mac)) == 0);
    RequireUntouchedFrom(ether, sizeof(mac));
    const auto mtu = QueryWord(3);
    Require(mtu >= 68 && mtu <= 65535);
    Require(QueryWord(4) == 1);
    Require(QueryWord(11) == 0);
    Require(QueryWord(19) == 0);
    Require(QueryWord(21) == 0);

    const auto address = QueryAddress(14);
    const auto host = ntohl(address.s_addr);
    Require(host != 0 && (host >> 24) != 127);
    Require(Binds(address));
    const auto mask = ntohl(QueryAddress(15).s_addr);
    Require(mask != 0 && (~mask & (~mask + 1)) == 0);
    QueryAddress(16);
    QueryAddress(17);
    QueryAddress(18);

    for (const int code : {5, 6, 12, 13, 20}) {
        std::memset(&info, 0x5a, sizeof(info));
        Require(sceNetCtlGetInfo(code, &info) == 0 && info.opaque[0] == 0);
    }

    for (const int code : {7, 8, 9, 10}) {
        Require(QueryWord(code) == 0);
    }

    Require(sceNetCtlGetNatInfo(nullptr) == kErrInvalidAddr);
    NetCtlNatInfo natInfo{};
    for (const auto size : {0u, 15u, 17u, 128u, 0xffffffffu}) {
        natInfo.size = size;
        Require(sceNetCtlGetNatInfo(&natInfo) == static_cast<int>(0x80412111));
    }
    natInfo.size = sizeof(natInfo);
    std::memset(natInfo.opaque, kFill, sizeof(natInfo.opaque));
    bool natUnsupported = false;
    try {
        sceNetCtlGetNatInfo(&natInfo);
    } catch (const std::logic_error&) {
        natUnsupported = true;
    }
    Require(natUnsupported && natInfo.size == sizeof(natInfo));
    for (const auto value : natInfo.opaque) Require(value == kFill);
}
