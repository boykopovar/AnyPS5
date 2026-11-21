extern "C" {
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceNetCtl/include/InventoryExports.hpp"
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
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#ifdef __linux__
#include <netpacket/packet.h>
#endif
#ifdef __APPLE__
#include <net/if_dl.h>
#include <sys/sysctl.h>
#endif
#endif
#include <atomic>
#include <cerrno>
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
static constexpr int SCE_NET_CTL_ERROR_RESULT_EVENT_REFUSED = static_cast<int>(0x8041210F);
static constexpr int NET_CTL_STATE_DISCONNECTED = 0;
static constexpr int NET_CTL_STATE_IPOBTAINED = 3;
static constexpr int NET_CTL_EVENT_DISCONNECTED = 1;
static constexpr int NET_CTL_EVENT_DISCONNECT_REQ_FINISHED = 2;
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
static constexpr std::uint16_t STUN_ATTR_OTHER_ADDRESS = 0x802c;
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
static constexpr int NET_CTL_INFO_V6_LINK_LOCAL = 2;
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
static CallbackSlot g_callbacksForLib[MAX_CALLBACKS] = {};

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
#ifdef _WIN32
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(descriptor, &readSet);
        timeval tv{};
        tv.tv_sec = static_cast<long>(remainingUs / 1000000);
        tv.tv_usec = static_cast<long>(remainingUs % 1000000);
        if (tv.tv_sec == 0 && tv.tv_usec == 0) tv.tv_usec = 1000;
        const int ready = select(0, &readSet, nullptr, nullptr, &tv);
#else
        pollfd entry{descriptor, POLLIN, 0};
        const int ready = poll(&entry, 1, static_cast<int>((remainingUs + 999) / 1000));
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

static bool WriteInfoWord(NetCtlInfo* info, std::uint32_t value) {
    return GuestCopyOut_nid_no_patch(info->opaque, &value, sizeof(value));
}

static bool WriteInfoAddress(NetCtlInfo* info, in_addr address) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&address.s_addr);
    char text[NET_CTL_IPV4_ADDR_STR_LEN] = {};
    std::snprintf(text, sizeof(text), "%u.%u.%u.%u", static_cast<unsigned>(bytes[0]), static_cast<unsigned>(bytes[1]), static_cast<unsigned>(bytes[2]), static_cast<unsigned>(bytes[3]));
    return GuestCopyOut_nid_no_patch(info->opaque, text, sizeof(text));
}

static constexpr std::size_t NET_CTL_IPV6_ADDR_STR_LEN = 46;

static unsigned Ipv6PrefixBits(const in6_addr& mask) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&mask);
    unsigned bits = 0;
    for (unsigned index = 0; index < sizeof(mask); ++index) {
        for (unsigned bit = 0; bit < 8; ++bit) {
            if ((bytes[index] >> bit) & 1u) ++bits;
        }
    }
    return bits;
}

static bool Ipv6IsGlobal(const in6_addr& address) {
    return !IN6_IS_ADDR_UNSPECIFIED(&address) && !IN6_IS_ADDR_LOOPBACK(&address) && !IN6_IS_ADDR_LINKLOCAL(&address);
}

struct HostIpv6Address {
    in6_addr address{};
    unsigned prefix = 0;
    bool valid = false;
};

struct HostIpv6Selection {
    HostIpv6Address global;
    HostIpv6Address linkLocalOfGlobal;
    HostIpv6Address linkLocal;
};

