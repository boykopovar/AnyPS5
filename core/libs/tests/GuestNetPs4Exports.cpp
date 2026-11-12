#include "prx/libSceNet/include/Net.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kErrorBase = static_cast<int>(0x80410100);
constexpr int kEnoent = 2;
constexpr int kEbadf = 9;
constexpr int kEinval = 22;
constexpr int kEnametoolong = 63;
constexpr int kEnotinit = 200;
constexpr int kDaemonInvalid = static_cast<int>(0x80411506u);
constexpr int kDaemonNotStarted = static_cast<int>(0x8041150Au);
constexpr int kDuplicateIpInvalid = static_cast<int>(0x80411A01u);
constexpr int kDuplicateIpNotStarted = static_cast<int>(0x80411A03u);
constexpr int kPppoeNotStarted = static_cast<int>(0x80411406u);

bool Failed(int result, int error) { return result == (kErrorBase | error) && *sceNetErrnoLoc() == error; }

std::uint64_t APS5_VABI IgnoreEvent(std::uint32_t, std::uint64_t, void*, void*) { return 0; }

std::array<char, 0x2000> g_buffer{};
std::array<std::uint8_t, 0x200> g_record{};

void CheckUninitialized() {
    NetSyncObject sync{};
    std::uint32_t flags = 0;
    std::int32_t value = 0;
    auto* connection = reinterpret_cast<NetResolverConnection*>(g_record.data());
    Require(Failed(sceNetSyncCreate(&sync, 0, "test"), kEnotinit));
    Require(Failed(sceNetSyncDestroy(&sync), kEnotinit));
    Require(Failed(sceNetSyncSignal(&sync, 1), kEnotinit));
    Require(Failed(sceNetControl(0xFFFFFFFFu, -1, 0, nullptr, 0), kEnotinit));
    Require(Failed(sceNetGetIfList(-1, nullptr, 0), kEnotinit));
    Require(Failed(sceNetGetIfListOnce(-1, nullptr, 0), kEnotinit));
    Require(Failed(sceNetGetRouteInfo(0, nullptr, 0, 0), kEnotinit));
    Require(sceNetAllocateAllRouteInfo(-1, &value) == nullptr);
    Require(Failed(sceNetFreeAllRouteInfo(nullptr), kEnotinit));
    Require(Failed(sceNetGetArpInfo(nullptr, 9), kEnotinit));
    Require(Failed(sceNetGetStatisticsInfoInternal(nullptr), kEnotinit));
    Require(Failed(sceNetEventCallbackCreate(0, nullptr, nullptr, 1), kEnotinit));
    Require(Failed(sceNetEventCallbackDestroy(0, 1), kEnotinit));
    Require(Failed(sceNetEventCallbackGetError(0, nullptr, 1), kEnotinit));
    Require(Failed(sceNetEventCallbackWaitCB(0, nullptr, 1), kEnotinit));
    Require(Failed(sceNetBandwidthControlSetDefaultParam(nullptr, 1), kEnotinit));
    Require(Failed(sceNetBandwidthControlGetDefaultParam(nullptr, 1), kEnotinit));
    Require(Failed(sceNetBandwidthControlGetDataTraffic(0, nullptr, 1), kEnotinit));
    Require(Failed(sceNetDumpCreate(nullptr, 0, 9), kEnotinit));
    Require(Failed(sceNetDumpRead(0, nullptr, -1, &flags), kEnotinit));
    Require(Failed(sceNetDumpDestroy(-1), kEnotinit));
    Require(Failed(sceNetDumpAbort(-1, 0), kEnotinit));
    Require(Failed(sceNetShowIfconfigForBuffer(g_buffer.data(), 1, 0), kEnotinit));
    Require(Failed(sceNetShowRouteForBuffer(g_buffer.data(), 1, 0), kEnotinit));
    Require(Failed(sceNetShowRoute6ForBuffer(g_buffer.data(), 1, 0), kEnotinit));
    Require(Failed(sceNetShowNetstatForBuffer(g_buffer.data(), 1, 0), kEnotinit));
    Require(Failed(sceNetShowNetstatExForBuffer(g_buffer.data(), 1, 0, 0x10000), kEnotinit));
    Require(Failed(sceNetResolverConnectCreate("test", 1, nullptr, 1), kEnotinit));
    Require(Failed(sceNetResolverConnect(nullptr, "localhost", 80, nullptr), kEnotinit));
    Require(Failed(sceNetResolverConnectDestroy(connection), kEnotinit));
    Require(Failed(sceNetResolverConnectAbort(nullptr, 2), kEnotinit));
}

