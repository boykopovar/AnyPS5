#include "prx/libc/include/Api.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#ifdef __linux__
#include <net/if_arp.h>
#include <netpacket/packet.h>
#elif defined(__APPLE__)
#include <net/if_dl.h>
#include <net/if_types.h>
#endif
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fstream>
#include <string>
#endif
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

static_assert(sizeof(GuestInterfaceAddress) == 56);
static_assert(offsetof(GuestInterfaceAddress, flags) == 16);
static_assert(offsetof(GuestInterfaceAddress, address) == 24);
static_assert(offsetof(GuestInterfaceAddress, netmask) == 32);
static_assert(offsetof(GuestInterfaceAddress, destination) == 40);
static_assert(offsetof(GuestInterfaceAddress, data) == 48);

static void Require(bool value) { if (!value) std::abort(); }

struct HostInterfaceAddress {
    int family;
    std::uint8_t address[16];
    std::uint8_t netmask[16];
    std::uint32_t scope;
    char name[16];
    unsigned int index;
    std::uint8_t type;
    std::uint8_t hardwareLength;
    std::uint8_t hardware[8];
    bool loopbackUp;
};

static HostInterfaceAddress HostAddress(const sockaddr* address) {
    HostInterfaceAddress entry{};
    entry.family = address->sa_family;
    if (entry.family == AF_INET) {
        std::memcpy(entry.address, &reinterpret_cast<const sockaddr_in*>(address)->sin_addr, 4);
    } else {
        const auto* v6 = reinterpret_cast<const sockaddr_in6*>(address);
        std::memcpy(entry.address, &v6->sin6_addr, 16);
        entry.scope = v6->sin6_scope_id;
    }
    return entry;
}

#ifdef __linux__
constexpr int HostLinkFamily = AF_PACKET;

static std::uint64_t SysfsCounter(const char* name, const char* file) {
    std::ifstream input(std::string("/sys/class/net/") + name + "/statistics/" + file);
    unsigned long long value = 0;
    Require(static_cast<bool>(input >> value));
    return value;
}
#elif defined(__APPLE__)
constexpr int HostLinkFamily = AF_LINK;

static std::uint64_t SysfsCounter(const char* name, const char* file) {
    ifaddrs* host = nullptr;
    Require(::getifaddrs(&host) == 0);
    std::uint64_t value = 0;
    bool found = false;
    for (auto* item = host; item; item = item->ifa_next) {
        if (!item->ifa_addr || item->ifa_addr->sa_family != AF_LINK || std::strcmp(item->ifa_name, name) != 0 || !item->ifa_data) continue;
        const auto& data = *static_cast<const if_data*>(item->ifa_data);
        const std::string counter(file);
        value = counter == "rx_packets" ? data.ifi_ipackets : counter == "tx_packets" ? data.ifi_opackets : counter == "rx_bytes" ? data.ifi_ibytes : data.ifi_obytes;
        found = true;
    }
    ::freeifaddrs(host);
    Require(found);
    return value;
}
#endif

#if defined(__linux__) || defined(__APPLE__)

static std::uint32_t HostMtu(const char* name) {
    const int probe = ::socket(AF_INET, SOCK_DGRAM, 0);
    Require(probe >= 0);
    ifreq request{};
    std::strncpy(request.ifr_name, name, sizeof(request.ifr_name) - 1);
    Require(::ioctl(probe, SIOCGIFMTU, &request) == 0);
    ::close(probe);
    return static_cast<std::uint32_t>(request.ifr_mtu);
}

