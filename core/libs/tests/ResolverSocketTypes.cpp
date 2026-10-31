#include "prx/libScePosixForWebKit/libScePosixForWebKit.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "resolver socket types: %s\n", message);
    std::abort();
}

void Resolve(const char* node, const char* service, int family, int type, int protocol, bool noHints) {
    GuestResolver::AddressInfo hints{};
    hints.flags = 12;
    hints.family = family;
    hints.socketType = type;
    hints.protocol = protocol;
    GuestResolver::AddressInfo* result = nullptr;
    Require(getaddrinfo_nid_postfix(node, service, noHints ? nullptr : &hints, &result) == 0 && result,
        "numeric lookup failed");
    bool stream = false;
    bool datagram = false;
    for (const auto* entry = result; entry; entry = entry->next) {
        Require(entry->socketType == 1 || entry->socketType == 2, "lookup returned a raw or invalid socket type");
        Require(entry->protocol == (entry->socketType == 1 ? 6 : 17), "transport protocol differs from socket type");
        Require(!type || entry->socketType == type, "explicit socket type changed");
        Require(!protocol || entry->protocol == protocol, "explicit protocol changed");
        stream |= entry->socketType == 1;
        datagram |= entry->socketType == 2;
        const int expectedFamily = node[0] == ':' ? 28 : 2;
        Require(entry->family == expectedFamily, "address family changed");
        Require(entry->addressLength == (expectedFamily == 2 ? 16u : 28u), "address length changed");
        const auto* bytes = static_cast<const unsigned char*>(entry->address);
        Require(bytes && bytes[0] == entry->addressLength && bytes[1] == expectedFamily, "guest sockaddr prefix differs");
        const unsigned port = service ? static_cast<unsigned>(std::strtoul(service, nullptr, 10)) : 0;
        Require(bytes[2] == port / 256 && bytes[3] == port % 256, "transport port changed");
        if (expectedFamily == 2) {
            constexpr std::array<unsigned char, 4> address{127, 0, 0, 1};
            Require(std::memcmp(bytes + 4, address.data(), address.size()) == 0, "IPv4 bytes changed");
        } else {
            constexpr std::array<unsigned char, 16> address{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
            Require(std::memcmp(bytes + 8, address.data(), address.size()) == 0, "IPv6 bytes changed");
        }
    }
    if (!type && !protocol) Require(stream && datagram, "unspecified query lost a transport");
    Require(hints.flags == 12 && hints.family == family && hints.socketType == type && hints.protocol == protocol,
        "lookup modified hints");
    freeaddrinfo_nid_postfix(result);
}

}

int main() {
#ifdef __linux__
    for (const char* node : {"127.0.0.1", "::1"}) {
        GuestResolver::AddressInfo hints{};
        hints.flags = 4;
        hints.protocol = node[0] == ':' ? 58 : 1;
        auto* result = reinterpret_cast<GuestResolver::AddressInfo*>(1);
        Require(getaddrinfo_nid_postfix(node, nullptr, &hints, &result) == 4 && !result,
            "raw-only host results did not fail with EAI_FAIL and clear output");
    }
#endif
    for (const char* node : {"127.0.0.1", "::1"}) {
        Resolve(node, nullptr, 0, 0, 0, true);
        for (const int family : {0, node[0] == ':' ? 28 : 2}) {
            for (const char* service : {static_cast<const char*>(nullptr), "0", "27910", "65535"}) {
                for (const int protocol : {0, 6, 17}) Resolve(node, service, family, 0, protocol, false);
                Resolve(node, service, family, 1, 6, false);
                Resolve(node, service, family, 2, 17, false);
            }
        }
    }
}