void CheckDaemonsNeverStarted() {
    std::array<std::uint8_t, 0x100> info{};
    Require(sceNetDhcpGetInfo(1, reinterpret_cast<NetDhcpInfo*>(info.data())) == kDaemonNotStarted);
    Require(sceNetDhcpGetInfo(-1, nullptr) == kDaemonNotStarted);
    Require(sceNetDhcpGetAutoipInfo(1, reinterpret_cast<NetAutoIpInfo*>(info.data())) == kDaemonNotStarted);
    Require(sceNetDhcpGetInfoEx(1, 1, reinterpret_cast<NetDhcpOptionInfo*>(info.data()), 0x18) == kDaemonNotStarted);
    Require(sceNetDhcpStop(1) == kDaemonNotStarted);
    Require(sceNetDhcpStop(-1) == kDaemonNotStarted);
    Require(sceNetDhcpdStop(1) == kDaemonNotStarted);
    Require(sceNetAddrConfig6Stop(1) == kDaemonNotStarted);
    Require(sceNetAddrConfig6GetInfo(1, reinterpret_cast<NetAddress6Info*>(info.data())) == kDaemonNotStarted);
    Require(sceNetInfoDumpStop() == kDaemonNotStarted);
    Require(sceNetInfoDumpStart(0, 0, nullptr) == -1);
    Require(sceNetPppoeStop() == kPppoeNotStarted);
    Require(sceNetDuplicateIpStop(1) == kDuplicateIpInvalid);
    Require(sceNetDuplicateIpStop(3) == kDuplicateIpNotStarted);
    Require(sceNetDuplicateIpStart(1, nullptr, nullptr, 1, 0) == kDuplicateIpInvalid);
    Require(sceNetDuplicateIpStart(3, nullptr, nullptr, 0, 0) == kDuplicateIpInvalid);
    const auto* dhcpd = reinterpret_cast<const NetDhcpdStartParameter*>(info.data());
    for (const std::uint32_t interfaceId : {0u, 2u, 3u, 6u, 0xFFFFFFFFu}) Require(sceNetDhcpdStart(interfaceId, nullptr, nullptr, 0, 0, dhcpd) == kDaemonInvalid);
    Require(sceNetDhcpdStart(1, nullptr, nullptr, 0, 0, nullptr) == kDaemonInvalid);
}

void CheckDhcpOptionParser() {
    const std::array<std::uint8_t, 10> options = {53, 1, 5, 1, 4, 255, 255, 255, 0, 3};
    NetDhcpOption option{};
    option.code = 1;
    Require(libSceNetUnknown_DnB6WJ91HGg_nid_no_patch(options.data(), 10, &option) == 0);
    Require(option.value == options.data() + 5 && option.length == 4 && option.declaredLength == 4);
    option = {};
    option.code = 53;
    Require(libSceNetUnknown_DnB6WJ91HGg_nid_no_patch(options.data(), 10, &option) == 0);
    Require(option.value == options.data() + 2 && option.length == 1 && option.declaredLength == 1);
    option = {};
    option.code = 1;
    Require(libSceNetUnknown_DnB6WJ91HGg_nid_no_patch(options.data(), 6, &option) == 0);
    Require(option.length == 1 && option.declaredLength == 4);
    option = {};
    option.code = 3;
    Require(libSceNetUnknown_DnB6WJ91HGg_nid_no_patch(options.data(), 9, &option) == kDaemonInvalid);
    Require(option.length == 0 && option.declaredLength == 0);
    const std::array<std::uint8_t, 4> padded = {0, 0, 6, 1};
    option = {};
    option.code = 6;
    Require(libSceNetUnknown_DnB6WJ91HGg_nid_no_patch(padded.data(), 4, &option) == kDaemonInvalid);
    const std::array<std::uint8_t, 2> single = {12, 0};
    option = {};
    option.code = 6;
    Require(libSceNetUnknown_DnB6WJ91HGg_nid_no_patch(single.data(), 2, &option) == kDaemonInvalid);
    const std::array<std::uint8_t, 3> last = {12, 1, 7};
    option = {};
    option.code = 6;
    Require(libSceNetUnknown_DnB6WJ91HGg_nid_no_patch(last.data(), 3, &option) == kDaemonInvalid);
    Require(option.length == 0 && option.declaredLength == 0);
    option.length = 9;
    Require(libSceNetUnknown_DnB6WJ91HGg_nid_no_patch(options.data(), 1, &option) == kDaemonInvalid);
    Require(option.length == 0);
    Require(libSceNetUnknown_DnB6WJ91HGg_nid_no_patch(nullptr, 10, &option) == kDaemonInvalid);
    Require(libSceNetUnknown_DnB6WJ91HGg_nid_no_patch(options.data(), 10, nullptr) == kDaemonInvalid);
}