#ifdef _WIN32
static HostIpv6Selection QueryHostIpv6Selection() {
    HostIpv6Selection found;
    ULONG size = 0;
    const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    if (GetAdaptersAddresses(AF_INET6, flags, nullptr, nullptr, &size) != ERROR_BUFFER_OVERFLOW) return found;
    std::vector<std::uint8_t> storage(size);
    auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data());
    if (GetAdaptersAddresses(AF_INET6, flags, nullptr, adapters, &size) != NO_ERROR) return found;
    for (auto* adapter = adapters; adapter; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        HostIpv6Address adapterGlobal;
        HostIpv6Address adapterLinkLocal;
        for (auto* unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next) {
            if (!unicast->Address.lpSockaddr || unicast->Address.lpSockaddr->sa_family != AF_INET6) continue;
            const auto* address = reinterpret_cast<const sockaddr_in6*>(unicast->Address.lpSockaddr);
            HostIpv6Address candidate;
            candidate.address = address->sin6_addr;
            candidate.prefix = unicast->OnLinkPrefixLength;
            candidate.valid = true;
            if (Ipv6IsGlobal(address->sin6_addr)) {
                if (!adapterGlobal.valid) adapterGlobal = candidate;
            } else if (IN6_IS_ADDR_LINKLOCAL(&address->sin6_addr)) {
                if (!adapterLinkLocal.valid) adapterLinkLocal = candidate;
            }
        }
        if (adapterGlobal.valid && !found.global.valid) {
            found.global = adapterGlobal;
            found.linkLocalOfGlobal = adapterLinkLocal;
        }
        if (!found.linkLocal.valid && adapterLinkLocal.valid) found.linkLocal = adapterLinkLocal;
    }
    return found;
}
#else
static HostIpv6Address ReadHostIpv6Address(const ifaddrs* item) {
    HostIpv6Address address;
    address.address = reinterpret_cast<const sockaddr_in6*>(item->ifa_addr)->sin6_addr;
    address.prefix = item->ifa_netmask && item->ifa_netmask->sa_family == AF_INET6
                         ? Ipv6PrefixBits(reinterpret_cast<const sockaddr_in6*>(item->ifa_netmask)->sin6_addr)
                         : 0;
    address.valid = true;
    return address;
}

static HostIpv6Selection QueryHostIpv6Selection() {
    HostIpv6Selection found;
    ifaddrs* list = nullptr;
    if (getifaddrs(&list) != 0) return found;
    const std::unique_ptr<ifaddrs, void (*)(ifaddrs*)> interfaces(list, freeifaddrs);
    std::string globalInterface;
    for (auto* item = list; item; item = item->ifa_next) {
        if (!item->ifa_addr || item->ifa_addr->sa_family != AF_INET6) continue;
        if (!(item->ifa_flags & IFF_UP) || (item->ifa_flags & IFF_LOOPBACK)) continue;
        if (found.global.valid || !Ipv6IsGlobal(reinterpret_cast<const sockaddr_in6*>(item->ifa_addr)->sin6_addr)) continue;
        found.global = ReadHostIpv6Address(item);
        globalInterface = item->ifa_name;
    }
    for (auto* item = list; item; item = item->ifa_next) {
        if (!item->ifa_addr || item->ifa_addr->sa_family != AF_INET6) continue;
        if (!(item->ifa_flags & IFF_UP) || (item->ifa_flags & IFF_LOOPBACK)) continue;
        if (!IN6_IS_ADDR_LINKLOCAL(&reinterpret_cast<const sockaddr_in6*>(item->ifa_addr)->sin6_addr)) continue;
        const HostIpv6Address candidate = ReadHostIpv6Address(item);
        if (!found.linkLocal.valid) found.linkLocal = candidate;
        if (!found.linkLocalOfGlobal.valid && !globalInterface.empty() && globalInterface == item->ifa_name) {
            found.linkLocalOfGlobal = candidate;
        }
    }
    return found;
}
#endif

static bool WriteInfoV6Address(NetCtlInfoV6* info, const HostIpv6Address& address) {
    char buffer[NET_CTL_IPV6_ADDR_STR_LEN + 1] = {};
    inet_ntop(AF_INET6, &address.address, buffer, NET_CTL_IPV6_ADDR_STR_LEN);
    buffer[NET_CTL_IPV6_ADDR_STR_LEN] = static_cast<char>(static_cast<std::uint8_t>(address.prefix));
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(info->opaque, buffer, sizeof(buffer));
    errno = savedErrno;
    return copied;
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
#elif defined(__APPLE__)
    ifaddrs* interfaces = nullptr;
    if (getifaddrs(&interfaces) != 0) throw std::runtime_error("sceNetCtlGetIfStat: the host interface list is unavailable");
    for (auto* item = interfaces; item; item = item->ifa_next) {
        if (!item->ifa_addr || item->ifa_addr->sa_family != AF_LINK || !item->ifa_data) continue;
        if (!(item->ifa_flags & IFF_UP) || (item->ifa_flags & IFF_LOOPBACK)) continue;
        const auto* statistics = static_cast<const struct if_data*>(item->ifa_data);
        sent += statistics->ifi_obytes;
        received += statistics->ifi_ibytes;
    }
    freeifaddrs(interfaces);
#else
    throw std::runtime_error("sceNetCtlGetIfStat: host interface byte counters are not implemented on this platform");
#endif
}

}
