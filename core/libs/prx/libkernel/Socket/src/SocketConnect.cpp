#ifdef _WIN32
#include "prx/libkernel/Socket/include/SocketConnect.hpp"
#include <ws2tcpip.h>
#include <mstcpip.h>
#include <iphlpapi.h>
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <vector>

namespace {

bool IsIpv4Loopback(ULONG address) {
    return (ntohl(address) >> 24) == 127;
}

bool ExtractTarget(const sockaddr* address, bool& isIpv4, ULONG& ipv4Address, in6_addr& ipv6Address) {
    if (!address) return false;
    if (address->sa_family == AF_INET) {
        ipv4Address = reinterpret_cast<const sockaddr_in*>(address)->sin_addr.s_addr;
        isIpv4 = true;
        return IsIpv4Loopback(ipv4Address);
    }
    if (address->sa_family == AF_INET6) {
        ipv6Address = reinterpret_cast<const sockaddr_in6*>(address)->sin6_addr;
        if (IN6_IS_ADDR_V4MAPPED(&ipv6Address)) {
            std::memcpy(&ipv4Address, ipv6Address.s6_addr + 12, sizeof(ipv4Address));
            isIpv4 = true;
            return IsIpv4Loopback(ipv4Address);
        }
        isIpv4 = false;
        return IN6_IS_ADDR_LOOPBACK(&ipv6Address) != 0;
    }
    return false;
}

u_short Port(const sockaddr* address) {
    return address->sa_family == AF_INET ? reinterpret_cast<const sockaddr_in*>(address)->sin_port : reinterpret_cast<const sockaddr_in6*>(address)->sin6_port;
}

enum class ListenerQuery {
    Matched,
    Missing,
    Unavailable,
};

template<typename TTable, typename TMatch>
ListenerQuery QueryListener(ULONG family, TMatch&& match) {
    std::vector<unsigned char> table;
    ULONG size = 0;
    DWORD result = ERROR_INSUFFICIENT_BUFFER;
    while (result == ERROR_INSUFFICIENT_BUFFER) {
        table.resize(size);
        result = GetExtendedTcpTable(table.data(), &size, FALSE, family, TCP_TABLE_OWNER_PID_LISTENER, 0);
    }
    const std::size_t headerSize = offsetof(TTable, table);
    if (result != NO_ERROR || table.size() < headerSize) return ListenerQuery::Unavailable;
    const auto* rows = reinterpret_cast<const TTable*>(table.data());
    const std::size_t maxEntries = (table.size() - headerSize) / sizeof(rows->table[0]);
    const DWORD count = static_cast<DWORD>((std::min)(static_cast<std::size_t>(rows->dwNumEntries), maxEntries));
    for (DWORD i = 0; i < count; ++i) {
        if (match(rows->table[i])) return ListenerQuery::Matched;
    }
    return ListenerQuery::Missing;
}

bool Ipv6ListenerMatchesIpv4(const MIB_TCP6ROW_OWNER_PID& row, u_short port, ULONG ipv4Address) {
    if (static_cast<u_short>(row.dwLocalPort) != port) return false;
    in6_addr local{};
    std::memcpy(&local, row.ucLocalAddr, sizeof(local));
    if (IN6_IS_ADDR_UNSPECIFIED(&local)) return true;
    if (!IN6_IS_ADDR_V4MAPPED(&local)) return false;
    ULONG mapped = 0;
    std::memcpy(&mapped, local.s6_addr + 12, sizeof(mapped));
    return mapped == htonl(INADDR_ANY) || mapped == ipv4Address;
}

bool Ipv6ListenerMatchesLoopback6(const MIB_TCP6ROW_OWNER_PID& row, u_short port) {
    if (static_cast<u_short>(row.dwLocalPort) != port) return false;
    in6_addr local{};
    std::memcpy(&local, row.ucLocalAddr, sizeof(local));
    return IN6_IS_ADDR_UNSPECIFIED(&local) || IN6_IS_ADDR_LOOPBACK(&local);
}

bool ListenerDefinitelyAbsent(const sockaddr* address, bool isIpv4, ULONG ipv4Address) {
    const u_short port = Port(address);
    if (!isIpv4) {
        return QueryListener<MIB_TCP6TABLE_OWNER_PID>(AF_INET6, [&](const MIB_TCP6ROW_OWNER_PID& row) {
            return Ipv6ListenerMatchesLoopback6(row, port);
        }) == ListenerQuery::Missing;
    }
    return QueryListener<MIB_TCPTABLE_OWNER_PID>(AF_INET, [&](const MIB_TCPROW_OWNER_PID& row) {
        return static_cast<u_short>(row.dwLocalPort) == port
            && (row.dwLocalAddr == htonl(INADDR_ANY) || row.dwLocalAddr == ipv4Address);
    }) == ListenerQuery::Missing
        && QueryListener<MIB_TCP6TABLE_OWNER_PID>(AF_INET6, [&](const MIB_TCP6ROW_OWNER_PID& row) {
            return Ipv6ListenerMatchesIpv4(row, port, ipv4Address);
        }) == ListenerQuery::Missing;
}

}

extern "C" void RefuseClosedLoopbackAtOnce_nid_no_patch(SOCKET socket, const sockaddr* address) {
    bool isIpv4 = false;
    ULONG ipv4Address = 0;
    in6_addr ipv6Address{};
    if (!ExtractTarget(address, isIpv4, ipv4Address, ipv6Address)) return;
    if (!ListenerDefinitelyAbsent(address, isIpv4, ipv4Address)) return;
    TCP_INITIAL_RTO_PARAMETERS parameters{TCP_INITIAL_RTO_UNSPECIFIED_RTT, TCP_INITIAL_RTO_NO_SYN_RETRANSMISSIONS};
    DWORD returned = 0;
    WSAIoctl(socket, SIO_TCP_INITIAL_RTO, &parameters, sizeof(parameters), nullptr, 0, &returned, nullptr, nullptr);
}
#endif
