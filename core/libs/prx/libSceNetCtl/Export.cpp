#include "prx/libSceNetCtl/include/NetCtl.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/route.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#ifdef __linux__
#include <netpacket/packet.h>
#endif
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif
#endif
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HostEntropy.hpp"
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// Network-control state reflects host interface addressing, not PSN sign-in status.

static constexpr int SCE_NET_CTL_ERROR_NOT_INITIALIZED = static_cast<int>(0x80412101);
static constexpr int SCE_NET_CTL_ERROR_ID_NOT_FOUND = static_cast<int>(0x80412104);
static constexpr int SCE_NET_CTL_ERROR_INVALID_SIZE = static_cast<int>(0x80412111);
static constexpr int SCE_NET_CTL_ERROR_CALLBACK_MAX = static_cast<int>(0x80412103);
static constexpr int SCE_NET_CTL_ERROR_INVALID_ID = static_cast<int>(0x80412105);
static constexpr int SCE_NET_CTL_ERROR_INVALID_CODE = static_cast<int>(0x80412106);
static constexpr int SCE_NET_CTL_ERROR_INVALID_ADDR = static_cast<int>(0x80412107);
static constexpr int SCE_NET_CTL_ERROR_NOT_CONNECTED = static_cast<int>(0x80412108);
static constexpr int NET_CTL_STATE_DISCONNECTED = 0;
static constexpr int NET_CTL_STATE_IPOBTAINED = 3;
static constexpr int NET_CTL_EVENT_DISCONNECTED = 1;
static constexpr int NET_CTL_EVENT_IPOBTAINED = 3;
static constexpr int NET_CTL_STUN_STATUS_FAILED = 1;
static constexpr int NET_CTL_STUN_STATUS_OK = 2;
static constexpr int NET_CTL_NAT_TYPE_NONE = 0;
static constexpr int NET_CTL_NAT_TYPE_1 = 1;
static constexpr int NET_CTL_NAT_TYPE_2 = 2;
static constexpr int NET_CTL_NAT_TYPE_3 = 3;
static constexpr std::uint16_t STUN_BINDING_REQUEST = 0x0001;
static constexpr std::uint16_t STUN_BINDING_RESPONSE = 0x0101;
static constexpr std::uint32_t STUN_MAGIC_COOKIE = 0x2112A442u;
static constexpr std::uint16_t STUN_ATTR_MAPPED_ADDRESS = 0x0001;
static constexpr std::uint16_t STUN_ATTR_CHANGED_ADDRESS = 0x0005;
static constexpr std::uint16_t STUN_ATTR_XOR_MAPPED_ADDRESS = 0x0020;
static constexpr std::uint16_t STUN_ATTR_XOR_MAPPED_ADDRESS_LEGACY = 0x8020;
static constexpr std::uint16_t STUN_ATTR_OTHER_ADDRESS = 0x802b;
static constexpr std::size_t STUN_HEADER_SIZE = 20;
static constexpr std::size_t STUN_TRANSACTION_ID_SIZE = 12;
static constexpr int STUN_DEFAULT_TIMEOUT_MS = 300;
static constexpr int STUN_TOTAL_BUDGET_MS = 1000;
static constexpr int STUN_CACHE_TTL_MS = 60000;
static constexpr int NET_CTL_INFO_DEVICE = 1;
static constexpr int NET_CTL_INFO_ETHER_ADDR = 2;
static constexpr int NET_CTL_INFO_MTU = 3;
static constexpr int NET_CTL_INFO_LINK = 4;
static constexpr int NET_CTL_INFO_BSSID = 5;
static constexpr int NET_CTL_INFO_SSID = 6;
static constexpr int NET_CTL_INFO_WIFI_SECURITY = 7;
static constexpr int NET_CTL_INFO_RSSI_DBM = 8;
static constexpr int NET_CTL_INFO_RSSI_PERCENTAGE = 9;
static constexpr int NET_CTL_INFO_CHANNEL = 10;
static constexpr int NET_CTL_INFO_IP_CONFIG = 11;
static constexpr int NET_CTL_INFO_DHCP_HOSTNAME = 12;
static constexpr int NET_CTL_INFO_PPPOE_AUTH_NAME = 13;
static constexpr int NET_CTL_INFO_IP_ADDRESS = 14;
static constexpr int NET_CTL_INFO_NETMASK = 15;
static constexpr int NET_CTL_INFO_DEFAULT_ROUTE = 16;
static constexpr int NET_CTL_INFO_PRIMARY_DNS = 17;
static constexpr int NET_CTL_INFO_SECONDARY_DNS = 18;
static constexpr int NET_CTL_INFO_HTTP_PROXY_CONFIG = 19;
static constexpr int NET_CTL_INFO_HTTP_PROXY_SERVER = 20;
static constexpr int NET_CTL_INFO_HTTP_PROXY_PORT = 21;
static constexpr int NET_CTL_INFO_V6_IP_ADDRESS = 1;
static constexpr int NET_CTL_INFO_V6_SECONDARY_DNS = 5;
static constexpr std::uint32_t NET_CTL_DEVICE_WIRED = 0;
static constexpr std::uint32_t NET_CTL_LINK_CONNECTED = 1;
static constexpr std::uint32_t NET_CTL_IP_DHCP = 0;
static constexpr std::uint32_t NET_CTL_HTTP_PROXY_OFF = 0;
static constexpr std::uint8_t NET_CTL_ETHER_ADDR[6] = {0x02, 0x50, 0x53, 0x35, 0x00, 0x01};
static constexpr std::size_t NET_CTL_IPV4_ADDR_STR_LEN = 16;
static constexpr int MAX_CALLBACKS = 8;

struct CallbackSlot {
    NetCtlCallback func = nullptr;
    void* arg = nullptr;
    int lastEvent = 0;
};

static std::atomic_bool g_initialized{true};
static std::mutex g_callbackLock;
static CallbackSlot g_callbacks[MAX_CALLBACKS] = {};
static CallbackSlot g_callbacksV6[MAX_CALLBACKS] = {};
static CallbackSlot g_callbacksNpToolkit[MAX_CALLBACKS] = {};

static int RegisterCallback(CallbackSlot (&callbacks)[MAX_CALLBACKS], NetCtlCallback func, void* arg, int* cid) {
    if (!g_initialized.load()) return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
    if (!func || !cid) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    std::lock_guard lock(g_callbackLock);
    for (int index = 0; index < MAX_CALLBACKS; ++index) {
        if (!callbacks[index].func) {
            callbacks[index] = {func, arg, 0};
            *cid = index;
            return 0;
        }
    }
    return SCE_NET_CTL_ERROR_CALLBACK_MAX;
}

static int UnregisterCallback(CallbackSlot (&callbacks)[MAX_CALLBACKS], int cid) {
    if (!g_initialized.load()) return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
    if (cid < 0 || cid >= MAX_CALLBACKS) return SCE_NET_CTL_ERROR_INVALID_ID;
    std::lock_guard lock(g_callbackLock);
    if (!callbacks[cid].func) return SCE_NET_CTL_ERROR_ID_NOT_FOUND;
    callbacks[cid] = {};
    return 0;
}

