#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstring>

extern "C" int* APS5_VABI __error_nid_postfix();

extern "C" {
int APS5_VABI __inet_pton_nid_postfix(int family, const char* text, void* destination) {
    if (family != 2 && family != 28) {
        *__error_nid_postfix() = 47;
        return -1;
    }
    if (!text || !destination) {
        *__error_nid_postfix() = 14;
        return -1;
    }
    unsigned char address[16]{};
    const int nativeFamily = family == 2 ? AF_INET : AF_INET6;
#ifdef _WIN32
    const int result = InetPtonA(nativeFamily, text, address);
#else
    const int result = ::inet_pton(nativeFamily, text, address);
#endif
    if (result == 1)
        std::memcpy(destination, address, family == 2 ? 4 : 16);
    else if (result == -1)
        *__error_nid_postfix() = 22;
    return result;
}

const char* APS5_VABI __inet_ntop_nid_postfix(int family, const void* source, char* destination,
                                              std::uint32_t capacity) {
    if (family != 2 && family != 28) {
        *__error_nid_postfix() = 47;
        return nullptr;
    }
    if (!source || !destination) {
        *__error_nid_postfix() = 14;
        return nullptr;
    }
    char text[INET6_ADDRSTRLEN]{};
    const int nativeFamily = family == 2 ? AF_INET : AF_INET6;
#ifdef _WIN32
    const auto* result = InetNtopA(nativeFamily, const_cast<void*>(source), text, sizeof(text));
#else
    const auto* result = ::inet_ntop(nativeFamily, source, text, sizeof(text));
#endif
    if (!result) {
        *__error_nid_postfix() = 22;
        return nullptr;
    }
    const auto size = std::strlen(text) + 1;
    if (size > capacity) {
        *__error_nid_postfix() = 28;
        return nullptr;
    }
    std::memcpy(destination, text, size);
    return destination;
}
}
