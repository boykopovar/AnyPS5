#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#else
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#endif
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <chrono>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#ifdef _WIN32
#include <vector>
#else
#include <arpa/inet.h>
#endif

// Network-control state reflects host interface addressing, not PSN sign-in status.
static constexpr int SCE_NET_CTL_ERROR_CALLBACK_MAX = static_cast<int>(0x80412103);
static constexpr int SCE_NET_CTL_ERROR_INVALID_ID = static_cast<int>(0x80412105);
static constexpr int SCE_NET_CTL_ERROR_INVALID_ADDR = static_cast<int>(0x80412107);
static constexpr int SCE_NET_CTL_ERROR_NOT_CONNECTED = static_cast<int>(0x80412108);
static constexpr int NET_CTL_STATE_DISCONNECTED = 0;
static constexpr int NET_CTL_STATE_IPOBTAINED = 3;
static constexpr int MAX_CALLBACKS = 8;
static constexpr int NET_CTL_INFO_DEVICE = 1;
static constexpr int NET_CTL_INFO_MTU = 3;
static constexpr int NET_CTL_INFO_LINK = 4;
static constexpr int NET_CTL_INFO_IP_CONFIG = 11;
static constexpr int NET_CTL_INFO_IP_ADDRESS = 14;
static constexpr int NET_CTL_INFO_NETMASK = 15;
static constexpr int NET_CTL_INFO_DEFAULT_ROUTE = 16;
static constexpr int NET_CTL_INFO_PRIMARY_DNS = 17;
static constexpr int NET_CTL_INFO_SECONDARY_DNS = 18;

static std::mutex g_callbackLock;
static NetCtlCallback g_callbacks[MAX_CALLBACKS] = {};

static bool QueryHostHasAddress(int family) {
#ifdef _WIN32
    static const int socket_status = [] {
        WSADATA data{};
        return WSAStartup(MAKEWORD(2, 2), &data);
    }();
    if (socket_status != 0) return false;
    ULONG size = 0;
    const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    if (GetAdaptersAddresses(family, flags, nullptr, nullptr, &size) != ERROR_BUFFER_OVERFLOW) return false;
    std::vector<std::uint8_t> storage(size);
    auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data());
    if (GetAdaptersAddresses(family, flags, nullptr, adapters, &size) != NO_ERROR) return false;
    for (auto* adapter = adapters; adapter; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        for (auto* address = adapter->FirstUnicastAddress; address; address = address->Next) {
            if (!address->Address.lpSockaddr) continue;
            if (address->Address.lpSockaddr->sa_family == AF_INET) {
                const auto* v4 = reinterpret_cast<const sockaddr_in*>(address->Address.lpSockaddr);
                const auto host = ntohl(v4->sin_addr.s_addr);
                if (host != 0 && (host >> 24) != 127) return true;
            } else if (address->Address.lpSockaddr->sa_family == AF_INET6) {
                const auto* v6 = reinterpret_cast<const sockaddr_in6*>(address->Address.lpSockaddr);
                if (!IN6_IS_ADDR_UNSPECIFIED(&v6->sin6_addr) && !IN6_IS_ADDR_LOOPBACK(&v6->sin6_addr)) return true;
            }
        }
    }
    return false;
#else
    ifaddrs* interfaces = nullptr;
    if (getifaddrs(&interfaces) != 0) return false;
    bool found = false;
    for (auto* item = interfaces; item && !found; item = item->ifa_next) {
        if (!item->ifa_addr || !(item->ifa_flags & IFF_UP) || (item->ifa_flags & IFF_LOOPBACK)) continue;
        const int address_family = item->ifa_addr->sa_family;
        if ((family == AF_UNSPEC || family == address_family) && address_family == AF_INET) {
            const auto* v4 = reinterpret_cast<const sockaddr_in*>(item->ifa_addr);
            const auto host = ntohl(v4->sin_addr.s_addr);
            found = host != 0 && (host >> 24) != 127;
        } else if ((family == AF_UNSPEC || family == address_family) && address_family == AF_INET6) {
            const auto* v6 = reinterpret_cast<const sockaddr_in6*>(item->ifa_addr);
            found = !IN6_IS_ADDR_UNSPECIFIED(&v6->sin6_addr) && !IN6_IS_ADDR_LOOPBACK(&v6->sin6_addr);
        }
    }
    freeifaddrs(interfaces);
    return found;
