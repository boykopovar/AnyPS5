#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#else
#include <ifaddrs.h>
#include <netinet/in.h>
#endif
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

struct GuestInterfaceAddress {
    GuestInterfaceAddress* next;
    char* name;
    std::uint32_t flags;
    void* address;
    void* netmask;
    void* destination;
    void* data;
};
static_assert(sizeof(GuestInterfaceAddress) == 56);
static_assert(offsetof(GuestInterfaceAddress, flags) == 16);
static_assert(offsetof(GuestInterfaceAddress, address) == 24);
static_assert(offsetof(GuestInterfaceAddress, netmask) == 32);
static_assert(offsetof(GuestInterfaceAddress, destination) == 40);
static_assert(offsetof(GuestInterfaceAddress, data) == 48);

extern "C" {
int APS5_VABI getifaddrs_nid_postfix(GuestInterfaceAddress** list);
void APS5_VABI freeifaddrs_nid_postfix(GuestInterfaceAddress* list);
int* APS5_VABI __error_nid_postfix();
}

static void Require(bool value) { if (!value) std::abort(); }

struct HostInterfaceAddress {
    int family;
    std::uint8_t address[16];
    std::uint8_t netmask[16];
    std::uint32_t scope;
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
    try {
        for (auto* item = host; item; item = item->ifa_next) {
            if (!item->ifa_addr || (item->ifa_addr->sa_family != AF_INET && item->ifa_addr->sa_family != AF_INET6)) continue;
            Require(item->ifa_netmask != nullptr);
            HostInterfaceAddress entry = HostAddress(item->ifa_addr);
            if (entry.family == AF_INET)
                std::memcpy(entry.netmask, &reinterpret_cast<const sockaddr_in*>(item->ifa_netmask)->sin_addr, 4);
            else
                std::memcpy(entry.netmask, &reinterpret_cast<const sockaddr_in6*>(item->ifa_netmask)->sin6_addr, 16);
            entries.push_back(entry);
        }
    } catch (...) {
        ::freeifaddrs(host);
        throw;
    }
    ::freeifaddrs(host);
#endif
    return entries;
}

int main() {
    constexpr std::uint32_t knownFlags = 0x1 | 0x2 | 0x8 | 0x10 | 0x40 | 0x80 | 0x100 | 0x200 | 0x8000;
    const std::uint8_t loopbackAddress[] = {127, 0, 0, 1};
    const std::uint8_t loopbackMask[] = {255, 0, 0, 0};
    bool matched = false;
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
            Require(item->data == nullptr);
            const auto* address = static_cast<const std::uint8_t*>(item->address);
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
    freeifaddrs_nid_postfix(nullptr);
    *__error_nid_postfix() = 0;
    Require(getifaddrs_nid_postfix(nullptr) == -1);
    Require(*__error_nid_postfix() == 14);
}
