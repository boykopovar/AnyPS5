#include "prx/libkernel/Socket/include/LanRelay.hpp"
#include <cstdint>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
using SocketHandle = SOCKET;
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketHandle = int;
#endif

namespace LanRelay {
namespace {

struct HostEntry {
    std::uint32_t Index = 0;
    std::uint32_t Address = 0;
    std::uint32_t Netmask = 0;
    std::uint32_t NetworkAddress = 0;
};

std::vector<HostEntry> HostIpv4Entries() {
    std::vector<HostEntry> entries;
#ifdef _WIN32
    const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG size = 0;
    if (GetAdaptersAddresses(AF_INET, flags, nullptr, nullptr, &size) != ERROR_BUFFER_OVERFLOW) return entries;
    std::vector<std::uint8_t> storage(size);
    auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data());
    if (GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &size) != NO_ERROR) return entries;
    for (const auto* adapter = adapters; adapter != nullptr; adapter = adapter->Next) {
        for (const auto* unicast = adapter->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next) {
            if (unicast->Address.lpSockaddr == nullptr || unicast->Address.lpSockaddr->sa_family != AF_INET) continue;
            const auto* address = reinterpret_cast<const sockaddr_in*>(unicast->Address.lpSockaddr);
            const std::uint32_t prefix = unicast->OnLinkPrefixLength;
            const std::uint32_t netmask = prefix == 0 ? 0 : prefix >= 32 ? 0xFFFFFFFFu : ~((1u << (32 - prefix)) - 1u);
            entries.push_back(HostEntry{adapter->IfIndex, ntohl(address->sin_addr.s_addr), netmask, address->sin_addr.s_addr});
        }
    }
#else
    ifaddrs* list = nullptr;
    if (getifaddrs(&list) != 0) return entries;
    for (const ifaddrs* entry = list; entry != nullptr; entry = entry->ifa_next) {
        if (entry->ifa_addr == nullptr || entry->ifa_netmask == nullptr || entry->ifa_addr->sa_family != AF_INET) continue;
        const auto* address = reinterpret_cast<const sockaddr_in*>(entry->ifa_addr);
        const auto* netmask = reinterpret_cast<const sockaddr_in*>(entry->ifa_netmask);
        entries.push_back(HostEntry{if_nametoindex(entry->ifa_name), ntohl(address->sin_addr.s_addr), ntohl(netmask->sin_addr.s_addr), address->sin_addr.s_addr});
    }
    freeifaddrs(list);
#endif
    return entries;
}

std::vector<Ipv4Interface> HostIpv4Interfaces() {
    std::vector<Ipv4Interface> interfaces;
    for (const auto& entry : HostIpv4Entries()) interfaces.push_back(Ipv4Interface{entry.Address, entry.Netmask});
    return interfaces;
}

}

bool HostIpv4AddressOfIndex(std::uint32_t index, std::uint32_t& networkAddress) {
    if (index == 0) return false;
    for (const auto& entry : HostIpv4Entries()) {
        if (entry.Index != index) continue;
        networkAddress = entry.NetworkAddress;
        return true;
    }
    return false;
}

void ForwardLanSend(std::uintptr_t native, const sockaddr* destination, const void* buffer, std::size_t length) {
    if (destination == nullptr || destination->sa_family != AF_INET) return;
    const auto* target = reinterpret_cast<const sockaddr_in*>(destination);
    const std::uint16_t destinationPort = ntohs(target->sin_port);
    if (destinationPort == 0 || length > MaxFramePayload) return;
    WithActiveRelay([&](Relay& relay) {
        if (relay.Peers().empty()) return;
        const std::uint32_t address = ntohl(target->sin_addr.s_addr);
        if (!IsMulticast(address)) {
            if (!IsBroadcast(address, HostIpv4Interfaces())) return;
            int enabled = 0;
            socklen_t enabledSize = sizeof enabled;
            if (getsockopt(static_cast<SocketHandle>(native), SOL_SOCKET, SO_BROADCAST, reinterpret_cast<char*>(&enabled), &enabledSize) != 0 || enabled == 0) return;
        }
        sockaddr_in local{};
        socklen_t localSize = sizeof local;
        if (getsockname(static_cast<SocketHandle>(native), reinterpret_cast<sockaddr*>(&local), &localSize) != 0) return;
        const auto* bytes = static_cast<const std::uint8_t*>(buffer);
        relay.SendToPeers(ntohs(local.sin_port), destinationPort, std::vector<std::uint8_t>(bytes, bytes + length));
    });
}

}