static void SendLoopbackDatagram() {
    const int receiver = ::socket(AF_INET, SOCK_DGRAM, 0);
    const int sender = ::socket(AF_INET, SOCK_DGRAM, 0);
    Require(receiver >= 0 && sender >= 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    Require(::bind(receiver, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0);
    socklen_t length = sizeof(address);
    Require(::getsockname(receiver, reinterpret_cast<sockaddr*>(&address), &length) == 0);
    const char payload = 'x';
    Require(::sendto(sender, &payload, 1, 0, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 1);
    char received = 0;
    Require(::recv(receiver, &received, 1, 0) == 1);
    ::close(sender);
    ::close(receiver);
}
#endif

static std::vector<HostInterfaceAddress> HostSnapshot() {
    std::vector<HostInterfaceAddress> entries;
#ifdef _WIN32
    const ULONG options = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG size = 0;
    std::vector<std::uint8_t> storage;
    ULONG result = GetAdaptersAddresses(AF_UNSPEC, options, nullptr, nullptr, &size);
    for (unsigned calls = 1; result == ERROR_BUFFER_OVERFLOW && calls < 4; ++calls) {
        storage.resize(size);
        result = GetAdaptersAddresses(AF_UNSPEC, options, nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data()), &size);
    }
    if (result == ERROR_NO_DATA) return entries;
    Require(result == NO_ERROR);
    for (auto* adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data()); adapter; adapter = adapter->Next)
        for (auto* unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next) {
            const sockaddr* address = unicast->Address.lpSockaddr;
            if (!address || (address->sa_family != AF_INET && address->sa_family != AF_INET6)) continue;
            HostInterfaceAddress entry = HostAddress(address);
            const unsigned bits = entry.family == AF_INET ? 32 : 128;
            unsigned prefix = unicast->OnLinkPrefixLength;
            if (prefix > bits) prefix = bits;
            for (unsigned bit = 0; bit < prefix; ++bit) entry.netmask[bit / 8] |= static_cast<std::uint8_t>(0x80 >> (bit % 8));
            entries.push_back(entry);
        }
#else
    ifaddrs* host = nullptr;
    Require(::getifaddrs(&host) == 0);
    std::vector<std::string> names;
    std::vector<std::vector<HostInterfaceAddress>> groups;
    const auto group = [&](const char* name) -> std::vector<HostInterfaceAddress>& {
        for (std::size_t index = 0; index < names.size(); ++index)
            if (names[index] == name) return groups[index];
        names.emplace_back(name);
        return groups.emplace_back();
    };
    try {
        for (auto* item = host; item; item = item->ifa_next) {
            if (!item->ifa_addr) continue;
#ifdef __linux__
            if (item->ifa_addr->sa_family == AF_PACKET) {
                const auto& link = *reinterpret_cast<const sockaddr_ll*>(item->ifa_addr);
                HostInterfaceAddress entry{};
                entry.family = AF_PACKET;
                std::strncpy(entry.name, item->ifa_name, sizeof(entry.name) - 1);
                entry.index = static_cast<unsigned int>(link.sll_ifindex);
                entry.type = link.sll_hatype == ARPHRD_ETHER ? 0x6 : link.sll_hatype == ARPHRD_LOOPBACK ? 0x18 : link.sll_hatype == ARPHRD_PPP ? 0x17 : 0x1;
                entry.hardwareLength = entry.type == 0x18 ? 0 : link.sll_halen;
                std::memcpy(entry.hardware, link.sll_addr, entry.hardwareLength);
                entry.loopbackUp = (item->ifa_flags & IFF_UP) != 0;
                auto& members = group(item->ifa_name);
                members.insert(members.begin(), entry);
                continue;
            }
#elif defined(__APPLE__)
            if (item->ifa_addr->sa_family == AF_LINK) {
                const auto& link = *reinterpret_cast<const sockaddr_dl*>(item->ifa_addr);
                HostInterfaceAddress entry{};
                entry.family = AF_LINK;
                std::strncpy(entry.name, item->ifa_name, sizeof(entry.name) - 1);
                entry.index = link.sdl_index;
                entry.type = link.sdl_type == IFT_ETHER ? 0x6 : link.sdl_type == IFT_LOOP ? 0x18 : link.sdl_type == IFT_PPP ? 0x17 : 0x1;
                entry.hardwareLength = entry.type == 0x18 ? 0 : std::min<std::uint8_t>(link.sdl_alen, sizeof(entry.hardware));
                std::memcpy(entry.hardware, link.sdl_data + link.sdl_nlen, entry.hardwareLength);
                entry.loopbackUp = (item->ifa_flags & IFF_UP) != 0;
                auto& members = group(item->ifa_name);
                members.insert(members.begin(), entry);
                continue;
            }
#endif
            if (item->ifa_addr->sa_family != AF_INET && item->ifa_addr->sa_family != AF_INET6) continue;
            Require(item->ifa_netmask != nullptr);
            HostInterfaceAddress entry = HostAddress(item->ifa_addr);
            std::strncpy(entry.name, item->ifa_name, sizeof(entry.name) - 1);
            if (entry.family == AF_INET)
                std::memcpy(entry.netmask, &reinterpret_cast<const sockaddr_in*>(item->ifa_netmask)->sin_addr, 4);
            else
                std::memcpy(entry.netmask, &reinterpret_cast<const sockaddr_in6*>(item->ifa_netmask)->sin6_addr, 16);
            group(item->ifa_name).push_back(entry);
        }
    } catch (...) {
        ::freeifaddrs(host);
        throw;
    }
    ::freeifaddrs(host);
    for (const auto& members : groups) entries.insert(entries.end(), members.begin(), members.end());
#endif
    return entries;
}

int main() {
    std::array<void*, 10> api{};
    ApplicationHeapRegister_nid_no_patch(api.data());
    constexpr std::uint32_t knownFlags = 0x1 | 0x2 | 0x8 | 0x10 | 0x40 | 0x80 | 0x100 | 0x200 | 0x8000;
    const std::uint8_t loopbackAddress[] = {127, 0, 0, 1};
    const std::uint8_t loopbackMask[] = {255, 0, 0, 0};
    bool matched = false;
#if defined(__linux__) || defined(__APPLE__)
    bool linkChecked = false;
    SendLoopbackDatagram();
#endif
    for (unsigned attempt = 0; attempt < 3 && !matched; ++attempt) {
        const std::vector<HostInterfaceAddress> host = HostSnapshot();
        GuestInterfaceAddress* list = nullptr;
        Require(getifaddrs_nid_postfix(&list) == 0);
        std::size_t entries = 0;
        bool equal = true;
        bool loopback = false;
        for (auto* item = list; item; item = item->next) {
            Require(++entries < 100000);
            Require(item->name != nullptr);
            const std::size_t nameLength = std::strlen(item->name);
            Require(nameLength >= 1 && nameLength <= 15);
            Require((item->flags & ~knownFlags) == 0);
            const auto* address = static_cast<const std::uint8_t*>(item->address);
            Require(address != nullptr && reinterpret_cast<std::uintptr_t>(address) % 8 == 0);
#if defined(__linux__) || defined(__APPLE__)
            if (address[1] == 18) {
                Require(item->netmask == nullptr && item->destination == nullptr);
                const auto* data = static_cast<const std::uint8_t*>(item->data);
                Require(data != nullptr && reinterpret_cast<std::uintptr_t>(data) % 8 == 0);
                std::uint16_t index = 0;
                std::uint16_t dataLength = 0;
                std::uint32_t mtu = 0;
                std::uint64_t counters[11] = {};
                std::memcpy(&index, address + 2, 2);
                std::memcpy(&dataLength, data + 6, 2);
                std::memcpy(&mtu, data + 8, 4);
                std::memcpy(counters, data + 24, sizeof(counters));
                Require(address[0] >= 54 && address[0] % 8 == 0 && address[0] >= 24 + address[6]);
                Require(address[5] == nameLength && std::memcmp(address + 8, item->name, nameLength) == 0 && address[7] == 0);
                Require(dataLength == 152 && data[0] == address[4] && data[2] == address[6] && data[1] == 0 && data[5] == 0);
                Require(data[4] == 1 || data[4] == 2);
                Require(index == if_nametoindex(item->name));
                Require(mtu == HostMtu(item->name));
                Require(counters[0] <= SysfsCounter(item->name, "rx_packets") && counters[2] <= SysfsCounter(item->name, "tx_packets"));
                Require(counters[5] <= SysfsCounter(item->name, "rx_bytes") && counters[6] <= SysfsCounter(item->name, "tx_bytes"));
                if (entries <= host.size()) {
                    const HostInterfaceAddress& expected = host[entries - 1];
                    equal = equal && expected.family == HostLinkFamily && std::strcmp(expected.name, item->name) == 0 && expected.index == index &&
                        expected.type == address[4] && expected.hardwareLength == address[6] &&
                        std::memcmp(expected.hardware, address + 8 + nameLength, expected.hardwareLength) == 0;
                    if (expected.type == 0x18) {
                        Require(data[3] == 0 && data[4] == (expected.loopbackUp ? 2 : 1));
                        if (expected.loopbackUp) Require(counters[0] > 0 && counters[2] > 0 && counters[5] > 0 && counters[6] > 0);
                        linkChecked = linkChecked || expected.loopbackUp;
                    }
                    if (expected.type == 0x6) Require(data[3] == 14 && address[6] == 6);
                } else {
                    equal = false;
                }
                continue;
            }
#endif
            Require(item->data == nullptr);
            const auto* netmask = static_cast<const std::uint8_t*>(item->netmask);
            Require(address != nullptr && netmask != nullptr);
            Require(reinterpret_cast<std::uintptr_t>(address) % 8 == 0);
            Require(reinterpret_cast<std::uintptr_t>(netmask) % 8 == 0);
            Require((address[0] == 16 && address[1] == 2) || (address[0] == 28 && address[1] == 28));
            Require(netmask[0] == address[0] && netmask[1] == address[1]);
            Require(address[2] == 0 && address[3] == 0 && netmask[2] == 0 && netmask[3] == 0);
            if (address[1] == 2) {
                for (std::size_t index = 8; index < 16; ++index) Require(address[index] == 0 && netmask[index] == 0);
            } else {
                for (std::size_t index = 4; index < 8; ++index) Require(address[index] == 0 && netmask[index] == 0);
            }
            if (item->destination) {
                const auto* destination = static_cast<const std::uint8_t*>(item->destination);
                Require(reinterpret_cast<std::uintptr_t>(destination) % 8 == 0);
                Require(destination[0] == address[0] && destination[1] == address[1]);
            }
            if (address[1] == 2 && (item->flags & 0x2)) Require(item->destination != nullptr);
            if (item->flags & 0x10) Require(item->destination != nullptr);
            if (entries <= host.size()) {
                const HostInterfaceAddress& expected = host[entries - 1];
#ifndef _WIN32
                equal = equal && std::strcmp(expected.name, item->name) == 0;
#endif
                const bool ipv4 = address[1] == 2;
                const std::size_t offset = ipv4 ? 4 : 8;
                const std::size_t length = ipv4 ? 4 : 16;
                equal = equal && expected.family == (ipv4 ? AF_INET : AF_INET6) &&
                    std::memcmp(address + offset, expected.address, length) == 0 &&
                    std::memcmp(netmask + offset, expected.netmask, length) == 0;
                if (!ipv4) {
                    std::uint32_t scope;
                    std::memcpy(&scope, address + 24, 4);
                    equal = equal && scope == expected.scope;
                }
            } else {
                equal = false;
            }
            if (address[1] == 2 && std::memcmp(address + 4, loopbackAddress, 4) == 0)
                loopback = loopback || (std::memcmp(netmask + 4, loopbackMask, 4) == 0 && (item->flags & 0x9) == 0x9 && !(item->flags & 0x2));
        }
        matched = equal && entries == host.size() && loopback;
        freeifaddrs_nid_postfix(list);

    }
    Require(matched);
#if defined(__linux__) || defined(__APPLE__)
    Require(linkChecked);
#endif
    GuestInterfaceAddress* released = nullptr;
    Require(getifaddrs_nid_postfix(&released) == 0 && released != nullptr);
    free_nid_postfix(released);
    freeifaddrs_nid_postfix(nullptr);

    *__error_nid_postfix() = 0;
    Require(getifaddrs_nid_postfix(nullptr) == -1);
    Require(*__error_nid_postfix() == 14);
}
