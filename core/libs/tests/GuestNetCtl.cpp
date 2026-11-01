#include "prx/libSceNetCtl/include/NetCtl.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <net/if.h>
#include <net/route.h>
#endif
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <thread>
#include <vector>

#ifdef __APPLE__
struct NetCtlDefaultRoute {
    char name[IF_NAMESIZE];
    in_addr gateway;
    std::uint32_t metric;
};
#endif


extern "C" {

#ifdef __APPLE__
std::size_t NetCtlDefaultRoutes_nid_no_patch(const char*, std::size_t, NetCtlDefaultRoute*, std::size_t);
#endif
void NetCtlSetStunServers_nid_no_patch(const sockaddr_in*, std::size_t, int);

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

    int stateV6 = -1;
    Require(sceNetCtlGetStateV6(&stateV6) == 0);
    const int eventV6 = stateV6 == kStateIpObtained ? kEventIpObtained : kEventDisconnected;

    int first = -1;
    Require(sceNetCtlRegisterCallback(RecordFirst, &firstProbe, &first) == 0 && first == 0);
    Require(sceNetCtlCheckCallback() == 0);
    Require(g_first.calls == 1 && g_first.event == event && g_first.arg == &firstProbe);
    Require(g_v6.calls == 1 && g_v6.event == eventV6 && g_v6.arg == &v6Probe);
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
    Require(g_second.calls == 2 && g_self.calls == 1 && g_v6.calls == 2);
    Require(g_v6.event == eventV6 && g_v6.arg == &v6Probe);
    Require(sceNetCtlUnregisterCallback(first) == 0);
    Require(sceNetCtlUnregisterCallbackV6(v6) == 0);
}

Delivery g_ipcInt;

void APS5_VABI RecordIpcInt(int event, void* arg) {
    ++g_ipcInt.calls;
    g_ipcInt.event = event;
    g_ipcInt.arg = arg;
}

int CurrentEvent() {
    int state = -1;
    Require(sceNetCtlGetState(&state) == 0);
    return state == kStateIpObtained ? kEventIpObtained : kEventDisconnected;
}

void CheckIpcIntGetters() {
    Require(sceNetCtlGetStateIpcInt(nullptr) == kErrInvalidAddr);
    Require(sceNetCtlGetStateV6IpcInt(nullptr) == kErrInvalidAddr);
    int state = -1;
    int twin = -2;
    Require(sceNetCtlGetStateIpcInt(&state) == 0 && sceNetCtlGetState(&twin) == 0 && state == twin);
    state = -1;
    twin = -2;
    Require(sceNetCtlGetStateV6IpcInt(&state) == 0 && sceNetCtlGetStateV6(&twin) == 0 && state == twin);

    Require(sceNetCtlGetResultIpcInt(kEventDisconnected, nullptr) == kErrInvalidAddr);
    Require(sceNetCtlGetResultV6IpcInt(kEventDisconnected, nullptr) == kErrInvalidAddr);
    for (const int eventType : {kEventDisconnected, 2, kEventIpObtained}) {
        int errorCode = -1;
        Require(sceNetCtlGetResultIpcInt(eventType, &errorCode) == 0 && errorCode == 0);
        errorCode = -1;
        Require(sceNetCtlGetResultV6IpcInt(eventType, &errorCode) == 0 && errorCode == 0);
    }

    NetCtlInfo info{};
    Require(sceNetCtlGetInfoIpcInt(14, nullptr) == kErrInvalidAddr);
    Require(sceNetCtlGetInfoIpcInt(0, &info) == kErrInvalidCode);
    Require(sceNetCtlGetInfoIpcInt(22, &info) == kErrInvalidCode);
    Require(sceNetCtlGetInfoIpcInt(0xffffffffu, &info) == kErrInvalidCode);
    for (std::uint32_t code = 1; code <= 21; ++code) {
        NetCtlInfo internal;
        NetCtlInfo expected;
        std::memset(&internal, kFill, sizeof(internal));
        std::memset(&expected, kFill, sizeof(expected));
        Require(sceNetCtlGetInfoIpcInt(code, &internal) == sceNetCtlGetInfo(static_cast<int>(code), &expected));
        Require(std::memcmp(&internal, &expected, sizeof(internal)) == 0);
    }

    std::uint8_t infoV6[256] = {};
    auto* v6 = reinterpret_cast<NetCtlInfoV6*>(infoV6);
    Require(sceNetCtlGetInfoV6IpcInt(0, v6) == kErrInvalidCode);
    Require(sceNetCtlGetInfoV6IpcInt(6, v6) == kErrInvalidCode);
    Require(sceNetCtlGetInfoV6IpcInt(1, nullptr) == kErrInvalidAddr);
    int stateV6 = -1;
    Require(sceNetCtlGetStateV6(&stateV6) == 0);
    if (stateV6 != kStateIpObtained) Require(sceNetCtlGetInfoV6IpcInt(1, v6) == kErrNotConnected);
}