static bool HasCallback(const CallbackSlot (&callbacks)[MAX_CALLBACKS]) {
    std::lock_guard lock(g_callbackLock);
    for (const auto& slot : callbacks) {
        if (slot.func) return true;
    }
    return false;
}

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

struct HostIpv4Interface {
    in_addr address{};
    in_addr netmask{};
    in_addr gateway{};
    std::uint32_t mtu = 0;
    std::vector<in_addr> dns;
};

#ifdef _WIN32
namespace {

struct HostAddressCache {
    std::mutex lock;
    bool registered = false;
    bool tried = false;
    std::atomic<std::uint64_t> generation{1};
    std::uint64_t seen[3]{};
    bool value[3]{};
    std::uint64_t interfaceSeen = 0;
    bool interfaceFound = false;
    HostIpv4Interface interfaceValue;
};

HostAddressCache& AddressCache() {
    static HostAddressCache cache;
    return cache;
}

void CALLBACK HostAddressChanged(PVOID, PMIB_UNICASTIPADDRESS_ROW, MIB_NOTIFICATION_TYPE) {
    AddressCache().generation.fetch_add(1, std::memory_order_acq_rel);
}

void CALLBACK HostInterfaceChanged(PVOID, PMIB_IPINTERFACE_ROW, MIB_NOTIFICATION_TYPE) {
    AddressCache().generation.fetch_add(1, std::memory_order_acq_rel);
}

void CALLBACK HostRouteChanged(PVOID, PMIB_IPFORWARD_ROW2, MIB_NOTIFICATION_TYPE) {
    AddressCache().generation.fetch_add(1, std::memory_order_acq_rel);
}

void WatchHostAddresses(HostAddressCache& cache) {
    if (cache.tried) return;
    cache.tried = true;
    HANDLE addresses = nullptr;
    HANDLE interfaces = nullptr;
    HANDLE routes = nullptr;
    cache.registered = NotifyUnicastIpAddressChange(AF_UNSPEC, HostAddressChanged, nullptr, FALSE, &addresses) == NO_ERROR &&
                       NotifyIpInterfaceChange(AF_UNSPEC, HostInterfaceChanged, nullptr, FALSE, &interfaces) == NO_ERROR &&
                       NotifyRouteChange2(AF_UNSPEC, HostRouteChanged, nullptr, FALSE, &routes) == NO_ERROR;
    if (!cache.registered) {
        if (addresses != nullptr) CancelMibChangeNotify2(addresses);
        if (interfaces != nullptr) CancelMibChangeNotify2(interfaces);
        if (routes != nullptr) CancelMibChangeNotify2(routes);
    }
}

}
#endif

static bool HostHasAddress(int family) {
#ifdef _WIN32
    auto& cache = AddressCache();
    const int slot = family == AF_INET ? 0 : family == AF_INET6 ? 1 : 2;
    std::lock_guard guard(cache.lock);
    WatchHostAddresses(cache);
    const auto generation = cache.generation.load(std::memory_order_acquire);
    if (cache.registered && cache.seen[slot] == generation) return cache.value[slot];
    const bool value = QueryHostHasAddress(family);
    cache.value[slot] = value;
    cache.seen[slot] = generation;
    return value;
#else
    return QueryHostHasAddress(family);
#endif
}

static bool UsableIpv4(in_addr address) {
    const auto host = ntohl(address.s_addr);
    return host != 0 && (host >> 24) != 127;
}

#ifdef _WIN32
static const sockaddr_in* Ipv4Of(const SOCKET_ADDRESS& address) {
    if (!address.lpSockaddr || address.lpSockaddr->sa_family != AF_INET) return nullptr;
    return reinterpret_cast<const sockaddr_in*>(address.lpSockaddr);
}

static std::uint32_t PrefixMask(unsigned length) {
    if (length == 0) return 0;
    return htonl(length >= 32 ? 0xFFFFFFFFu : 0xFFFFFFFFu << (32 - length));
}
#else
struct HostDefaultRoute {
    std::string name;
    in_addr gateway{};
    unsigned long metric = 0;
};

#ifdef __APPLE__
struct NetCtlDefaultRoute {
    char name[IF_NAMESIZE];
    in_addr gateway;
    std::uint32_t metric;
};

extern "C" std::size_t NetCtlDefaultRoutes_nid_no_patch(const char* table, std::size_t size, NetCtlDefaultRoute* routes, std::size_t capacity) {
    std::size_t count = 0;
    for (std::size_t offset = 0; offset + sizeof(rt_msghdr) <= size;) {
        const auto* header = reinterpret_cast<const rt_msghdr*>(table + offset);
        if (header->rtm_msglen < sizeof(rt_msghdr) || header->rtm_msglen > size - offset) break;
        const sockaddr* addresses[RTAX_MAX] = {};
        const std::size_t end = offset + header->rtm_msglen;
        std::size_t field = offset + sizeof(rt_msghdr);
        for (int index = 0; index < RTAX_MAX && field + sizeof(sockaddr) <= end; ++index) {
            if (!(header->rtm_addrs & (1 << index))) continue;
            const auto* address = reinterpret_cast<const sockaddr*>(table + field);
            addresses[index] = address;
            field += address->sa_len ? (address->sa_len + sizeof(std::uint32_t) - 1) & ~(sizeof(std::uint32_t) - 1) : sizeof(std::uint32_t);
        }
        offset = end;
        const auto* destination = addresses[RTAX_DST];
        const auto* gateway = addresses[RTAX_GATEWAY];
        if (!destination || destination->sa_family != AF_INET || !gateway || gateway->sa_family != AF_INET) continue;
        if (reinterpret_cast<const sockaddr_in*>(destination)->sin_addr.s_addr != 0) continue;
        if (!(header->rtm_flags & RTF_UP) || (header->rtm_flags & (RTF_REJECT | RTF_HOST))) continue;
        NetCtlDefaultRoute route{};
        if (!if_indextoname(header->rtm_index, route.name)) continue;
        route.gateway = reinterpret_cast<const sockaddr_in*>(gateway)->sin_addr;
        route.metric = (header->rtm_flags & RTF_IFSCOPE) ? 1 : 0;
        if (count < capacity) routes[count] = route;
        ++count;
    }
    return count;
}