void CheckSync() {
    Require(sceNetSyncGet(5) == nullptr);
    Require(sceNetSyncGet(0xFFFFFFFFu) == nullptr);
    for (std::uint32_t index = 0; index < 5; ++index) {
        NetSyncObject* sync = sceNetSyncGet(index);
        Require(sync != nullptr && sync != sceNetSyncGet((index + 1) % 5));
        Require(sceNetSyncWait(sync, nullptr) == 1);
        std::uint32_t timeout = 1000;
        Require(sceNetSyncWait(sync, &timeout) == 0 && timeout == 0xFFFFFFFFu);
        Require(sceNetSyncSignal(sync, 1) == 0);
    }
    NetSyncObject sync{};
    Require(sceNetSyncCreate(&sync, 0, "GuestNetPs4Exports") == 0);
    Require(sync.mutex != nullptr && sync.condition != nullptr && sync.signals == 0);
    std::uint32_t timeout = 2000;
    Require(sceNetSyncWait(&sync, &timeout) == 0 && timeout == 0xFFFFFFFFu);
    Require(sceNetSyncSignal(&sync, 2) == 0);
    Require(sceNetSyncSignal(&sync, 4) == 0);
    timeout = 2000;
    Require(sceNetSyncWait(&sync, &timeout) == 6 && timeout == 2000);
    Require(sync.signals == 0);
    Require(sceNetSyncDestroy(&sync) == 0);
}

