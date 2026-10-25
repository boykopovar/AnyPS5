#include "prx/libc/include/Api.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#else
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#ifdef __linux__
#include <linux/if_link.h>
#include <net/if_arp.h>
#include <netpacket/packet.h>
#endif
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <fstream>
#endif
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

static_assert(sizeof(GuestInterfaceAddress) == 56);
static_assert(offsetof(GuestInterfaceAddress, flags) == 16);
static_assert(offsetof(GuestInterfaceAddress, address) == 24);
static_assert(offsetof(GuestInterfaceAddress, destination) == 40);
static_assert(offsetof(GuestInterfaceAddress, data) == 48);

namespace {
constexpr std::uint32_t guestUp = 0x1;
constexpr std::uint32_t guestBroadcast = 0x2;
constexpr std::uint32_t guestLoopback = 0x8;
constexpr std::uint32_t guestPointToPoint = 0x10;
constexpr std::uint32_t guestRunning = 0x40;
constexpr std::uint32_t guestMulticast = 0x8000;
constexpr std::size_t guestNameCapacity = 16;

struct Entry {
    std::string name;
    std::uint32_t flags;
    std::vector<std::uint8_t> address;
    std::vector<std::uint8_t> netmask;
    std::vector<std::uint8_t> destination;
    std::vector<std::uint8_t> data;
};

std::vector<std::uint8_t> GuestIpv4(const std::uint8_t* bytes) {
    std::vector<std::uint8_t> guest(16, 0);
    guest[0] = 16;
    guest[1] = 2;
    std::memcpy(guest.data() + 4, bytes, 4);
    return guest;
}

std::vector<std::uint8_t> GuestIpv6(const std::uint8_t* bytes, std::uint32_t scope) {
    std::vector<std::uint8_t> guest(28, 0);
    guest[0] = 28;
    guest[1] = 28;
    std::memcpy(guest.data() + 8, bytes, 16);
    std::memcpy(guest.data() + 24, &scope, 4);
    return guest;
}

std::vector<std::uint8_t> GuestAddress(const sockaddr* address) {
    if (address->sa_family == AF_INET)
        return GuestIpv4(reinterpret_cast<const std::uint8_t*>(&reinterpret_cast<const sockaddr_in*>(address)->sin_addr));
    const auto* v6 = reinterpret_cast<const sockaddr_in6*>(address);
    return GuestIpv6(reinterpret_cast<const std::uint8_t*>(&v6->sin6_addr), v6->sin6_scope_id);
}

#ifndef _WIN32
std::vector<std::uint8_t> GuestNetmask(const sockaddr* netmask, int family) {
    sockaddr_storage mask{};
    const std::size_t size = family == AF_INET ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
#ifdef __APPLE__
    const std::size_t length = std::min<std::size_t>(netmask->sa_len, size);
#else
    const std::size_t length = size;
#endif
    if (length > offsetof(sockaddr, sa_data))
        std::memcpy(reinterpret_cast<char*>(&mask) + offsetof(sockaddr, sa_data), netmask->sa_data, length - offsetof(sockaddr, sa_data));
    mask.ss_family = static_cast<sa_family_t>(family);
    return GuestAddress(reinterpret_cast<const sockaddr*>(&mask));
}
#endif

std::string CheckedName(const char* name) {
    const std::string checked(name);
    if (checked.empty() || checked.size() >= guestNameCapacity)
        throw std::runtime_error("getifaddrs: host interface name does not fit the guest's 16-byte limit: " + checked);
    return checked;
}

std::size_t Aligned(std::size_t size) {
    return (size + 7) & ~static_cast<std::size_t>(7);
}

std::vector<std::uint8_t> BroadcastAddress(const std::vector<std::uint8_t>& address, const std::vector<std::uint8_t>& netmask) {
    std::vector<std::uint8_t> broadcast = address;
    for (std::size_t index = 4; index < 8; ++index)
        broadcast[index] = static_cast<std::uint8_t>(address[index] | static_cast<std::uint8_t>(~netmask[index]));
    return broadcast;
}

#ifdef _WIN32
std::vector<std::uint8_t> PrefixMask(int family, unsigned prefix) {
    const unsigned bits = family == AF_INET ? 32 : 128;
    if (prefix > bits) prefix = bits;
    std::uint8_t mask[16] = {};
    for (unsigned bit = 0; bit < prefix; ++bit) mask[bit / 8] |= static_cast<std::uint8_t>(0x80 >> (bit % 8));
    return family == AF_INET ? GuestIpv4(mask) : GuestIpv6(mask, 0);
}

std::uint32_t InterfaceFlags(const IP_ADAPTER_ADDRESSES* adapter) {
    std::uint32_t flags = 0;
    if (adapter->OperStatus == IfOperStatusUp) flags |= guestUp | guestRunning;
    if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) flags |= guestLoopback;
    else if (adapter->IfType != IF_TYPE_PPP && adapter->IfType != IF_TYPE_TUNNEL) flags |= guestBroadcast;
    if (!(adapter->Flags & IP_ADAPTER_NO_MULTICAST)) flags |= guestMulticast;
    return flags;
}