static std::vector<HostDefaultRoute> HostDefaultRoutes() {
    int name[] = {CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_FLAGS, RTF_GATEWAY};
    std::size_t size = 0;
    if (sysctl(name, 6, nullptr, &size, nullptr, 0) != 0) throw std::runtime_error("sceNetCtlGetInfo: the host routing table is unavailable");
    std::vector<char> table(size);
    if (sysctl(name, 6, table.data(), &size, nullptr, 0) != 0) throw std::runtime_error("sceNetCtlGetInfo: the host routing table is unavailable");
    std::vector<NetCtlDefaultRoute> found(NetCtlDefaultRoutes_nid_no_patch(table.data(), size, nullptr, 0));
    found.resize(NetCtlDefaultRoutes_nid_no_patch(table.data(), size, found.data(), found.size()));
    std::vector<HostDefaultRoute> routes;
    for (const auto& entry : found) routes.push_back({entry.name, entry.gateway, entry.metric});
    return routes;
}
#else
static std::vector<HostDefaultRoute> HostDefaultRoutes() {
    std::ifstream table("/proc/net/route");
    if (!table) throw std::runtime_error("sceNetCtlGetInfo: the host routing table is unavailable");
    std::vector<HostDefaultRoute> routes;
    std::string line;
    std::getline(table, line);
    while (std::getline(table, line)) {
        std::istringstream fields(line);
        HostDefaultRoute route;
        unsigned long destination = 0;
        unsigned long gateway = 0;
        unsigned long flags = 0;
        unsigned long references = 0;
        unsigned long uses = 0;
        unsigned long mask = 0;
        if (!(fields >> route.name >> std::hex >> destination >> gateway >> flags >> std::dec >> references >> uses >> route.metric >> std::hex >> mask)) continue;
        if (destination != 0 || mask != 0 || !(flags & RTF_UP) || (flags & RTF_REJECT)) continue;
        route.gateway.s_addr = static_cast<std::uint32_t>(gateway);
        routes.push_back(route);
    }
    return routes;
}
#endif

static std::uint32_t HostInterfaceMtu(const char* name) {
    const int descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (descriptor < 0) throw std::runtime_error("sceNetCtlGetInfo: no socket to read the host interface MTU");
    ifreq request{};
    std::strncpy(request.ifr_name, name, IFNAMSIZ - 1);
    const int result = ioctl(descriptor, SIOCGIFMTU, &request);
    close(descriptor);
    if (result != 0) throw std::runtime_error(std::string("sceNetCtlGetInfo: no MTU for host interface ") + name);
    return static_cast<std::uint32_t>(request.ifr_mtu);
}

static std::vector<in_addr> HostDnsServers() {
    std::ifstream config("/etc/resolv.conf");
    std::vector<in_addr> servers;
    std::string line;
    while (std::getline(config, line)) {
        std::istringstream fields(line);
        std::string keyword;
        std::string value;
        in_addr server{};
        if (fields >> keyword >> value && keyword == "nameserver" && inet_pton(AF_INET, value.c_str(), &server) == 1) servers.push_back(server);
    }
    return servers;
}
#endif

static bool QueryHostIpv4Interface(HostIpv4Interface& found) {
#ifdef _WIN32
    ULONG size = 0;
    const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_INCLUDE_GATEWAYS;
    if (GetAdaptersAddresses(AF_INET, flags, nullptr, nullptr, &size) != ERROR_BUFFER_OVERFLOW) return false;
    std::vector<std::uint8_t> storage(size);
    auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data());
    if (GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &size) != NO_ERROR) return false;
    bool chosen = false;
    bool chosenHasGateway = false;
    ULONG chosenMetric = 0;
    for (auto* adapter = adapters; adapter; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        auto* unicast = adapter->FirstUnicastAddress;
        while (unicast && !(Ipv4Of(unicast->Address) && UsableIpv4(Ipv4Of(unicast->Address)->sin_addr))) unicast = unicast->Next;
        if (!unicast) continue;
        auto* gateway = adapter->FirstGatewayAddress;
        while (gateway && !Ipv4Of(gateway->Address)) gateway = gateway->Next;
        if (chosen && !(gateway && (!chosenHasGateway || adapter->Ipv4Metric < chosenMetric))) continue;
        HostIpv4Interface candidate;
        candidate.address = Ipv4Of(unicast->Address)->sin_addr;
        candidate.netmask.s_addr = PrefixMask(unicast->OnLinkPrefixLength);
        if (gateway) candidate.gateway = Ipv4Of(gateway->Address)->sin_addr;
        candidate.mtu = adapter->Mtu;
        for (auto* server = adapter->FirstDnsServerAddress; server; server = server->Next) {
            if (Ipv4Of(server->Address)) candidate.dns.push_back(Ipv4Of(server->Address)->sin_addr);
        }
        found = std::move(candidate);
        chosen = true;
        chosenHasGateway = gateway != nullptr;
        chosenMetric = adapter->Ipv4Metric;
    }
    return chosen;
#else
    ifaddrs* list = nullptr;
    if (getifaddrs(&list) != 0) return false;
    const std::unique_ptr<ifaddrs, void (*)(ifaddrs*)> interfaces(list, freeifaddrs);
    const auto routes = HostDefaultRoutes();
    const ifaddrs* chosen = nullptr;
    const HostDefaultRoute* chosenRoute = nullptr;
    for (auto* item = list; item; item = item->ifa_next) {
        if (!item->ifa_addr || item->ifa_addr->sa_family != AF_INET || !(item->ifa_flags & IFF_UP) || (item->ifa_flags & IFF_LOOPBACK)) continue;
        if (!UsableIpv4(reinterpret_cast<const sockaddr_in*>(item->ifa_addr)->sin_addr)) continue;
        const HostDefaultRoute* route = nullptr;
        for (const auto& entry : routes) {
            if (entry.name == item->ifa_name && (!route || entry.metric < route->metric)) route = &entry;
        }
        if (chosen && !(route && (!chosenRoute || route->metric < chosenRoute->metric))) continue;
        chosen = item;
        chosenRoute = route;
    }
    if (!chosen) return false;
    found.address = reinterpret_cast<const sockaddr_in*>(chosen->ifa_addr)->sin_addr;
    if (chosen->ifa_netmask) found.netmask = reinterpret_cast<const sockaddr_in*>(chosen->ifa_netmask)->sin_addr;
    if (chosenRoute) found.gateway = chosenRoute->gateway;
    found.mtu = HostInterfaceMtu(chosen->ifa_name);
    found.dns = HostDnsServers();
    return true;
#endif
}

static bool FindHostIpv4Interface(HostIpv4Interface& found) {
#ifdef _WIN32
    auto& cache = AddressCache();
    std::lock_guard guard(cache.lock);
    WatchHostAddresses(cache);
    const auto generation = cache.generation.load(std::memory_order_acquire);
    if (!cache.registered || cache.interfaceSeen != generation) {
        HostIpv4Interface value;
        cache.interfaceFound = QueryHostIpv4Interface(value);
        cache.interfaceValue = std::move(value);
        cache.interfaceSeen = generation;
    }
    if (cache.interfaceFound) found = cache.interfaceValue;
    return cache.interfaceFound;
#else
    return QueryHostIpv4Interface(found);
#endif
}

extern "C" std::size_t NetCtlHostDnsServers_nid_no_patch(std::uint32_t* servers, std::size_t capacity) {
    HostIpv4Interface host;
    if (!FindHostIpv4Interface(host)) return 0;
    for (std::size_t index = 0; index < host.dns.size() && index < capacity; ++index) servers[index] = host.dns[index].s_addr;
    return host.dns.size();
}