void CheckIpcIntCallbacks() {
    constexpr int invalidId = static_cast<int>(0x80412105);
    constexpr int idNotFound = static_cast<int>(0x80412104);
    Require(sceNetCtlRegisterCallbackIpcInt(nullptr, nullptr, nullptr) == kErrInvalidAddr);
    Require(sceNetCtlRegisterCallbackV6IpcInt(nullptr, nullptr, nullptr) == kErrInvalidAddr);
    Require(sceNetCtlUnregisterCallbackIpcInt(-1) == invalidId);
    Require(sceNetCtlUnregisterCallbackV6IpcInt(-1) == invalidId);

    const int event = CurrentEvent();
    int probe = 0;
    int internal = -1;
    Require(sceNetCtlRegisterCallbackIpcInt(RecordIpcInt, &probe, &internal) == 0 && internal == 0);
    int shared = -1;
    Require(sceNetCtlRegisterCallback(RecordIpcInt, &probe, &shared) == 0 && shared == 1);
    Require(sceNetCtlCheckCallback() == 0);
    Require(g_ipcInt.calls == 2 && g_ipcInt.event == event && g_ipcInt.arg == &probe);
    Require(sceNetCtlUnregisterCallbackIpcInt(shared) == 0);
    Require(sceNetCtlUnregisterCallback(internal) == 0);
    Require(sceNetCtlUnregisterCallbackIpcInt(internal) == idNotFound);

    int stateV6 = -1;
    Require(sceNetCtlGetStateV6(&stateV6) == 0);
    const int eventV6 = stateV6 == kStateIpObtained ? kEventIpObtained : kEventDisconnected;
    int internalV6 = -1;
    Require(sceNetCtlRegisterCallbackV6IpcInt(RecordIpcInt, &probe, &internalV6) == 0 && internalV6 == 0);
    int sharedV6 = -1;
    Require(sceNetCtlRegisterCallbackV6(RecordIpcInt, &probe, &sharedV6) == 0 && sharedV6 == 1);
    Require(sceNetCtlCheckCallback() == 0);
    Require(g_ipcInt.calls == 4 && g_ipcInt.event == eventV6 && g_ipcInt.arg == &probe);
    Require(sceNetCtlCheckCallback() == 0);
    Require(g_ipcInt.calls == 4 && g_ipcInt.event == eventV6 && g_ipcInt.arg == &probe);
    Require(sceNetCtlUnregisterCallbackV6IpcInt(sharedV6) == 0);
    Require(sceNetCtlUnregisterCallbackV6(internalV6) == 0);
    Require(sceNetCtlUnregisterCallbackV6IpcInt(internalV6) == idNotFound);

    Require(sceNetCtlRegisterCallbackForLibIpcInt(nullptr, nullptr, nullptr) == kErrInvalidAddr);
    Require(sceNetCtlRegisterCallbackForLibIpcInt(RecordIpcInt, nullptr, nullptr) == kErrInvalidAddr);
    Require(sceNetCtlRegisterCallbackForLibIpcInt(nullptr, &probe, &internal) == kErrInvalidAddr);
    Require(sceNetCtlUnregisterCallbackForLibIpcInt(-1) == invalidId);
    Require(sceNetCtlUnregisterCallbackForLibIpcInt(8) == invalidId);
    Require(sceNetCtlUnregisterCallbackForLibIpcInt(0) == idNotFound);
    Require(sceNetCtlCheckCallbackForLibIpcInt() == 0);

    int forLibId = -1;
    Require(sceNetCtlRegisterCallbackForLibIpcInt(RecordIpcInt, &probe, &forLibId) == 0 && forLibId == 0);
    const int callsBefore = g_ipcInt.calls;
    int publicId = -1;
    Require(sceNetCtlRegisterCallback(RecordIpcInt, &probe, &publicId) == 0);
    Require(sceNetCtlCheckCallback() == 0);
    Require(g_ipcInt.calls == callsBefore + 1);
    int publicV6Id = -1;
    Require(sceNetCtlRegisterCallbackV6(RecordIpcInt, &probe, &publicV6Id) == 0);
    Require(sceNetCtlCheckCallbackForNpToolkit() == 0);
    Require(g_ipcInt.calls == callsBefore + 1);
    Require(sceNetCtlCheckCallbackForLibIpcInt() == 0);
    Require(g_ipcInt.calls == callsBefore + 2 && g_ipcInt.event == event && g_ipcInt.arg == &probe);
    Require(sceNetCtlCheckCallbackForLibIpcInt() == 0);
    Require(g_ipcInt.calls == callsBefore + 2);
    Require(sceNetCtlUnregisterCallbackV6(publicV6Id) == 0);
    Require(sceNetCtlUnregisterCallback(publicId) == 0);
    Require(sceNetCtlUnregisterCallbackForLibIpcInt(forLibId) == 0);
    Require(sceNetCtlUnregisterCallbackForLibIpcInt(forLibId) == idNotFound);

    int cids[8] = {};
    for (int i = 0; i < 8; ++i) {
        Require(sceNetCtlRegisterCallbackForLibIpcInt(RecordIpcInt, &probe, &cids[i]) == 0);
        Require(cids[i] == i);
    }
    int extra = -1;
    Require(sceNetCtlRegisterCallbackForLibIpcInt(RecordIpcInt, &probe, &extra) == static_cast<int>(0x80412103));
    for (int i = 0; i < 8; ++i) {
        Require(sceNetCtlUnregisterCallbackForLibIpcInt(cids[i]) == 0);
    }
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

#ifdef _WIN32
using TestSocket = SOCKET;
constexpr TestSocket kInvalidSocket = INVALID_SOCKET;
void CloseTestSocket(TestSocket descriptor) { closesocket(descriptor); }
#else
using TestSocket = int;
constexpr TestSocket kInvalidSocket = -1;
void CloseTestSocket(TestSocket descriptor) { close(descriptor); }
#endif

TestSocket OpenTestUdpSocket(sockaddr_in& bound) {
    const TestSocket descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    Require(descriptor != kInvalidSocket);
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    local.sin_port = 0;
    Require(bind(descriptor, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) == 0);
#ifdef _WIN32
    int length = sizeof(bound);
#else
    socklen_t length = sizeof(bound);
#endif
    Require(getsockname(descriptor, reinterpret_cast<sockaddr*>(&bound), &length) == 0);
    return descriptor;
}

bool WaitReadable(TestSocket descriptor, long timeoutMs) {
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(descriptor, &readSet);
    timeval tv{};
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
#ifdef _WIN32
    return select(0, &readSet, nullptr, nullptr, &tv) > 0;
#else
    return select(descriptor + 1, &readSet, nullptr, nullptr, &tv) > 0;
#endif
}

void AppendStunAddressAttribute(std::vector<std::uint8_t>& packet, std::uint16_t attrType, std::uint32_t address, std::uint16_t hostPort, bool xorMask) {
    constexpr std::uint32_t kCookie = 0x2112A442u;
    const std::uint16_t wirePort = xorMask ? static_cast<std::uint16_t>(hostPort ^ static_cast<std::uint16_t>(kCookie >> 16)) : hostPort;
    const std::uint32_t wireAddr = xorMask ? (address ^ htonl(kCookie)) : address;
    packet.push_back(static_cast<std::uint8_t>(attrType >> 8));
    packet.push_back(static_cast<std::uint8_t>(attrType & 0xFF));
    packet.push_back(0x00);
    packet.push_back(0x08);
    packet.push_back(0x00);
    packet.push_back(0x01);
    packet.push_back(static_cast<std::uint8_t>(wirePort >> 8));
    packet.push_back(static_cast<std::uint8_t>(wirePort & 0xFF));
    const auto* addrBytes = reinterpret_cast<const std::uint8_t*>(&wireAddr);
    packet.insert(packet.end(), addrBytes, addrBytes + 4);
}

using StunTxId = std::array<std::uint8_t, 12>;

StunTxId ReceiveStunBindingRequest(TestSocket serverSocket, sockaddr_in& client) {
    Require(WaitReadable(serverSocket, 2000));
    std::uint8_t request[64] = {};
#ifdef _WIN32
    int clientLen = sizeof(client);
#else
    socklen_t clientLen = sizeof(client);
#endif
    const auto received = recvfrom(serverSocket, reinterpret_cast<char*>(request), sizeof(request), 0,
                                   reinterpret_cast<sockaddr*>(&client), &clientLen);
    Require(received == 20);
    Require(request[0] == 0x00 && request[1] == 0x01 && request[2] == 0x00 && request[3] == 0x00);
    const std::uint32_t expectedCookie = htonl(0x2112A442u);
    Require(std::memcmp(request + 4, &expectedCookie, sizeof(expectedCookie)) == 0);
    StunTxId txId{};
    std::memcpy(txId.data(), request + 8, txId.size());
    Require(txId != StunTxId{});
    return txId;
}

void SendStunBindingResponse(TestSocket serverSocket, const sockaddr_in& client, const StunTxId& txId, std::uint32_t mappedAddr, std::uint16_t mappedHostPort, bool xorMapped, const sockaddr_in* otherServer = nullptr) {
    const std::uint16_t effectivePort = mappedHostPort != 0 ? mappedHostPort : ntohs(client.sin_port);
    const std::uint32_t effectiveAddr = mappedAddr != 0 ? mappedAddr : client.sin_addr.s_addr;
    std::vector<std::uint8_t> response(20, 0);
    response[0] = 0x01;
    response[1] = 0x01;
    const std::uint32_t cookie = htonl(0x2112A442u);
    std::memcpy(response.data() + 4, &cookie, sizeof(cookie));
    std::memcpy(response.data() + 8, txId.data(), txId.size());
    AppendStunAddressAttribute(response, xorMapped ? 0x0020 : 0x0001, effectiveAddr, effectivePort, xorMapped);
    if (otherServer) {
        AppendStunAddressAttribute(response, 0x0005, otherServer->sin_addr.s_addr, ntohs(otherServer->sin_port), false);
    }
    const auto attrLen = static_cast<std::uint16_t>(response.size() - 20);
    response[2] = static_cast<std::uint8_t>(attrLen >> 8);
    response[3] = static_cast<std::uint8_t>(attrLen & 0xFF);
#ifdef _WIN32
    const int clientLen = sizeof(client);
#else
    const socklen_t clientLen = sizeof(client);
#endif
    const auto sent = sendto(serverSocket, reinterpret_cast<const char*>(response.data()), static_cast<int>(response.size()), 0,
                             reinterpret_cast<const sockaddr*>(&client), clientLen);
    Require(sent == static_cast<int>(response.size()));
}

void RecordUniqueTransactionId(std::vector<StunTxId>& seen, const StunTxId& candidate) {
    for (const auto& existing : seen) Require(existing != candidate);
    seen.push_back(candidate);
}

void CheckNatDiscovery(in_addr address) {
    sockaddr_in server1Addr{};
    sockaddr_in server2Addr{};
    const TestSocket server1 = OpenTestUdpSocket(server1Addr);
    const TestSocket server2 = OpenTestUdpSocket(server2Addr);
    std::vector<StunTxId> seenIds;

    NetCtlNatInfo natInfo{};
    natInfo.size = sizeof(natInfo);
    natInfo.stun_status = 99;
    natInfo.nat_type = 99;
    natInfo.mapped_addr = 0xFFFFFFFFu;
    NetCtlSetStunServers_nid_no_patch(&server1Addr, 1, 40);
    Require(sceNetCtlGetNatInfo(&natInfo) == 0);
    Require(natInfo.size == sizeof(natInfo));
    Require(natInfo.stun_status == 1);
    Require(natInfo.nat_type == 0);
    Require(natInfo.mapped_addr == 0);
    {
        sockaddr_in from{};
        RecordUniqueTransactionId(seenIds, ReceiveStunBindingRequest(server1, from));
    }

    StunTxId openTx{};
    std::thread openWorker([&] {
        sockaddr_in client{};
        openTx = ReceiveStunBindingRequest(server1, client);
        SendStunBindingResponse(server1, client, openTx, address.s_addr, 0, true);
    });
    natInfo = {sizeof(natInfo), 99, 99, 0};
    Require(sceNetCtlGetNatInfo(&natInfo) == 0);
    openWorker.join();
    RecordUniqueTransactionId(seenIds, openTx);
    Require(natInfo.size == sizeof(natInfo));
    Require(natInfo.stun_status == 2);
    Require(natInfo.nat_type == 1);
    Require(natInfo.mapped_addr == address.s_addr);

    const std::uint32_t externalIp = (ntohl(address.s_addr) == 0xCB00712Au) ? htonl(0xCB00712Bu) : htonl(0xCB00712Au);
    const std::uint32_t spoofedIp = htonl(0xC0000201u);

    NetCtlSetStunServers_nid_no_patch(&server1Addr, 1, 30);
    StunTxId mismatchedOnlyTx{};
    std::thread mismatchOnlyWorker([&] {
        sockaddr_in client{};
        mismatchedOnlyTx = ReceiveStunBindingRequest(server1, client);
        StunTxId badTx = mismatchedOnlyTx;
        badTx[0] ^= 0xFFu;
        SendStunBindingResponse(server1, client, badTx, spoofedIp, 54321, true);
    });
    natInfo = {sizeof(natInfo), 99, 99, 0xFFFFFFFFu};
    Require(sceNetCtlGetNatInfo(&natInfo) == 0);
    mismatchOnlyWorker.join();
    RecordUniqueTransactionId(seenIds, mismatchedOnlyTx);
    Require(natInfo.size == sizeof(natInfo));
    Require(natInfo.stun_status == 1);
    Require(natInfo.nat_type == 0);
    Require(natInfo.mapped_addr == 0);

    NetCtlSetStunServers_nid_no_patch(&server1Addr, 1, 1000);
    StunTxId incompleteTx{};
    std::thread incompleteWorker([&] {
        sockaddr_in client{};
        incompleteTx = ReceiveStunBindingRequest(server1, client);
        SendStunBindingResponse(server1, client, incompleteTx, externalIp, 54321, true);
    });
    natInfo = {sizeof(natInfo), 99, 99, 0xFFFFFFFFu};
    Require(sceNetCtlGetNatInfo(&natInfo) == 0);
    incompleteWorker.join();
    RecordUniqueTransactionId(seenIds, incompleteTx);
    Require(natInfo.size == sizeof(natInfo));
    Require(natInfo.stun_status == 1);
    Require(natInfo.nat_type == 0);
    Require(natInfo.mapped_addr == 0);

    const sockaddr_in bothServers[2] = {server1Addr, server2Addr};
    NetCtlSetStunServers_nid_no_patch(bothServers, 2, 1000);
    StunTxId type2Tx1{};
    StunTxId type2Tx2{};
    std::thread type2Worker([&] {
        sockaddr_in firstClient{};
        type2Tx1 = ReceiveStunBindingRequest(server1, firstClient);
        SendStunBindingResponse(server1, firstClient, type2Tx1, externalIp, 54321, true);
        sockaddr_in secondClient{};
        type2Tx2 = ReceiveStunBindingRequest(server2, secondClient);
        Require(firstClient.sin_addr.s_addr == secondClient.sin_addr.s_addr);
        Require(firstClient.sin_port == secondClient.sin_port);
        StunTxId wrongTx = type2Tx2;
        wrongTx[11] ^= 0xA5u;
        SendStunBindingResponse(server2, secondClient, wrongTx, spoofedIp, 59999, true);
        SendStunBindingResponse(server2, secondClient, type2Tx2, externalIp, 54321, true);
    });
    natInfo = {sizeof(natInfo), 99, 99, 0};
    Require(sceNetCtlGetNatInfo(&natInfo) == 0);
    type2Worker.join();
    RecordUniqueTransactionId(seenIds, type2Tx1);
    RecordUniqueTransactionId(seenIds, type2Tx2);
    Require(natInfo.size == sizeof(natInfo));
    Require(natInfo.stun_status == 2);
    Require(natInfo.nat_type == 2);
    Require(natInfo.mapped_addr == externalIp);

    NetCtlNatInfo cachedInfo{sizeof(cachedInfo), 99, 99, 0};
    Require(sceNetCtlGetNatInfo(&cachedInfo) == 0);
    Require(cachedInfo.stun_status == 2 && cachedInfo.nat_type == 2 && cachedInfo.mapped_addr == externalIp);

    NetCtlSetStunServers_nid_no_patch(&server1Addr, 1, 1000);
    StunTxId type3Tx1{};
    StunTxId type3Tx2{};
    std::thread type3Worker([&] {
        sockaddr_in firstClient{};
        type3Tx1 = ReceiveStunBindingRequest(server1, firstClient);
        SendStunBindingResponse(server1, firstClient, type3Tx1, externalIp, 54321, false, &server2Addr);
        sockaddr_in secondClient{};
        type3Tx2 = ReceiveStunBindingRequest(server2, secondClient);
        Require(firstClient.sin_addr.s_addr == secondClient.sin_addr.s_addr);
        Require(firstClient.sin_port == secondClient.sin_port);
        SendStunBindingResponse(server2, secondClient, type3Tx2, externalIp, 54322, false);
    });
    natInfo = {sizeof(natInfo), 99, 99, 0};
    Require(sceNetCtlGetNatInfo(&natInfo) == 0);
    type3Worker.join();
    RecordUniqueTransactionId(seenIds, type3Tx1);
    RecordUniqueTransactionId(seenIds, type3Tx2);
    Require(natInfo.size == sizeof(natInfo));
    Require(natInfo.stun_status == 2);
    Require(natInfo.nat_type == 3);
    Require(natInfo.mapped_addr == externalIp);

    NetCtlSetStunServers_nid_no_patch(nullptr, 0, 0);
    CloseTestSocket(server1);
    CloseTestSocket(server2);
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
    Require(sceNetCtlRegisterCallbackForLibIpcInt(nullptr, nullptr, nullptr) == kErrInvalidAddr);
    Require(sceNetCtlUnregisterCallbackForLibIpcInt(-1) == invalidId);
    Require(sceNetCtlCheckCallbackForLibIpcInt() == 0);
    CheckCallbacks();
    CheckIpcIntGetters();
    CheckIpcIntCallbacks();

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
    CheckNatDiscovery(address);
}
