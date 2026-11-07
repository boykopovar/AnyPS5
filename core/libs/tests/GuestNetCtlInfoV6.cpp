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
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

static void Require(bool value) { if (!value) std::abort(); }

constexpr int kErrInvalidCode = static_cast<int>(0x80412106);
constexpr int kErrInvalidAddr = static_cast<int>(0x80412107);
constexpr int kErrNotConnected = static_cast<int>(0x80412108);
constexpr int kStateIpObtained = 3;
constexpr std::uint8_t kFill = 0xa5;
constexpr std::size_t kAddressLength = 46;

struct HostIpv6Address {
    in6_addr address{};
    unsigned prefix = 0;
};

static unsigned MaskPrefixBits(const in6_addr& mask) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&mask);
    unsigned bits = 0;
    for (unsigned index = 0; index < sizeof(mask); ++index) {
        for (unsigned bit = 0; bit < 8; ++bit) {
            if ((bytes[index] >> bit) & 1u) ++bits;
        }
    }
    return bits;
}

static unsigned AddressPrefix(const sockaddr* netmask) {
    if (!netmask || netmask->sa_family != AF_INET6) return 0;
    return MaskPrefixBits(reinterpret_cast<const sockaddr_in6*>(netmask)->sin6_addr);
}

static void CollectHostAddresses(std::vector<HostIpv6Address>& global, std::vector<HostIpv6Address>& linkLocal) {
#ifdef _WIN32
    ULONG size = 0;
    const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    if (GetAdaptersAddresses(AF_INET6, flags, nullptr, nullptr, &size) != ERROR_BUFFER_OVERFLOW) return;
    std::vector<std::uint8_t> storage(size);
    auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data());
    if (GetAdaptersAddresses(AF_INET6, flags, nullptr, adapters, &size) != NO_ERROR) return;
    for (auto* adapter = adapters; adapter; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        for (auto* unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next) {
            if (!unicast->Address.lpSockaddr || unicast->Address.lpSockaddr->sa_family != AF_INET6) continue;
            const auto* address = reinterpret_cast<const sockaddr_in6*>(unicast->Address.lpSockaddr);
            const HostIpv6Address entry{address->sin6_addr, unicast->OnLinkPrefixLength};
            if (IN6_IS_ADDR_LINKLOCAL(&address->sin6_addr)) linkLocal.push_back(entry);
            else if (!IN6_IS_ADDR_UNSPECIFIED(&address->sin6_addr) && !IN6_IS_ADDR_LOOPBACK(&address->sin6_addr)) global.push_back(entry);
        }
    }
#else
    ifaddrs* interfaces = nullptr;
    if (getifaddrs(&interfaces) != 0) return;
    for (auto* item = interfaces; item; item = item->ifa_next) {
        if (!item->ifa_addr || item->ifa_addr->sa_family != AF_INET6) continue;
        if (!(item->ifa_flags & IFF_UP) || (item->ifa_flags & IFF_LOOPBACK)) continue;
        const auto* address = reinterpret_cast<const sockaddr_in6*>(item->ifa_addr);
        const HostIpv6Address entry{address->sin6_addr, AddressPrefix(item->ifa_netmask)};
        if (IN6_IS_ADDR_LINKLOCAL(&address->sin6_addr)) linkLocal.push_back(entry);
        else if (!IN6_IS_ADDR_UNSPECIFIED(&address->sin6_addr) && !IN6_IS_ADDR_LOOPBACK(&address->sin6_addr)) global.push_back(entry);
    }
    freeifaddrs(interfaces);
#endif
}

static bool SameAddress(const in6_addr& left, const in6_addr& right) {
    return std::memcmp(&left, &right, sizeof(left)) == 0;
}

static void CheckCode(std::uint32_t code, const std::vector<HostIpv6Address>& kind) {
    NetCtlInfoV6 info;
    std::memset(&info, kFill, sizeof(info));
    const int result = sceNetCtlGetInfoV6(code, &info);
    if (kind.empty()) {
        Require(result == kErrNotConnected);
        return;
    }
    Require(result == 0);
    char text[kAddressLength];
    std::memcpy(text, info.opaque, sizeof(text));
    const void* terminator = std::memchr(text, '\0', sizeof(text));
    Require(terminator != nullptr);
    const auto length = static_cast<std::size_t>(static_cast<const char*>(terminator) - text);
    for (std::size_t index = length + 1; index < kAddressLength; ++index) Require(info.opaque[index] == 0);
    in6_addr parsed{};
    Require(inet_pton(AF_INET6, text, &parsed) == 1);
    if (code == 1) {
        Require(!IN6_IS_ADDR_LINKLOCAL(&parsed) && !IN6_IS_ADDR_LOOPBACK(&parsed) && !IN6_IS_ADDR_UNSPECIFIED(&parsed));
    } else {
        Require(IN6_IS_ADDR_LINKLOCAL(&parsed));
    }
    const auto prefix = static_cast<unsigned>(info.opaque[kAddressLength]);
    bool matched = false;
    for (const auto& entry : kind) {
        if (SameAddress(entry.address, parsed) && entry.prefix == prefix) matched = true;
    }
    Require(matched);
    for (std::size_t index = kAddressLength + 1; index < sizeof(info.opaque); ++index) Require(info.opaque[index] == kFill);
}

int main() {
#ifdef _WIN32
    WSADATA data{};
    Require(WSAStartup(MAKEWORD(2, 2), &data) == 0);
#endif
    NetCtlInfoV6 info{};
    Require(sceNetCtlGetInfoV6(0, &info) == kErrInvalidCode);
    Require(sceNetCtlGetInfoV6(6, &info) == kErrInvalidCode);
    Require(sceNetCtlGetInfoV6(1, nullptr) == kErrInvalidAddr);

    std::memset(&info, kFill, sizeof(info));
    const int first = sceNetCtlGetInfoV6(1, &info);
    Require(first == 0 || first == kErrNotConnected);
    sceNetCtlTerm();
    std::memset(&info, kFill, sizeof(info));
    const int termed = sceNetCtlGetInfoV6(1, &info);
    Require(termed == first);
    Require(sceNetCtlGetInfoV6(1, nullptr) == kErrInvalidAddr);
    Require(sceNetCtlInit() == 0);
    std::memset(&info, kFill, sizeof(info));
    Require(sceNetCtlGetInfoV6(1, &info) == first);

    std::vector<HostIpv6Address> global;
    std::vector<HostIpv6Address> linkLocal;
    CollectHostAddresses(global, linkLocal);

    int state = -1;
    Require(sceNetCtlGetStateV6(&state) == 0);
    if (state != kStateIpObtained) {
        Require(global.empty() && linkLocal.empty());
        for (const auto code : {1u, 2u, 3u, 4u, 5u}) Require(sceNetCtlGetInfoV6(code, &info) == kErrNotConnected);
        return 0;
    }
    Require(!global.empty() || !linkLocal.empty());
    CheckCode(1, global);
    CheckCode(2, linkLocal);
    return 0;
}