#ifdef _WIN32
using StunSocket = SOCKET;
static constexpr StunSocket INVALID_STUN_SOCKET = INVALID_SOCKET;
static void CloseStunSocket(StunSocket descriptor) { closesocket(descriptor); }
static bool EnsureSocketRuntime() {
    static const int status = [] {
        WSADATA data{};
        return WSAStartup(MAKEWORD(2, 2), &data);
    }();
    return status == 0;
}
#else
using StunSocket = int;
static constexpr StunSocket INVALID_STUN_SOCKET = -1;
static void CloseStunSocket(StunSocket descriptor) { close(descriptor); }
static bool EnsureSocketRuntime() { return true; }
#endif

struct StunEndpoint {
    std::uint32_t address = 0;
    std::uint16_t port = 0;
    bool valid = false;
};

struct StunProbeResponse {
    StunEndpoint mapped;
    StunEndpoint other;
};

struct StunConfig {
    std::mutex lock;
    bool overrideActive = false;
    std::vector<sockaddr_in> overrideServers;
    int overrideTimeoutMs = STUN_DEFAULT_TIMEOUT_MS;
    std::uint64_t configGeneration = 1;
    std::uint64_t cachedConfigGeneration = 0;
    std::uint32_t cachedLocalAddr = 0;
    std::chrono::steady_clock::time_point cachedUntil{};
    bool hasCachedResult = false;
    NetCtlNatInfo cachedResult{};
};

static StunConfig& GetStunConfig() {
    static StunConfig config;
    return config;
}

extern "C" void NetCtlSetStunServers_nid_no_patch(const sockaddr_in* servers, std::size_t count, int timeoutMs) {
    auto& config = GetStunConfig();
    std::lock_guard guard(config.lock);
    ++config.configGeneration;
    config.hasCachedResult = false;
    config.overrideServers.clear();
    if (!servers && count == 0 && timeoutMs <= 0) {
        config.overrideActive = false;
        config.overrideTimeoutMs = STUN_DEFAULT_TIMEOUT_MS;
        return;
    }
    config.overrideActive = true;
    config.overrideTimeoutMs = timeoutMs > 0 ? timeoutMs : STUN_DEFAULT_TIMEOUT_MS;
    if (servers && count > 0) config.overrideServers.assign(servers, servers + count);
}

static void AppendUniqueStunServer(std::vector<sockaddr_in>& servers, const sockaddr_in& candidate) {
    if (candidate.sin_family != AF_INET || candidate.sin_addr.s_addr == 0 || candidate.sin_port == 0) return;
    for (const auto& existing : servers) {
        if (existing.sin_addr.s_addr == candidate.sin_addr.s_addr && existing.sin_port == candidate.sin_port) return;
    }
    servers.push_back(candidate);
}

static std::vector<sockaddr_in> ResolveStunServers(int& timeoutMs, std::uint64_t& generationOut, std::chrono::steady_clock::time_point totalDeadline) {
    auto& config = GetStunConfig();
    {
        std::lock_guard guard(config.lock);
        generationOut = config.configGeneration;
        if (config.overrideActive) {
            timeoutMs = config.overrideTimeoutMs;
            return config.overrideServers;
        }
    }
    timeoutMs = STUN_DEFAULT_TIMEOUT_MS;
    struct DefaultStunHost {
        const char* host;
        const char* port;
    };
    static constexpr DefaultStunHost kDefaultHosts[] = {
        {"stun.playstation.net", "3478"},
        {"stun.cloudflare.com", "3478"},
        {"stun.l.google.com", "19302"},
        {"stun1.l.google.com", "19302"},
    };
    struct DnsQueryState {
        std::mutex lock;
        std::condition_variable cv;
        std::vector<sockaddr_in> servers;
        bool done = false;
    };
    const auto now = std::chrono::steady_clock::now();
    if (!EnsureSocketRuntime() || now >= totalDeadline) return {};
    const auto dnsCap = now + std::chrono::milliseconds(STUN_DEFAULT_TIMEOUT_MS * 2);
    const auto dnsDeadline = dnsCap < totalDeadline ? dnsCap : totalDeadline;
    auto state = std::make_shared<DnsQueryState>();
    std::thread([state, dnsDeadline] {
        for (const auto& entry : kDefaultHosts) {
            if (std::chrono::steady_clock::now() >= dnsDeadline) break;
            addrinfo hints{};
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_DGRAM;
            hints.ai_protocol = IPPROTO_UDP;
            addrinfo* list = nullptr;
            if (getaddrinfo(entry.host, entry.port, &hints, &list) != 0 || !list) continue;
            {
                std::lock_guard guard(state->lock);
                for (auto* item = list; item; item = item->ai_next) {
                    if (item->ai_addr && item->ai_addrlen >= sizeof(sockaddr_in) && item->ai_addr->sa_family == AF_INET) {
                        AppendUniqueStunServer(state->servers, *reinterpret_cast<const sockaddr_in*>(item->ai_addr));
                    }
                }
            }
            freeaddrinfo(list);
        }
        {
            std::lock_guard guard(state->lock);
            state->done = true;
        }
        state->cv.notify_one();
    }).detach();
    std::unique_lock lock(state->lock);
    state->cv.wait_until(lock, dnsDeadline, [&] { return state->done; });
    return state->servers;
}

static StunEndpoint DecodeStunIpv4Endpoint(const std::uint8_t* value, std::uint16_t length, bool xorMask) {
    StunEndpoint endpoint{};
    if (length < 8 || value[1] != 0x01) return endpoint;
    const auto rawPort = static_cast<std::uint16_t>((static_cast<std::uint16_t>(value[2]) << 8) | value[3]);
    std::uint32_t rawAddress = 0;
    std::memcpy(&rawAddress, value + 4, sizeof(rawAddress));
    endpoint.port = xorMask ? static_cast<std::uint16_t>(rawPort ^ static_cast<std::uint16_t>(STUN_MAGIC_COOKIE >> 16)) : rawPort;
    endpoint.address = xorMask ? (rawAddress ^ htonl(STUN_MAGIC_COOKIE)) : rawAddress;
    endpoint.valid = endpoint.address != 0 && endpoint.port != 0;
    return endpoint;
}