#endif
}

// The host's first active non-loopback IPv4 interface, as SceNetCtlInfo reports one: dotted addresses,
// with 0.0.0.0 for a route or DNS server the host does not name.
struct HostIpv4 {
    std::string address = "127.0.0.1";
    std::string netmask = "255.0.0.0";
    std::string gateway = "0.0.0.0";
    std::string primaryDns = "0.0.0.0";
    std::string secondaryDns = "0.0.0.0";
    std::uint32_t mtu = 1500;
};

static std::string DottedIpv4(const sockaddr* address) {
    char text[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, &reinterpret_cast<const sockaddr_in*>(address)->sin_addr, text, sizeof(text));
    return text;
}

static std::string DottedMask(std::uint32_t prefix) {
    const std::uint32_t mask = prefix == 0 ? 0u : ~0u << (32u - prefix);
    in_addr value{};
    value.s_addr = htonl(mask);
    char text[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, &value, text, sizeof(text));
    return text;
}

// Without any interface the loopback address stands in: titles start their local servers (a campaign's
// listen server) on the address they read here, and fail without one.
static HostIpv4 ReadHostIpv4() {
    HostIpv4 result;
#ifdef _WIN32
    if (!QueryHostHasAddress(AF_INET)) return result;
    ULONG size = 0;
    const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_INCLUDE_GATEWAYS;
    if (GetAdaptersAddresses(AF_INET, flags, nullptr, nullptr, &size) != ERROR_BUFFER_OVERFLOW) return result;
    std::vector<std::uint8_t> storage(size);
    auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data());
    if (GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &size) != NO_ERROR) return result;
    for (auto* adapter = adapters; adapter; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        for (auto* unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next) {
            const auto* address = unicast->Address.lpSockaddr;
            if (!address || address->sa_family != AF_INET) continue;
            const auto host = ntohl(reinterpret_cast<const sockaddr_in*>(address)->sin_addr.s_addr);
            if (host == 0 || (host >> 24) == 127) continue;
            result.address = DottedIpv4(address);
            result.netmask = DottedMask(unicast->OnLinkPrefixLength);
            result.mtu = adapter->Mtu;
            for (auto* gateway = adapter->FirstGatewayAddress; gateway; gateway = gateway->Next) {
                if (gateway->Address.lpSockaddr && gateway->Address.lpSockaddr->sa_family == AF_INET) {
                    result.gateway = DottedIpv4(gateway->Address.lpSockaddr);
                    break;
                }
            }
            int dnsIndex = 0;
            for (auto* dns = adapter->FirstDnsServerAddress; dns && dnsIndex < 2; dns = dns->Next) {
                if (!dns->Address.lpSockaddr || dns->Address.lpSockaddr->sa_family != AF_INET) continue;
                (dnsIndex++ == 0 ? result.primaryDns : result.secondaryDns) = DottedIpv4(dns->Address.lpSockaddr);
            }
            return result;
        }
    }
#else
    ifaddrs* interfaces = nullptr;
    if (getifaddrs(&interfaces) != 0) return result;
    for (auto* item = interfaces; item; item = item->ifa_next) {
        if (!item->ifa_addr || item->ifa_addr->sa_family != AF_INET || !(item->ifa_flags & IFF_UP) || (item->ifa_flags & IFF_LOOPBACK)) continue;
        const auto host = ntohl(reinterpret_cast<const sockaddr_in*>(item->ifa_addr)->sin_addr.s_addr);
        if (host == 0 || (host >> 24) == 127) continue;
        result.address = DottedIpv4(item->ifa_addr);
        if (item->ifa_netmask) result.netmask = DottedIpv4(item->ifa_netmask);
        break;
    }
    freeifaddrs(interfaces);
#endif
    return result;
}

// Titles poll the network state every frame, and one adapter enumeration takes milliseconds on Windows:
// the host's answers are kept for a few seconds.
constexpr auto HOST_QUERY_LIFETIME = std::chrono::seconds(3);

