#include "prx/libScePosixForWebKit/libScePosixForWebKit.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "resolver dns override: %s\n", message);
    std::abort();
}
}

int main() {
#ifdef _WIN32
    Require(_putenv_s("ANYPS5_DNS_OVERRIDE", "Guest-Posix.DNS-Override=192.0.2.44") == 0, "putenv failed");
#else
    Require(setenv("ANYPS5_DNS_OVERRIDE", "Guest-Posix.DNS-Override=192.0.2.44", 1) == 0, "setenv failed");
#endif
    GuestResolver::AddressInfo hints{};
    hints.family = 2;
    GuestResolver::AddressInfo* result = nullptr;
    Require(getaddrinfo_nid_postfix("guest-posix.dns-override", nullptr, &hints, &result) == 0 && result,
        "override lookup failed");
    bool found = false;
    for (const auto* entry = result; entry; entry = entry->next) {
        Require(entry->family == 2 && entry->addressLength == 16, "override returned a non-IPv4 address");
        const auto* bytes = static_cast<const unsigned char*>(entry->address);
        constexpr std::array<unsigned char, 4> address{192, 0, 2, 44};
        Require(std::memcmp(bytes + 4, address.data(), address.size()) == 0, "override address changed");
        found = true;
    }
    Require(found, "override returned no address");
    freeaddrinfo_nid_postfix(result);

    GuestResolver::AddressInfo* none = nullptr;
    hints.family = 28;
    Require(getaddrinfo_nid_postfix("guest-posix.dns-override", nullptr, &hints, &none) != 0 && !none,
        "IPv6 query for an IPv4 override returned an address");
    return 0;
}