static bool ParseStunBindingResponse(const std::uint8_t* packet, std::size_t size, const std::uint8_t* transactionId, StunProbeResponse& response) {
    if (size < STUN_HEADER_SIZE) return false;
    const auto type = static_cast<std::uint16_t>((static_cast<std::uint16_t>(packet[0]) << 8) | packet[1]);
    const auto length = static_cast<std::uint16_t>((static_cast<std::uint16_t>(packet[2]) << 8) | packet[3]);
    if (type != STUN_BINDING_RESPONSE || (length & 3u) != 0 || STUN_HEADER_SIZE + length > size) return false;
    const std::uint32_t expectedCookie = htonl(STUN_MAGIC_COOKIE);
    if (std::memcmp(packet + 4, &expectedCookie, sizeof(expectedCookie)) != 0) return false;
    if (std::memcmp(packet + 8, transactionId, STUN_TRANSACTION_ID_SIZE) != 0) return false;
    response = {};
    bool hasXorMapped = false;
    const std::size_t end = STUN_HEADER_SIZE + length;
    for (std::size_t offset = STUN_HEADER_SIZE; offset < end;) {
        if (offset + 4 > end) return false;
        const auto attrType = static_cast<std::uint16_t>((static_cast<std::uint16_t>(packet[offset]) << 8) | packet[offset + 1]);
        const auto attrLen = static_cast<std::uint16_t>((static_cast<std::uint16_t>(packet[offset + 2]) << 8) | packet[offset + 3]);
        const std::size_t valueOffset = offset + 4;
        if (valueOffset + attrLen > end) return false;
        const std::uint8_t* value = packet + valueOffset;
        if (attrType == STUN_ATTR_XOR_MAPPED_ADDRESS || attrType == STUN_ATTR_XOR_MAPPED_ADDRESS_LEGACY) {
            const auto endpoint = DecodeStunIpv4Endpoint(value, attrLen, true);
            if (endpoint.valid) {
                response.mapped = endpoint;
                hasXorMapped = true;
            }
        } else if (attrType == STUN_ATTR_MAPPED_ADDRESS && !hasXorMapped) {
            const auto endpoint = DecodeStunIpv4Endpoint(value, attrLen, false);
            if (endpoint.valid) response.mapped = endpoint;
        } else if (attrType == STUN_ATTR_OTHER_ADDRESS || attrType == STUN_ATTR_CHANGED_ADDRESS) {
            const auto endpoint = DecodeStunIpv4Endpoint(value, attrLen, false);
            if (endpoint.valid) response.other = endpoint;
        }
        offset = valueOffset + ((static_cast<std::size_t>(attrLen) + 3u) & ~std::size_t(3u));
    }
    return response.mapped.valid;
}

static bool ProbeStunServer(StunSocket descriptor, const sockaddr_in& server, int timeoutMs, std::chrono::steady_clock::time_point totalDeadline, StunProbeResponse& response) {
    const auto start = std::chrono::steady_clock::now();
    if (start >= totalDeadline) return false;
    std::uint8_t request[STUN_HEADER_SIZE] = {};
    request[0] = static_cast<std::uint8_t>(STUN_BINDING_REQUEST >> 8);
    request[1] = static_cast<std::uint8_t>(STUN_BINDING_REQUEST & 0xFF);
    const std::uint32_t cookie = htonl(STUN_MAGIC_COOKIE);
    std::memcpy(request + 4, &cookie, sizeof(cookie));
    HostEntropy_nid_no_patch(request + 8, STUN_TRANSACTION_ID_SIZE);
    const auto sent = sendto(descriptor, reinterpret_cast<const char*>(request), sizeof(request), 0,
                             reinterpret_cast<const sockaddr*>(&server), sizeof(server));
    if (sent != static_cast<int>(sizeof(request))) return false;
    const auto probeDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs > 0 ? timeoutMs : 1);
    const auto deadline = probeDeadline < totalDeadline ? probeDeadline : totalDeadline;
    for (;;) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) return false;
        const auto remainingUs = std::chrono::duration_cast<std::chrono::microseconds>(deadline - now).count();
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(descriptor, &readSet);
        timeval tv{};
        tv.tv_sec = static_cast<long>(remainingUs / 1000000);
        tv.tv_usec = static_cast<long>(remainingUs % 1000000);
        if (tv.tv_sec == 0 && tv.tv_usec == 0) tv.tv_usec = 1000;
#ifdef _WIN32
        const int ready = select(0, &readSet, nullptr, nullptr, &tv);
#else
        const int ready = select(descriptor + 1, &readSet, nullptr, nullptr, &tv);
#endif
        if (ready <= 0) return false;
        std::uint8_t buffer[512];
        sockaddr_in from{};
#ifdef _WIN32
        int fromLen = sizeof(from);
#else
        socklen_t fromLen = sizeof(from);
#endif
        const auto received = recvfrom(descriptor, reinterpret_cast<char*>(buffer), sizeof(buffer), 0,
                                       reinterpret_cast<sockaddr*>(&from), &fromLen);
#ifdef _WIN32
        if (received < 0) {
            if (WSAGetLastError() == WSAECONNRESET) continue;
            return false;
        }
#else
        if (received < 0) return false;
#endif
        if (received == 0) return false;
        if (ParseStunBindingResponse(buffer, static_cast<std::size_t>(received), request + 8, response)) return true;
    }
}