std::string InterfaceName(const IP_ADAPTER_ADDRESSES* adapter) {
    char name[NDIS_IF_MAX_STRING_SIZE + 1] = {};
    const auto status = ConvertInterfaceLuidToNameA(&adapter->Luid, name, sizeof(name));
    if (status != NO_ERROR) throw std::runtime_error("getifaddrs: ConvertInterfaceLuidToNameA failed with " + std::to_string(status));
    if (std::strlen(name) < guestNameCapacity) return CheckedName(name);
    return "if" + std::to_string(adapter->IfIndex ? adapter->IfIndex : adapter->Ipv6IfIndex);
}

int HostEntries(std::vector<Entry>& entries) {
    const ULONG options = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG size = 0;
    std::vector<std::uint8_t> storage;
    ULONG result = GetAdaptersAddresses(AF_UNSPEC, options, nullptr, nullptr, &size);
    for (unsigned calls = 1; result == ERROR_BUFFER_OVERFLOW && calls < 4; ++calls) {
        storage.resize(size);
        result = GetAdaptersAddresses(AF_UNSPEC, options, nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data()), &size);
    }
    if (result == ERROR_NO_DATA) return 0;
    if (result == ERROR_BUFFER_OVERFLOW) { *__error_nid_postfix() = 55; return -1; }
    if (result == ERROR_NOT_ENOUGH_MEMORY) { *__error_nid_postfix() = 12; return -1; }
    if (result != NO_ERROR) throw std::runtime_error("getifaddrs: GetAdaptersAddresses failed with " + std::to_string(result));
    for (auto* adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data()); adapter; adapter = adapter->Next) {
        std::string name;
        for (auto* unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next) {
            const sockaddr* address = unicast->Address.lpSockaddr;
            if (!address || (address->sa_family != AF_INET && address->sa_family != AF_INET6)) continue;
            if (name.empty()) name = InterfaceName(adapter);
            Entry entry{name, InterfaceFlags(adapter), GuestAddress(address), PrefixMask(address->sa_family, unicast->OnLinkPrefixLength), {}};
            if (address->sa_family == AF_INET && (entry.flags & guestBroadcast))
                entry.destination = BroadcastAddress(entry.address, entry.netmask);
            entries.push_back(std::move(entry));
        }
    }
    return 0;
}
#else
#ifdef __linux__
constexpr std::uint8_t guestLinkFamily = 18;
constexpr std::size_t guestLinkNameArea = 8 + 16;
constexpr std::size_t guestLinkMinimum = 54;
constexpr std::uint8_t guestTypeOther = 0x1;
constexpr std::uint8_t guestTypeEthernet = 0x6;
constexpr std::uint8_t guestTypePpp = 0x17;
constexpr std::uint8_t guestTypeLoopback = 0x18;
constexpr std::uint8_t guestLinkStateDown = 1;
constexpr std::uint8_t guestLinkStateUp = 2;
constexpr std::size_t guestInterfaceDataSize = 152;
constexpr unsigned int hostLowerUp = 0x10000;
#endif

