#ifdef _WIN32
#include "prx/libkernel/Socket/include/SocketConnect.hpp"
#include <ws2tcpip.h>
#include <mstcpip.h>
#include <iphlpapi.h>
#include <vector>

namespace {

bool IsLoopback(const sockaddr* address) {
    if (address->sa_family == AF_INET) return (ntohl(reinterpret_cast<const sockaddr_in*>(address)->sin_addr.s_addr) >> 24) == 127;
    if (address->sa_family == AF_INET6) return IN6_IS_ADDR_LOOPBACK(&reinterpret_cast<const sockaddr_in6*>(address)->sin6_addr);
    return false;
}

u_short Port(const sockaddr* address) {
    return address->sa_family == AF_INET ? reinterpret_cast<const sockaddr_in*>(address)->sin_port : reinterpret_cast<const sockaddr_in6*>(address)->sin6_port;
}

template<typename TTable>
bool ListenerOnPort(ULONG family, u_short port) {
    std::vector<unsigned char> table;
    ULONG size = 0;
    DWORD result = ERROR_INSUFFICIENT_BUFFER;
    while (result == ERROR_INSUFFICIENT_BUFFER) {
        table.resize(size);
        result = GetExtendedTcpTable(table.data(), &size, FALSE, family, TCP_TABLE_OWNER_PID_LISTENER, 0);
    }
    if (result != NO_ERROR) return true;
    const auto* rows = reinterpret_cast<const TTable*>(table.data());
    for (DWORD i = 0; i < rows->dwNumEntries; ++i) {
        if (static_cast<u_short>(rows->table[i].dwLocalPort) == port) return true;
    }
    return false;
}

}

extern "C" void RefuseClosedLoopbackAtOnce_nid_no_patch(SOCKET socket, const sockaddr* address) {
    if (!IsLoopback(address)) return;
    const u_short port = Port(address);
    if (ListenerOnPort<MIB_TCPTABLE_OWNER_PID>(AF_INET, port) || ListenerOnPort<MIB_TCP6TABLE_OWNER_PID>(AF_INET6, port)) return;
    TCP_INITIAL_RTO_PARAMETERS parameters{TCP_INITIAL_RTO_UNSPECIFIED_RTT, TCP_INITIAL_RTO_NO_SYN_RETRANSMISSIONS};
    DWORD returned = 0;
    WSAIoctl(socket, SIO_TCP_INITIAL_RTO, &parameters, sizeof(parameters), nullptr, 0, &returned, nullptr, nullptr);
}
#endif