static NetCtlNatInfo DiscoverNatInfo(in_addr localAddress) {
    NetCtlNatInfo result{};
    result.size = sizeof(NetCtlNatInfo);
    result.stun_status = NET_CTL_STUN_STATUS_FAILED;
    result.nat_type = NET_CTL_NAT_TYPE_NONE;
    result.mapped_addr = 0;

    const auto totalDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(STUN_TOTAL_BUDGET_MS);
    int timeoutMs = STUN_DEFAULT_TIMEOUT_MS;
    std::uint64_t generation = 0;
    const auto servers = ResolveStunServers(timeoutMs, generation, totalDeadline);
    if (servers.empty() || !EnsureSocketRuntime()) return result;

    const StunSocket descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (descriptor == INVALID_STUN_SOCKET) return result;

#ifdef _WIN32
    BOOL newBehavior = FALSE;
    DWORD bytesReturned = 0;
    WSAIoctl(descriptor, _WSAIOW(IOC_VENDOR, 12), &newBehavior, sizeof(newBehavior), nullptr, 0, &bytesReturned, nullptr, nullptr);
#endif

    sockaddr_in bindAddr{};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr = (ntohl(servers[0].sin_addr.s_addr) >> 24) == 127 ? servers[0].sin_addr : localAddress;
    bindAddr.sin_port = 0;
    if (bind(descriptor, reinterpret_cast<const sockaddr*>(&bindAddr), sizeof(bindAddr)) != 0) {
        CloseStunSocket(descriptor);
        return result;
    }

    sockaddr_in boundAddr{};
#ifdef _WIN32
    int boundLen = sizeof(boundAddr);
#else
    socklen_t boundLen = sizeof(boundAddr);
#endif
    if (getsockname(descriptor, reinterpret_cast<sockaddr*>(&boundAddr), &boundLen) != 0) {
        CloseStunSocket(descriptor);
        return result;
    }
    const std::uint16_t localPort = ntohs(boundAddr.sin_port);

    StunProbeResponse firstResponse{};
    sockaddr_in primaryServer{};
    std::size_t primaryIndex = servers.size();
    for (std::size_t i = 0; i < servers.size(); ++i) {
        if (ProbeStunServer(descriptor, servers[i], timeoutMs, totalDeadline, firstResponse)) {
            primaryServer = servers[i];
            primaryIndex = i;
            break;
        }
    }
    if (primaryIndex == servers.size()) {
        CloseStunSocket(descriptor);
        return result;
    }

    if (firstResponse.mapped.address == localAddress.s_addr && firstResponse.mapped.port == localPort) {
        CloseStunSocket(descriptor);
        result.stun_status = NET_CTL_STUN_STATUS_OK;
        result.nat_type = NET_CTL_NAT_TYPE_1;
        result.mapped_addr = firstResponse.mapped.address;
        return result;
    }

    std::vector<sockaddr_in> secondaryServers;
    if (firstResponse.other.valid) {
        sockaddr_in otherServer{};
        otherServer.sin_family = AF_INET;
        otherServer.sin_addr.s_addr = firstResponse.other.address;
        otherServer.sin_port = htons(firstResponse.other.port);
        if (otherServer.sin_addr.s_addr != primaryServer.sin_addr.s_addr || otherServer.sin_port != primaryServer.sin_port) {
            AppendUniqueStunServer(secondaryServers, otherServer);
        }
        if (otherServer.sin_port != primaryServer.sin_port) {
            sockaddr_in sameIpOtherPort = primaryServer;
            sameIpOtherPort.sin_port = otherServer.sin_port;
            AppendUniqueStunServer(secondaryServers, sameIpOtherPort);
        }
    }
    for (std::size_t i = 0; i < servers.size(); ++i) {
        if (i == primaryIndex) continue;
        if (servers[i].sin_addr.s_addr != primaryServer.sin_addr.s_addr || servers[i].sin_port != primaryServer.sin_port) {
            AppendUniqueStunServer(secondaryServers, servers[i]);
        }
    }

    for (const auto& secondary : secondaryServers) {
        StunProbeResponse secondResponse{};
        if (ProbeStunServer(descriptor, secondary, timeoutMs, totalDeadline, secondResponse)) {
            CloseStunSocket(descriptor);
            result.stun_status = NET_CTL_STUN_STATUS_OK;
            result.nat_type = (secondResponse.mapped.address == firstResponse.mapped.address &&
                               secondResponse.mapped.port == firstResponse.mapped.port)
                                  ? NET_CTL_NAT_TYPE_2
                                  : NET_CTL_NAT_TYPE_3;
            result.mapped_addr = firstResponse.mapped.address;
            return result;
        }
    }

    CloseStunSocket(descriptor);
    return result;
}

extern "C" bool NetCtlHostIpv4Address_nid_no_patch(std::uint32_t* address, std::uint32_t* netmask, std::uint32_t* mtu) {
    HostIpv4Interface host;
    if (!FindHostIpv4Interface(host)) return false;
    *address = host.address.s_addr;
    *netmask = host.netmask.s_addr;
    *mtu = host.mtu;
    return true;
}

static void WriteInfoWord(NetCtlInfo* info, std::uint32_t value) {
    std::memcpy(info->opaque, &value, sizeof(value));
}

static void WriteInfoAddress(NetCtlInfo* info, in_addr address) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&address.s_addr);
    char text[NET_CTL_IPV4_ADDR_STR_LEN] = {};
    std::snprintf(text, sizeof(text), "%u.%u.%u.%u", static_cast<unsigned>(bytes[0]), static_cast<unsigned>(bytes[1]), static_cast<unsigned>(bytes[2]), static_cast<unsigned>(bytes[3]));
    std::memcpy(info->opaque, text, sizeof(text));
}

extern "C" std::uint64_t NetCtlReadByteCounter_nid_no_patch(const char* path) {
    std::ifstream file(path);
    std::uint64_t value = 0;
    if (!(file >> value)) throw std::runtime_error(std::string("sceNetCtlGetIfStat: no byte counter in ") + path);
    return value;
}

static void HostTraffic(std::uint64_t& sent, std::uint64_t& received) {
    sent = 0;
    received = 0;
#ifdef _WIN32
    MIB_IF_TABLE2* table = nullptr;
    if (GetIfTable2(&table) != NO_ERROR) throw std::runtime_error("sceNetCtlGetIfStat: the host interface table is unavailable");
    for (ULONG index = 0; index < table->NumEntries; ++index) {
        const auto& row = table->Table[index];
        if (row.OperStatus != IfOperStatusUp || row.Type == IF_TYPE_SOFTWARE_LOOPBACK || !row.InterfaceAndOperStatusFlags.HardwareInterface) continue;
        sent += row.OutOctets;
        received += row.InOctets;
    }
    FreeMibTable(table);
#elif defined(__linux__)
    ifaddrs* interfaces = nullptr;
    if (getifaddrs(&interfaces) != 0) throw std::runtime_error("sceNetCtlGetIfStat: the host interface list is unavailable");
    for (auto* item = interfaces; item; item = item->ifa_next) {
        if (!item->ifa_addr || item->ifa_addr->sa_family != AF_PACKET) continue;
        if (!(item->ifa_flags & IFF_UP) || (item->ifa_flags & IFF_LOOPBACK)) continue;
        const std::string statistics = std::string("/sys/class/net/") + item->ifa_name + "/statistics/";
        sent += NetCtlReadByteCounter_nid_no_patch((statistics + "tx_bytes").c_str());
        received += NetCtlReadByteCounter_nid_no_patch((statistics + "rx_bytes").c_str());
    }
    freeifaddrs(interfaces);
#else
    throw std::runtime_error("sceNetCtlGetIfStat: host interface byte counters are not implemented on this platform");
#endif
}

static int CheckCallbacks(CallbackSlot (&callbacks)[MAX_CALLBACKS], bool includeV6 = false) {
    if (!g_initialized.load()) return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
    const bool primary = HasCallback(callbacks);
    const bool ipv6 = includeV6 && HasCallback(g_callbacksV6);
    if (!primary && !ipv6) return 0;
    const int primaryEvent = primary ? (HostHasAddress(AF_UNSPEC) ? NET_CTL_EVENT_IPOBTAINED : NET_CTL_EVENT_DISCONNECTED) : 0;
    const int ipv6Event = ipv6 ? (HostHasAddress(AF_INET6) ? NET_CTL_EVENT_IPOBTAINED : NET_CTL_EVENT_DISCONNECTED) : 0;
    struct Delivery {
        CallbackSlot callback;
        int event;
    };
    Delivery due[2 * MAX_CALLBACKS];
    int count = 0;
    {
        std::lock_guard lock(g_callbackLock);
        const auto collect = [&](CallbackSlot (&slots)[MAX_CALLBACKS], int event) {
            if (event == 0) return;
            for (auto& slot : slots) {
                if (!slot.func || slot.lastEvent == event) continue;
                slot.lastEvent = event;
                due[count++] = {slot, event};
            }
        };
        collect(callbacks, primaryEvent);
        if (includeV6) collect(g_callbacksV6, ipv6Event);
    }
    for (int index = 0; index < count; ++index)
        due[index].callback.func(due[index].event, due[index].callback.arg);
    return 0;
}

