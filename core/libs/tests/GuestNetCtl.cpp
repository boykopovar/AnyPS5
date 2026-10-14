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
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>

extern "C" {
int APS5_VABI sceNetCtlCheckCallback(void);
int APS5_VABI sceNetCtlGetInfo(int, NetCtlInfo*);
int APS5_VABI sceNetCtlGetState(int*);
int APS5_VABI sceNetCtlRegisterCallback(NetCtlCallback, void*, int*);
int APS5_VABI sceNetCtlRegisterCallbackV6(NetCtlCallback, void*, int*);
void APS5_VABI sceNetCtlTerm(void);
int APS5_VABI sceNetCtlUnregisterCallback(int);
int APS5_VABI sceNetCtlUnregisterCallbackV6(int);
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
    Require(g_first.calls == 1 && g_second.calls == 1);

    Require(sceNetCtlRegisterCallbackV6(RecordV6, &v6Probe, &v6) == 0 && v6 == 0);
    Require(sceNetCtlRegisterCallback(RecordFirst, &secondProbe, &first) == 0 && first == 0);
    Require(sceNetCtlCheckCallback() == 0);
    Require(g_first.calls == 2 && g_first.event == event && g_first.arg == &secondProbe);
    Require(g_second.calls == 1 && g_self.calls == 1 && g_v6.calls == 0);
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

int main() {
#ifdef _WIN32
    WSADATA data{};
    Require(WSAStartup(MAKEWORD(2, 2), &data) == 0);
#endif
    CheckCallbacks();

    NetCtlInfo info{};
    Require(sceNetCtlGetInfo(0, &info) == kErrInvalidCode);
    Require(sceNetCtlGetInfo(22, &info) == kErrInvalidCode);
    Require(sceNetCtlGetInfo(14, nullptr) == kErrInvalidAddr);

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

    const auto address = QueryAddress(14);
    const auto host = ntohl(address.s_addr);
    Require(host != 0 && (host >> 24) != 127);
    Require(Binds(address));
    const auto mask = ntohl(QueryAddress(15).s_addr);
    Require(mask != 0 && (~mask & (~mask + 1)) == 0);
    QueryAddress(16);
    QueryAddress(17);
    QueryAddress(18);

    for (const int code : {5, 6, 7, 8, 9, 10, 12, 13, 20, 21}) {
        bool threw = false;
        try {
            sceNetCtlGetInfo(code, &info);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw);
    }
}