void CheckValidation() {
    std::uint32_t flags = 0;
    std::int32_t value = 0;
    auto* routes = reinterpret_cast<NetRouteInfo*>(g_record.data());
    auto* interfaces = reinterpret_cast<NetInterfaceInfo*>(g_record.data());
    auto* statistics = reinterpret_cast<NetInterfaceStatistics*>(g_record.data());
    auto* connection = reinterpret_cast<NetResolverConnection*>(g_record.data());
    Require(Failed(sceNetControl(0xFFFFFFFFu, -1, 0, nullptr, 0), kEinval));
    Require(Failed(sceNetControl(1, 1, 0, nullptr, 0), kEinval));
    Require(Failed(sceNetGetIfList(1, nullptr, 1), kEinval));
    Require(Failed(sceNetGetIfList(1, interfaces, 0), kEinval));
    Require(Failed(sceNetGetIfList(1, interfaces, 2), kEinval));
    Require(sceNetGetIfList(1, nullptr, 0) == 0);
    Require(Failed(sceNetGetIfListOnce(-1, statistics, 1), kEinval));
    Require(Failed(sceNetGetIfListOnce(1, nullptr, 1), kEinval));
    Require(Failed(sceNetGetIfListOnce(1, statistics, 2), kEinval));
    Require(Failed(sceNetGetRouteInfo(0, routes, 4, 0), kEinval));
    Require(Failed(sceNetGetRouteInfo(23, routes, 4, 0), kEinval));
    Require(Failed(sceNetGetRouteInfo(2, nullptr, 1, 1), kEinval));
    Require(Failed(sceNetGetRouteInfo(28, routes, 2, 1), kEinval));
    Require(sceNetFreeAllRouteInfo(nullptr) == 0);
    Require(Failed(sceNetGetArpInfo(nullptr, 4), kEinval));
    Require(Failed(sceNetGetStatisticsInfoInternal(nullptr), kEinval));
    const NetEventCallback callback = IgnoreEvent;
    Require(Failed(sceNetEventCallbackCreate(1, nullptr, nullptr, 0), kEinval));
    Require(Failed(sceNetEventCallbackCreate(1, callback, nullptr, 1), kEinval));
    Require(Failed(sceNetEventCallbackDestroy(1, 0), kEinval));
    Require(Failed(sceNetEventCallbackDestroy(1, 1), kEinval));
    Require(Failed(sceNetEventCallbackGetError(1, &value, 0), kEinval));
    Require(Failed(sceNetEventCallbackWaitCB(0, nullptr, 0), kEnoent));
    Require(Failed(sceNetEventCallbackWaitCB(0, nullptr, 1), kEinval));
    auto* bandwidth = reinterpret_cast<NetBandwidthParameter*>(g_record.data());
    Require(Failed(sceNetBandwidthControlSetDefaultParam(nullptr, 0), kEinval));
    Require(Failed(sceNetBandwidthControlSetDefaultParam(bandwidth, 1), kEinval));
    Require(Failed(sceNetBandwidthControlGetDefaultParam(nullptr, 0), kEinval));
    Require(Failed(sceNetBandwidthControlGetDefaultParam(bandwidth, 1), kEinval));
    auto* traffic = reinterpret_cast<NetDataTraffic*>(g_record.data());
    Require(Failed(sceNetBandwidthControlGetDataTraffic(1, traffic, 0), kEinval));
    Require(Failed(sceNetBandwidthControlGetDataTraffic(-1, nullptr, 0), kEinval));
    Require(Failed(sceNetBandwidthControlGetDataTraffic(-1, traffic, 1), kEinval));
    Require(Failed(sceNetDumpCreate(nullptr, 0x800, 0), kEinval));
    Require(Failed(sceNetDumpCreate(reinterpret_cast<const char*>(0x10), 0x800, 0), 14));
    Require(Failed(sceNetDumpCreate("dump", 0x7FF, 0), kEinval));
    Require(Failed(sceNetDumpCreate("dump", 0x800, 3), kEinval));
    Require(Failed(sceNetDumpCreate("dump", 0x800, 5), kEinval));
    Require(Failed(sceNetDumpCreate("0123456789abcdef0123456789abcdef", 0x800, 0), kEnametoolong));
    Require(Failed(sceNetDumpCreate("0123456789abcdef0123456789abcdef", 0, 3), kEnametoolong));
    Require(Failed(sceNetDumpRead(0, nullptr, 0, nullptr), kEinval));
    Require(Failed(sceNetDumpRead(0, g_buffer.data(), -1, nullptr), kEinval));
    Require(Failed(sceNetDumpRead(0, g_buffer.data(), 1, reinterpret_cast<std::uint32_t*>(0x10)), 14));
    flags = 0x21;
    Require(Failed(sceNetDumpRead(0, g_buffer.data(), 1, &flags), kEinval));
    const std::int32_t small = static_cast<std::int32_t>(g_buffer.size()) - 1;
    Require(Failed(sceNetShowIfconfigForBuffer(g_buffer.data(), small, 0), kEinval));
    Require(Failed(sceNetShowRouteForBuffer(g_buffer.data(), small, 0), kEinval));
    Require(Failed(sceNetShowRoute6ForBuffer(g_buffer.data(), small, 0), kEinval));
    Require(Failed(sceNetShowNetstatForBuffer(g_buffer.data(), small, 0), kEinval));
    Require(Failed(sceNetShowNetstatExForBuffer(g_buffer.data(), small, 0, 0), kEinval));
    Require(Failed(sceNetShowNetstatExForBuffer(nullptr, 0, 0, 0x10000), kEinval));
    Require(Failed(sceNetResolverConnectCreate("test", 1, nullptr, 0), kEinval));
    Require(Failed(sceNetResolverConnectCreate("test", 1, connection, 1), kEinval));
    Require(Failed(sceNetResolverConnect(nullptr, "localhost", 80, nullptr), kEinval));
    Require(Failed(sceNetResolverConnectDestroy(nullptr), kEinval));
    g_record.fill(0);
    Require(Failed(sceNetResolverConnectDestroy(connection), kEbadf));
    g_record[0x1D8] = 2;
    Require(Failed(sceNetResolverConnectDestroy(connection), kEbadf));
    g_record.fill(0);
    Require(Failed(sceNetResolverConnectAbort(nullptr, 0), kEinval));
    Require(Failed(sceNetResolverConnectAbort(connection, 2), kEinval));
}