extern "C" {

int APS5_VABI sceNetCtlCheckCallback(void) {
    return CheckCallbacks(g_callbacks, true);
}

int APS5_VABI sceNetCtlGetIfStat(NetCtlIfStat* stat) {
    if (!g_initialized.load()) return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
    if (!stat) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    if (!HostHasAddress(AF_UNSPEC)) return SCE_NET_CTL_ERROR_NOT_CONNECTED;
    *stat = {};
    HostTraffic(stat->tx_bytes, stat->rx_bytes);
    return 0;
}

int APS5_VABI sceNetCtlGetInfo(int code, NetCtlInfo* info) {
    if (!g_initialized.load()) return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
    if (!info) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    if (code < NET_CTL_INFO_DEVICE || code > NET_CTL_INFO_HTTP_PROXY_PORT) return SCE_NET_CTL_ERROR_INVALID_CODE;
    HostIpv4Interface host;
    if (!FindHostIpv4Interface(host)) return SCE_NET_CTL_ERROR_NOT_CONNECTED;
    switch (code) {
        case NET_CTL_INFO_DEVICE: WriteInfoWord(info, NET_CTL_DEVICE_WIRED); break;
        case NET_CTL_INFO_ETHER_ADDR: std::memcpy(info->opaque, NET_CTL_ETHER_ADDR, sizeof(NET_CTL_ETHER_ADDR)); break;
        case NET_CTL_INFO_MTU: WriteInfoWord(info, host.mtu); break;
        case NET_CTL_INFO_LINK: WriteInfoWord(info, NET_CTL_LINK_CONNECTED); break;
        case NET_CTL_INFO_IP_CONFIG: WriteInfoWord(info, NET_CTL_IP_DHCP); break;
        case NET_CTL_INFO_IP_ADDRESS: WriteInfoAddress(info, host.address); break;
        case NET_CTL_INFO_NETMASK: WriteInfoAddress(info, host.netmask); break;
        case NET_CTL_INFO_DEFAULT_ROUTE: WriteInfoAddress(info, host.gateway); break;
        case NET_CTL_INFO_PRIMARY_DNS: WriteInfoAddress(info, host.dns.size() > 0 ? host.dns[0] : in_addr{}); break;
        case NET_CTL_INFO_SECONDARY_DNS: WriteInfoAddress(info, host.dns.size() > 1 ? host.dns[1] : in_addr{}); break;
        case NET_CTL_INFO_HTTP_PROXY_CONFIG: WriteInfoWord(info, NET_CTL_HTTP_PROXY_OFF); break;
        case NET_CTL_INFO_BSSID:
        case NET_CTL_INFO_SSID:
        case NET_CTL_INFO_DHCP_HOSTNAME:
        case NET_CTL_INFO_PPPOE_AUTH_NAME:
        case NET_CTL_INFO_HTTP_PROXY_SERVER: std::memset(info->opaque, 0, sizeof(info->opaque)); break;
        case NET_CTL_INFO_WIFI_SECURITY:
        case NET_CTL_INFO_RSSI_DBM:
        case NET_CTL_INFO_RSSI_PERCENTAGE:
        case NET_CTL_INFO_CHANNEL:
        case NET_CTL_INFO_HTTP_PROXY_PORT: WriteInfoWord(info, 0); break;
        default: throw std::runtime_error("sceNetCtlGetInfo: info code " + std::to_string(code) + " is not implemented");
    }
    return 0;
}

int APS5_VABI sceNetCtlGetNatInfo(NetCtlNatInfo* info) {
    if (!g_initialized.load()) return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
    if (!info) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    if (info->size != sizeof(NetCtlNatInfo)) return SCE_NET_CTL_ERROR_INVALID_SIZE;
    HostIpv4Interface host;
    if (!FindHostIpv4Interface(host)) return SCE_NET_CTL_ERROR_NOT_CONNECTED;
    auto& config = GetStunConfig();
    std::uint64_t generation = 0;
    const auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard guard(config.lock);
        generation = config.configGeneration;
        if (config.hasCachedResult && config.cachedConfigGeneration == generation &&
            config.cachedLocalAddr == host.address.s_addr && now < config.cachedUntil) {
            *info = config.cachedResult;
            return 0;
        }
    }
    const NetCtlNatInfo discovered = DiscoverNatInfo(host.address);
    if (discovered.stun_status == NET_CTL_STUN_STATUS_OK) {
        std::lock_guard guard(config.lock);
        if (config.configGeneration == generation) {
            config.cachedConfigGeneration = generation;
            config.cachedLocalAddr = host.address.s_addr;
            config.cachedUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds(STUN_CACHE_TTL_MS);
            config.cachedResult = discovered;
            config.hasCachedResult = true;
        }
    }
    *info = discovered;
    return 0;
}

int APS5_VABI sceNetCtlGetResult(int event, int* result) {
    if (!g_initialized.load()) return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
    if (!result) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    if (event < NET_CTL_EVENT_DISCONNECTED || event > NET_CTL_EVENT_IPOBTAINED) {
        throw std::runtime_error("sceNetCtlGetResult: event type " + std::to_string(event) + " is not implemented");
    }
    *result = 0;
    return 0;
}

int APS5_VABI sceNetCtlGetState(int* state) {
    if (!g_initialized.load()) return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
    if (!state) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    *state = HostHasAddress(AF_UNSPEC) ? NET_CTL_STATE_IPOBTAINED : NET_CTL_STATE_DISCONNECTED;
    return 0;
}

int APS5_VABI sceNetCtlGetStateV6(int* state) {
    if (!g_initialized.load()) return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
    if (!state) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    *state = HostHasAddress(AF_INET6) ? NET_CTL_STATE_IPOBTAINED : NET_CTL_STATE_DISCONNECTED;
    return 0;
}

int APS5_VABI sceNetCtlInit(void) {
    g_initialized.store(true);
    return 0;
}

int APS5_VABI sceNetCtlRegisterCallback(NetCtlCallback func, void* arg, int* cid) {
    return RegisterCallback(g_callbacks, func, arg, cid);
}

int APS5_VABI sceNetCtlRegisterCallbackV6(NetCtlCallback func, void* arg, int* cid) {
    return RegisterCallback(g_callbacksV6, func, arg, cid);
}

int APS5_VABI sceNetCtlGetInfoV6(std::uint32_t code, NetCtlInfoV6* info) {
    if (!info) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    if (code < NET_CTL_INFO_V6_IP_ADDRESS || code > NET_CTL_INFO_V6_SECONDARY_DNS) return SCE_NET_CTL_ERROR_INVALID_CODE;
    if (!HostHasAddress(AF_INET6)) return SCE_NET_CTL_ERROR_NOT_CONNECTED;
    throw std::runtime_error("sceNetCtlGetInfoV6: info code " + std::to_string(code) + " is not implemented");
}