template <typename Value, typename Query>
static Value CachedHostQuery(std::mutex& lock, std::optional<Value>& cached, std::chrono::steady_clock::time_point& stamp, Query query) {
    std::lock_guard guard(lock);
    const auto now = std::chrono::steady_clock::now();
    if (!cached || now - stamp >= HOST_QUERY_LIFETIME) {
        cached = query();
        stamp = now;
    }
    return *cached;
}

static bool HostHasAddress(int family) {
    static std::mutex lock;
    static std::optional<bool> cached[3];
    static std::chrono::steady_clock::time_point stamps[3];
    const int slot = family == AF_INET ? 1 : family == AF_INET6 ? 2 : 0;
    return CachedHostQuery(lock, cached[slot], stamps[slot], [family] { return QueryHostHasAddress(family); });
}

static HostIpv4 QueryHostIpv4() {
    static std::mutex lock;
    static std::optional<HostIpv4> cached;
    static std::chrono::steady_clock::time_point stamp;
    return CachedHostQuery(lock, cached, stamp, ReadHostIpv4);
}

extern "C" {

int APS5_VABI sceNetCtlCheckCallback(void) {
    return 0;
}

int APS5_VABI sceNetCtlGetInfo(int code, NetCtlInfo* info) {
    if (!info) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    const auto host = QueryHostIpv4();
    std::memset(info, 0, sizeof(*info));
    const auto text = [&](const std::string& value) {
        std::strncpy(reinterpret_cast<char*>(info->opaque), value.c_str(), 15);
        return 0;
    };
    const auto word = [&](std::uint32_t value) {
        std::memcpy(info->opaque, &value, sizeof(value));
        return 0;
    };
    switch (code) {
    case NET_CTL_INFO_DEVICE: return word(0);  // wired
    case NET_CTL_INFO_MTU: return word(host.mtu);
    case NET_CTL_INFO_LINK: return word(1);    // connected
    case NET_CTL_INFO_IP_CONFIG: return word(0);  // DHCP
    case NET_CTL_INFO_IP_ADDRESS: return text(host.address);
    case NET_CTL_INFO_NETMASK: return text(host.netmask);
    case NET_CTL_INFO_DEFAULT_ROUTE: return text(host.gateway);
    case NET_CTL_INFO_PRIMARY_DNS: return text(host.primaryDns);
    case NET_CTL_INFO_SECONDARY_DNS: return text(host.secondaryDns);
    default: return SCE_NET_CTL_ERROR_NOT_CONNECTED;
    }
}

int APS5_VABI sceNetCtlGetNatInfo(NetCtlNatInfo* nat_info) {
    (void)nat_info;
    return SCE_NET_CTL_ERROR_NOT_CONNECTED;
}

int APS5_VABI sceNetCtlGetResult(int event_type, int* error_code) {
    (void)event_type;
    if (!error_code) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    *error_code = 0;
    return 0;
}

int APS5_VABI sceNetCtlGetState(int* state) {
    if (!state) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    *state = HostHasAddress(AF_UNSPEC) ? NET_CTL_STATE_IPOBTAINED : NET_CTL_STATE_DISCONNECTED;
    return 0;
}

int APS5_VABI sceNetCtlGetStateV6(int* state) {
    if (!state) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    *state = HostHasAddress(AF_INET6) ? NET_CTL_STATE_IPOBTAINED : NET_CTL_STATE_DISCONNECTED;
    return 0;
}

int APS5_VABI sceNetCtlInit(void) {
    return 0;
}

int APS5_VABI sceNetCtlRegisterCallback(NetCtlCallback func, void* arg, int* cid) {
    if (!func || !cid) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    std::lock_guard lock(g_callbackLock);
    for (int index = 0; index < MAX_CALLBACKS; ++index) {
        if (!g_callbacks[index]) {
            g_callbacks[index] = func;
            *cid = index;
            return 0;
        }
    }
    return SCE_NET_CTL_ERROR_CALLBACK_MAX;
}

void APS5_VABI sceNetCtlTerm(void) {
}

int APS5_VABI sceNetCtlUnregisterCallback(int cid) {
    if (cid < 0 || cid >= MAX_CALLBACKS) return SCE_NET_CTL_ERROR_INVALID_ID;
    std::lock_guard lock(g_callbackLock);
    g_callbacks[cid] = nullptr;
    return 0;
}

}