struct HostInterface {
    std::string name;
    bool linked = false;
    Entry link;
    std::vector<Entry> addresses;
};

#ifdef __linux__
template<typename TValue>
void Store(std::vector<std::uint8_t>& bytes, std::size_t offset, TValue value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

std::vector<std::uint8_t> GuestLinkAddress(const std::string& name, std::uint16_t index, std::uint8_t type, const std::uint8_t* hardware, std::size_t hardwareLength) {
    std::size_t size = std::max(guestLinkMinimum, guestLinkNameArea + hardwareLength);
    size = Aligned(size);
    if (size > 0xFF) throw std::runtime_error("getifaddrs: link-level address of " + name + " does not fit sockaddr_dl");
    std::vector<std::uint8_t> guest(size, 0);
    guest[0] = static_cast<std::uint8_t>(size);
    guest[1] = guestLinkFamily;
    Store(guest, 2, index);
    guest[4] = type;
    guest[5] = static_cast<std::uint8_t>(name.size());
    guest[6] = static_cast<std::uint8_t>(hardwareLength);
    std::memcpy(guest.data() + 8, name.data(), name.size());
    if (hardwareLength != 0) std::memcpy(guest.data() + 8 + name.size(), hardware, hardwareLength);
    return guest;
}

std::optional<std::uint64_t> ReadSysfsCounter(const std::string& name, const char* file) {
    std::ifstream input("/sys/class/net/" + name + "/" + file);
    long long value = 0;
    if (!(input >> value)) return std::nullopt;
    return static_cast<std::uint64_t>(value);
}

std::uint32_t HostMtu(const std::string& name) {
    const int probe = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (probe < 0) throw std::runtime_error(std::string("getifaddrs: no socket to read the MTU of ") + name + ": " + std::strerror(errno));
    ifreq request{};
    std::memcpy(request.ifr_name, name.c_str(), name.size() + 1);
    const int result = ::ioctl(probe, SIOCGIFMTU, &request);
    const int error = errno;
    ::close(probe);
    if (result != 0) throw std::runtime_error(std::string("getifaddrs: SIOCGIFMTU failed on ") + name + ": " + std::strerror(error));
    return static_cast<std::uint32_t>(request.ifr_mtu);
}

std::vector<std::uint8_t> GuestInterfaceData(const std::string& name, unsigned int hostFlags, const void* hostStats, std::uint8_t type, std::uint8_t hardwareLength) {
    std::vector<std::uint8_t> data(guestInterfaceDataSize, 0);
    data[0] = type;
    data[2] = hardwareLength;
    data[3] = type == guestTypeEthernet ? 14 : 0;
    if (type == guestTypeLoopback) data[4] = (hostFlags & IFF_UP) ? guestLinkStateUp : guestLinkStateDown;
    else data[4] = (hostFlags & hostLowerUp) ? guestLinkStateUp : guestLinkStateDown;
    Store(data, 6, static_cast<std::uint16_t>(guestInterfaceDataSize));
    Store(data, 8, HostMtu(name));
    if (const auto speed = ReadSysfsCounter(name, "speed"); speed && static_cast<std::int64_t>(*speed) > 0)
        Store(data, 16, *speed * 1000000u);
    constexpr std::pair<const char*, std::size_t> counters[] = {
        {"statistics/rx_packets", 24}, {"statistics/rx_errors", 32}, {"statistics/tx_packets", 40}, {"statistics/tx_errors", 48},
        {"statistics/collisions", 56}, {"statistics/rx_bytes", 64}, {"statistics/tx_bytes", 72}, {"statistics/multicast", 80},
        {"statistics/rx_dropped", 96}, {"statistics/tx_dropped", 104},
    };
    bool complete = true;
    for (const auto& [file, offset] : counters) {
        const auto value = ReadSysfsCounter(name, file);
        if (!value) { complete = false; break; }
        Store(data, offset, *value);
    }
    if (!complete) {
        if (!hostStats) throw std::runtime_error("getifaddrs: no counters for " + name + " in sysfs or the host list");
        rtnl_link_stats stats{};
        std::memcpy(&stats, hostStats, sizeof(stats));
        const std::pair<std::uint32_t, std::size_t> fallback[] = {
            {stats.rx_packets, 24}, {stats.rx_errors, 32}, {stats.tx_packets, 40}, {stats.tx_errors, 48}, {stats.collisions, 56},
            {stats.rx_bytes, 64}, {stats.tx_bytes, 72}, {stats.multicast, 80}, {stats.rx_dropped, 96}, {stats.tx_dropped, 104},
        };
        for (const auto& [value, offset] : fallback) Store(data, offset, static_cast<std::uint64_t>(value));
    }
    return data;
}
#endif

std::uint32_t InterfaceFlags(unsigned int host) {
    constexpr std::pair<unsigned int, std::uint32_t> flags[] = {
        {IFF_UP, guestUp}, {IFF_BROADCAST, guestBroadcast}, {IFF_LOOPBACK, guestLoopback},
        {IFF_POINTOPOINT, guestPointToPoint}, {IFF_RUNNING, guestRunning}, {IFF_NOARP, 0x80},
        {IFF_PROMISC, 0x100}, {IFF_ALLMULTI, 0x200}, {IFF_MULTICAST, guestMulticast},
    };
    std::uint32_t guest = 0;
    for (const auto& [hostFlag, guestFlag] : flags)
        if (host & hostFlag) guest |= guestFlag;
    return guest;
}

int HostEntries(std::vector<Entry>& entries) {
    ifaddrs* host = nullptr;
    if (::getifaddrs(&host) != 0) {
        const int hostError = errno;
        switch (hostError) {
        case ENOMEM: *__error_nid_postfix() = 12; return -1;
        case ENOBUFS: *__error_nid_postfix() = 55; return -1;
        case EMFILE: *__error_nid_postfix() = 24; return -1;
        case ENFILE: *__error_nid_postfix() = 23; return -1;
        default: throw std::runtime_error(std::string("getifaddrs: host enumeration failed: ") + std::strerror(hostError));
        }
    }
    std::vector<HostInterface> interfaces;
    const auto interfaceFor = [&](const std::string& name) -> HostInterface& {
        const auto found = std::find_if(interfaces.begin(), interfaces.end(), [&](const HostInterface& candidate) { return candidate.name == name; });
        if (found != interfaces.end()) return *found;
        interfaces.push_back({name, false, {}, {}});
        return interfaces.back();
    };
    try {
        for (auto* item = host; item; item = item->ifa_next) {
            if (!item->ifa_addr) continue;
#ifdef __linux__
            if (item->ifa_addr->sa_family == AF_PACKET) {
                const auto& link = *reinterpret_cast<const sockaddr_ll*>(item->ifa_addr);
                HostInterface& owner = interfaceFor(CheckedName(item->ifa_name));
                if (owner.linked) throw std::runtime_error("getifaddrs: two link-level entries for " + owner.name);
                if (link.sll_ifindex <= 0 || link.sll_ifindex > 0xFFFF) throw std::runtime_error("getifaddrs: interface index of " + owner.name + " does not fit sockaddr_dl");
                const std::uint8_t type = link.sll_hatype == ARPHRD_ETHER ? guestTypeEthernet : link.sll_hatype == ARPHRD_LOOPBACK ? guestTypeLoopback
                    : link.sll_hatype == ARPHRD_PPP ? guestTypePpp : guestTypeOther;
                const std::uint8_t hardwareLength = type == guestTypeLoopback ? 0 : std::min<std::uint8_t>(link.sll_halen, sizeof(link.sll_addr));
                owner.link = Entry{owner.name, InterfaceFlags(item->ifa_flags),
                    GuestLinkAddress(owner.name, static_cast<std::uint16_t>(link.sll_ifindex), type, link.sll_addr, hardwareLength), {}, {},
                    GuestInterfaceData(owner.name, item->ifa_flags, item->ifa_data, type, hardwareLength)};
                owner.linked = true;
                continue;
            }
#endif
            if (item->ifa_addr->sa_family != AF_INET && item->ifa_addr->sa_family != AF_INET6) continue;
            if (!item->ifa_netmask) throw std::runtime_error(std::string("getifaddrs: host address without a netmask on ") + item->ifa_name);
            Entry entry{CheckedName(item->ifa_name), InterfaceFlags(item->ifa_flags), GuestAddress(item->ifa_addr), GuestNetmask(item->ifa_netmask, item->ifa_addr->sa_family), {}};
            const int family = item->ifa_addr->sa_family;
            if (item->ifa_flags & IFF_POINTOPOINT) {
                if (item->ifa_dstaddr && item->ifa_dstaddr->sa_family == family) entry.destination = GuestAddress(item->ifa_dstaddr);
                else entry.flags &= ~guestPointToPoint;
            } else if (family == AF_INET && (item->ifa_flags & IFF_BROADCAST)) {
                entry.destination = item->ifa_broadaddr && item->ifa_broadaddr->sa_family == AF_INET
                    ? GuestAddress(item->ifa_broadaddr) : BroadcastAddress(entry.address, entry.netmask);
            }
            interfaceFor(entry.name).addresses.push_back(std::move(entry));
        }
    } catch (...) {
        ::freeifaddrs(host);
        throw;
    }
    ::freeifaddrs(host);
    for (auto& owner : interfaces) {
        if (owner.linked) entries.push_back(std::move(owner.link));
        for (auto& address : owner.addresses) entries.push_back(std::move(address));
    }
    return 0;
}
#endif
}