void APS5_VABI sceNetCtlTerm(void) {
}

int APS5_VABI sceNetCtlUnregisterCallback(int cid) {
    return UnregisterCallback(g_callbacks, cid);
}

int APS5_VABI sceNetCtlUnregisterCallbackV6(int cid) {
    return UnregisterCallback(g_callbacksV6, cid);
}

int APS5_VABI sceNetCtlRegisterCallbackForNpToolkit(NetCtlCallback callback, void* argument, int* callbackId) {
    return RegisterCallback(g_callbacksNpToolkit, callback, argument, callbackId);
}

int APS5_VABI sceNetCtlRegisterCallbackIpcInt(NetCtlCallback callback, void* argument, int* callbackId) {
    return sceNetCtlRegisterCallback(callback, argument, callbackId);
}

int APS5_VABI sceNetCtlRegisterCallbackForLibIpcInt(NetCtlCallback callback, void* argument, int* callbackId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlUnregisterCallbackForNpToolkit(int callbackId) {
    return UnregisterCallback(g_callbacksNpToolkit, callbackId);
}

int APS5_VABI sceNetCtlUnregisterCallbackIpcInt(int callbackId) {
    return sceNetCtlUnregisterCallback(callbackId);
}

int APS5_VABI sceNetCtlUnregisterCallbackForLibIpcInt(int callbackId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlCheckCallbackForNpToolkit(void) {
    return CheckCallbacks(g_callbacksNpToolkit);
}

int APS5_VABI sceNetCtlCheckCallbackForLibIpcInt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlClearEventIpcInt(int event) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlClearEventForNpToolkit(int event) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlClearEventForLibIpcInt(int event) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlGetResultIpcInt(int event, int* result) {
    return sceNetCtlGetResult(event, result);
}

int APS5_VABI sceNetCtlGetStateIpcInt(int* state) {
    return sceNetCtlGetState(state);
}

int APS5_VABI sceNetCtlGetState2IpcInt(int* state) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlGetInfoIpcInt(std::uint32_t code, NetCtlInfo* info) {
    return sceNetCtlGetInfo(static_cast<int>(code), info);
}

int APS5_VABI sceNetCtlGetEtherLinkMode(int* mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlGetWifiType(int* type) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlConnectIpcInt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlConnectWithRetryIpcInt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlConnectConfIpcInt(const NetCtlConnectionConfiguration* configuration) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlDisconnectIpcInt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlScanIpcInt(const char* const* ssids, std::uint32_t count, const NetCtlScanOptions* options) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlGetScanInfoBssidIpcInt(NetCtlScanBssidList* bssids, std::uint32_t capacity) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlGetScanInfoByBssidIpcInt(const std::uint8_t* bssid, NetCtlScanInfo* info, void* buffer, std::size_t* size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlGetScanInfoForSsidScanIpcInt(NetCtlScanInfo* info, void* buffer, std::size_t* size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlGetScanInfoBssidForSsidListScanIpcInt(NetCtlScanBssidList* bssids, std::uint32_t capacity) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlGetScanInfoForSsidListScanIpcInt(const std::uint8_t* bssid, NetCtlScanInfo* info, void* buffer, std::size_t* size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlSetErrorNotificationEnabledIpcInt(int enabled) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlRegisterCallbackV6IpcInt(NetCtlCallback callback, void* argument, int* callbackId) {
    return sceNetCtlRegisterCallbackV6(callback, argument, callbackId);
}

int APS5_VABI sceNetCtlUnregisterCallbackV6IpcInt(int callbackId) {
    return sceNetCtlUnregisterCallbackV6(callbackId);
}

int APS5_VABI sceNetCtlGetResultV6(int event_type, int* error_code) {
    if (!error_code) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    if (event_type < NET_CTL_EVENT_DISCONNECTED || event_type > NET_CTL_EVENT_IPOBTAINED) {
        throw std::runtime_error("sceNetCtlGetResultV6: event type " + std::to_string(event_type) + " is not implemented");
    }
    *error_code = 0;
    return 0;
}

int APS5_VABI sceNetCtlGetResultV6IpcInt(int event, int* result) {
    return sceNetCtlGetResultV6(event, result);
}

int APS5_VABI sceNetCtlGetStateV6IpcInt(int* state) {
    return sceNetCtlGetStateV6(state);
}

int APS5_VABI sceNetCtlGetInfoV6IpcInt(std::uint32_t code, NetCtlInfoV6* info) {
    return sceNetCtlGetInfoV6(code, info);
}

int APS5_VABI sceNetCtlGetNatInfoIpcInt(NetCtlNatInfoInternal* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlSetStunWithPaddingFlagIpcInt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlUnsetStunWithPaddingFlagIpcInt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlIsBandwidthManagementEnabledIpcInt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlDisableBandwidthManagementIpcInt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlEnableBandwidthManagementIpcInt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlGetBandwidthInfoIpcInt(NetCtlBandwidthInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlGetNetEvConfigInfoIpcInt(NetCtlNetEvConfiguration* configuration) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlGetConnectionElapsedTimeIpcInt(std::uint64_t* elapsedTime) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlApRpCheckCallback(void) {
    return 0;
}

int APS5_VABI sceNetCtlApRpClearEvent(void) {
    return 0;
}

int APS5_VABI sceNetCtlApRpRegisterCallback(void) {
    return 0;
}

int APS5_VABI sceNetCtlApRpUnregisterCallback(void) {
    return 0;
}

int APS5_VABI sceNetCtlApRpGetResult(void) {
    return 0;
}

int APS5_VABI sceNetCtlApRpGetState(void) {
    return 0;
}

int APS5_VABI sceNetCtlApRpGetInfo(void) {
    return 0;
}

int APS5_VABI sceNetCtlApRpStart(void) {
    return 0;
}

int APS5_VABI sceNetCtlApCpStart(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlApAppStartWithRetryPid(std::int32_t processId, const char* passcode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlApAppStartWithRetry(const char* passcode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlApRpStartWithRetry(void) {
    return 0;
}

int APS5_VABI sceNetCtlApRpStartConf(void) {
    return 0;
}

int APS5_VABI sceNetCtlApRpStop(void) {
    return 0;
}

int APS5_VABI sceNetCtlApCpStop(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlApAppInitWpaKey(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlApAppInitWpaKeyForQa(std::uint8_t mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlApRestart(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetBweCheckCallbackIpcInt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetBweClearEventIpcInt(int event) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetBweRegisterCallbackIpcInt(NetCtlCallback callback, void* argument, int* callbackId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetBweUnregisterCallbackIpcInt(int callbackId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetBweGetInfoIpcInt(NetBweInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetBweStartInternetConnectionTestIpcInt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetBweFinishInternetConnectionTestIpcInt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetBweStartInternetConnectionTestBandwidthTestIpcInt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetBweSetInternetConnectionTestResultIpcInt(std::int32_t firstResult, std::int32_t secondResult) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlGetWifiInfoIpcInt(NetCtlWifiInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
