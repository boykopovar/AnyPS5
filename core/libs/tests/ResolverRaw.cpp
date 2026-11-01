#include "prx/libScePosixForWebKit/libScePosixForWebKit.hpp"
#include "prx/libScePosixForWebKit/GuestResolver.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>


namespace {

void Require(bool result, const char* message) {
    if (result) return;
    std::fprintf(stderr, "raw resolver: %s\n", message);
    std::abort();
}

void Address(const char* node, int family, int protocol, int flags) {
    GuestResolver::AddressInfo hints{};
    hints.flags = flags;
    hints.family = family;
    hints.socketType = 3;
    hints.protocol = protocol;
    GuestResolver::AddressInfo* result = nullptr;
    Require(getaddrinfo_nid_postfix(node, nullptr, &hints, &result) == 0 && result, "raw address resolution failed");
    unsigned count = 0;
    for (const auto* entry = result; entry; entry = entry->next) {
        ++count;
        Require(entry->socketType == 3 && entry->protocol == protocol, "raw type or protocol changed");
        const auto* bytes = static_cast<const unsigned char*>(entry->address);
        Require(entry->family == (node[0] == ':' ? 28 : 2), "address family changed");
        Require(bytes[0] == entry->addressLength && bytes[1] == entry->family, "guest sockaddr prefix differs");
        Require(bytes[2] == 0 && bytes[3] == 0, "raw address has a transport port");
        if (entry->family == 2) {
            constexpr std::array<unsigned char, 4> expected{127, 0, 0, 1};
            Require(entry->addressLength == 16 && std::memcmp(bytes + 4, expected.data(), expected.size()) == 0, "IPv4 bytes differ");
        } else {
            constexpr std::array<unsigned char, 16> expected{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
            Require(entry->addressLength == 28 && std::memcmp(bytes + 8, expected.data(), expected.size()) == 0, "IPv6 bytes differ");
        }
        if (flags & 2) Require(entry->canonicalName && std::strcmp(entry->canonicalName, node) == 0, "numeric canonical name differs");
    }
    Require(count == 1, "raw numeric query returned duplicate socket variants");
    Require(hints.socketType == 3 && hints.protocol == protocol && hints.flags == flags && hints.family == family, "query mutated hints");
    freeaddrinfo_nid_postfix(result);
}

}

int main() {
    for (int protocol : {0, 1, 6, 17, 58, 132, 255, 256, -1, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        for (int flags : {4, 6, 12}) {
            Address("127.0.0.1", 0, protocol, flags);
            Address("127.0.0.1", 2, protocol, flags);
            Address("::1", 0, protocol, flags);
            Address("::1", 28, protocol, flags);
        }
    }
    GuestResolver::AddressInfo hints{};
    hints.flags = 4;
    hints.socketType = 3;
    for (const char* service : {"", "0", "123", "65535", "http", "not-a-service"}) {
        auto* result = reinterpret_cast<GuestResolver::AddressInfo*>(1);
        Require(getaddrinfo_nid_postfix("127.0.0.1", service, &hints, &result) == 9 && !result, "raw service did not fail with EAI_SERVICE and clear output");
    }
    GuestResolver::AddressInfo* result = nullptr;
    Require(getaddrinfo_nid_postfix(nullptr, nullptr, &hints, &result) == 8 && !result, "missing host and service was accepted");
    hints.family = 2;
    Require(getaddrinfo_nid_postfix("::1", nullptr, &hints, &result) != 0 && !result, "family mismatch was accepted");
    hints.family = 0;
    hints.flags = 0x200;
    Require(getaddrinfo_nid_postfix("127.0.0.1", nullptr, &hints, &result) == 3 && !result, "invalid raw flags were accepted");
    Require(getaddrinfo_nid_postfix(nullptr, nullptr, &hints, &result) == 8 && !result, "invalid flags overrode missing names");
    hints.flags = 4;
    hints.addressLength = 1;
    Require(getaddrinfo_nid_postfix("127.0.0.1", nullptr, &hints, &result) == 12 && !result, "invalid raw hint state was accepted");
    Require(getaddrinfo_nid_postfix(nullptr, nullptr, &hints, &result) == 8 && !result, "invalid hint state overrode missing names");
    return 0;
}
