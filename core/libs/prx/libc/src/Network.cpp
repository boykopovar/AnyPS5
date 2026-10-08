#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

#ifdef _WIN32
static bool EnsureWinsock() {
    static const int result = [] {
        WSADATA data{};
        return WSAStartup(MAKEWORD(2, 2), &data);
    }();
    return result == 0;
}
#endif

extern "C" {

std::uint32_t APS5_VABI __inet_addr_nid_postfix(const char* text) {
#ifdef _WIN32
    if (!EnsureWinsock()) return INADDR_NONE;
#endif
    return ::inet_addr(text);
}

char* APS5_VABI __inet_ntoa_nid_postfix(std::uint32_t address) {
#ifdef _WIN32
    if (!EnsureWinsock()) return nullptr;
#endif
    in_addr native{};
    native.s_addr = address;
    thread_local char buffer[INET_ADDRSTRLEN];
#ifdef _WIN32
    if (InetNtopA(AF_INET, &native, buffer, sizeof(buffer)) == nullptr) return nullptr;
#else
    if (::inet_ntop(AF_INET, &native, buffer, sizeof(buffer)) == nullptr) return nullptr;
#endif
    return buffer;
}

}