void CheckGetIfList() {
    static_assert(sizeof(NetInterfaceInfo) == 0x1e0);
    static_assert(offsetof(NetInterfaceInfo, name) == 0x00);
    static_assert(offsetof(NetInterfaceInfo, interfaceId) == 0x20);
    static_assert(offsetof(NetInterfaceInfo, address) == 0x28);
    static_assert(offsetof(NetInterfaceInfo, broadcast) == 0x2c);
    static_assert(offsetof(NetInterfaceInfo, destination) == 0x30);
    static_assert(offsetof(NetInterfaceInfo, netmask) == 0x34);
    static_assert(offsetof(NetInterfaceInfo, flags) == 0x48);
    static_assert(offsetof(NetInterfaceInfo, flags2) == 0x4a);
    static_assert(offsetof(NetInterfaceInfo, mediaLink) == 0x4c);
    static_assert(offsetof(NetInterfaceInfo, mac) == 0x50);
    static_assert(offsetof(NetInterfaceInfo, mtu) == 0x58);
    static_assert(offsetof(NetInterfaceInfo, metric) == 0x5c);
    static_assert(offsetof(NetInterfaceInfo, counters) == 0x60);
    static_assert(offsetof(NetInterfaceInfo, address6) == 0x160);

    std::vector<NetInterfaceInfo> records(8);
    const int count = sceNetGetIfList(-1, records.data(), static_cast<std::uint32_t>(records.size()));
    Require(count >= 1);
    const NetInterfaceInfo* loopback = nullptr;
    const NetInterfaceInfo* nonLoopback = nullptr;
    for (int index = 0; index < count; ++index) {
        if (std::strcmp(records[index].name, "lo0") == 0) loopback = &records[index];
        else nonLoopback = &records[index];
    }
    Require(loopback != nullptr);
    Require(loopback->interfaceId == 2);
    const auto* loopbackBytes = reinterpret_cast<const std::uint8_t*>(&loopback->address);
    Require(loopbackBytes[0] == 127 && loopbackBytes[1] == 0 && loopbackBytes[2] == 0 && loopbackBytes[3] == 1);
    Require((loopback->flags & 0x8) != 0);
    bool loopbackMacZero = true;
    for (const std::uint8_t octet : loopback->mac) if (octet != 0) loopbackMacZero = false;
    Require(loopbackMacZero);
    for (const std::uint8_t byte : loopback->counters) Require(byte == 0);

    if (nonLoopback != nullptr) {
        Require(nonLoopback->mtu != 0 && nonLoopback->metric == nonLoopback->mtu);
        bool macPresent = false;
        for (const std::uint8_t octet : nonLoopback->mac) if (octet != 0) macPresent = true;
        Require(macPresent);
        Require((nonLoopback->flags & 0x1) != 0);
    }

    Require(sceNetGetIfList(-1, records.data(), 0) == 0);
    NetInterfaceInfo one{};
    Require(sceNetGetIfList(-1, &one, 1) == 1);

    NetInterfaceInfo lo{};
    Require(sceNetGetIfList(2, &lo, 1) == 1 && std::strcmp(lo.name, "lo0") == 0 && lo.interfaceId == 2);
    NetInterfaceInfo eth{};
    const int ethCount = sceNetGetIfList(1, &eth, 1);
    Require(ethCount == 0 || (ethCount == 1 && std::strcmp(eth.name, "eth0") == 0 && eth.interfaceId == 1));
}

}

int main() {
    CheckUninitialized();
    CheckDaemonsNeverStarted();
    CheckDhcpOptionParser();
    Require(sceNetInit_nid_postfix() == 0);
    CheckSync();
    CheckValidation();
    CheckGetIfList();
    CheckDaemonsNeverStarted();
    return 0;
}