extern "C" {

int APS5_VABI getifaddrs_nid_postfix(GuestInterfaceAddress** list) {
    if (!list) { *__error_nid_postfix() = 14; return -1; }
    try {
        std::vector<Entry> entries;
        if (HostEntries(entries) != 0) return -1;
        if (entries.empty()) {
            *list = nullptr;
            return 0;
        }
        std::size_t size = entries.size() * sizeof(GuestInterfaceAddress);
        for (const auto& entry : entries)
            size += Aligned(entry.name.size() + 1) + Aligned(entry.address.size()) + Aligned(entry.netmask.size()) + Aligned(entry.destination.size()) + Aligned(entry.data.size());
        auto* block = static_cast<std::uint8_t*>(ApplicationHeapAllocate_nid_no_patch(size));
        if (!block) {
            *__error_nid_postfix() = 12;
            return -1;
        }
        auto* items = reinterpret_cast<GuestInterfaceAddress*>(block);
        std::size_t offset = entries.size() * sizeof(GuestInterfaceAddress);
        const auto place = [&](const void* source, std::size_t length) {
            void* target = block + offset;
            std::memcpy(target, source, length);
            offset += Aligned(length);
            return target;
        };
        for (std::size_t index = 0; index < entries.size(); ++index) {
            const Entry& entry = entries[index];
            GuestInterfaceAddress& item = items[index];
            item.next = index + 1 < entries.size() ? &items[index + 1] : nullptr;
            item.name = static_cast<char*>(place(entry.name.c_str(), entry.name.size() + 1));
            item.flags = entry.flags;
            item.address = place(entry.address.data(), entry.address.size());
            item.netmask = entry.netmask.empty() ? nullptr : place(entry.netmask.data(), entry.netmask.size());
            item.destination = entry.destination.empty() ? nullptr : place(entry.destination.data(), entry.destination.size());
            item.data = entry.data.empty() ? nullptr : place(entry.data.data(), entry.data.size());
        }
        *list = items;
        return 0;
    } catch (const std::bad_alloc&) { *__error_nid_postfix() = 12; return -1; }
}

void APS5_VABI freeifaddrs_nid_postfix(GuestInterfaceAddress* list) {
    ApplicationHeapFree_nid_no_patch(list);
}

}
