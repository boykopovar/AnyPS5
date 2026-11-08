#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libSceNet/include/Net.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#else
#include <netdb.h>
#endif
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libScePosixForWebKit/GuestResolver.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <source_location>
#include <barrier>
#include <stdexcept>
#include <string>
#include <thread>

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Resolver check failed at line %u\n", location.line());
        std::abort();
    }
}
int main() {
    auto* legacy = gethostbyname_nid_postfix("127.0.0.1");
    Require(legacy && legacy->name && std::strcmp(legacy->name, "127.0.0.1") == 0);
    Require(legacy->family == 2 && legacy->length == 4);
    const std::array<unsigned char, 4> loopback{127, 0, 0, 1};
    Require(legacy->addresses && legacy->addresses[0]);
    Require(std::memcmp(legacy->addresses[0], loopback.data(), loopback.size()) == 0);
    Require(legacy->addresses[1] == nullptr && legacy->aliases && legacy->aliases[0] == nullptr);
    struct NumericCase {
        const char* name;
        std::array<unsigned char, 4> address;
    };
    const NumericCase numericCases[]{
        {"127.1", {127, 0, 0, 1}},
        {"127.0.1", {127, 0, 0, 1}},
        {"0x7f.1", {127, 0, 0, 1}},
        {"0X7F.0x1", {127, 0, 0, 1}},
        {"0177.01", {127, 0, 0, 1}},
        {"0x7f000001", {127, 0, 0, 1}},
        {"2130706433", {127, 0, 0, 1}},
        {"127.1 trailing", {127, 0, 0, 1}},
        {"127.0.0.1\tignored", {127, 0, 0, 1}},
        {"4294967297", {0, 0, 0, 1}},
        {"18446744073709551617", {0, 0, 0, 1}},
        {"0x.1", {0, 0, 0, 1}},
        {"255.255.255.255", {255, 255, 255, 255}},
        {"0", {0, 0, 0, 0}},
        {"1.16777215", {1, 255, 255, 255}},
        {"1.2.65535", {1, 2, 255, 255}}
    };
    for (const auto& test : numericCases) {
        *sceNetErrnoLoc() = 77;
        legacy = gethostbyname_nid_postfix(test.name);
        Require(legacy && std::strcmp(legacy->name, test.name) == 0);
        Require(legacy->family == 2 && legacy->length == 4);
        Require(legacy->aliases && !legacy->aliases[0]);
        Require(legacy->addresses && legacy->addresses[0] && !legacy->addresses[1]);
        Require(std::memcmp(legacy->addresses[0], test.address.data(), test.address.size()) == 0);
        Require(*sceNetErrnoLoc() == 0);
    }
    legacy = gethostbyname_nid_postfix("localhost");
    const auto* native = ::gethostbyname("localhost");
    Require(legacy && native && std::strcmp(legacy->name, native->h_name) == 0);
    Require(legacy->family == 2 && legacy->length == native->h_length);
    std::size_t index = 0;
    while (native->h_aliases[index]) {
        Require(legacy->aliases[index] && std::strcmp(legacy->aliases[index], native->h_aliases[index]) == 0);
        ++index;
    }
    Require(legacy->aliases[index] == nullptr);
    index = 0;
    while (native->h_addr_list[index]) {
        Require(legacy->addresses[index] && std::memcmp(legacy->addresses[index], native->h_addr_list[index], legacy->length) == 0);
        ++index;
    }
    Require(legacy->addresses[index] == nullptr);
    std::barrier ready(2);
    std::thread worker([&] {
        auto* entry = gethostbyname_nid_postfix("127.0.0.2");
        ready.arrive_and_wait();
        const std::array<unsigned char, 4> expected{127, 0, 0, 2};
        Require(entry && std::strcmp(entry->name, "127.0.0.2") == 0);
        Require(std::memcmp(entry->addresses[0], expected.data(), expected.size()) == 0);
    });
    legacy = gethostbyname_nid_postfix("127.0.0.1");
    ready.arrive_and_wait();
    Require(std::strcmp(legacy->name, "127.0.0.1") == 0);
    Require(std::memcmp(legacy->addresses[0], loopback.data(), loopback.size()) == 0);
    worker.join();
    *sceNetErrnoLoc() = 77;
    Require(gethostbyname_nid_postfix(std::string(1024, 'a').c_str()) == nullptr);
    Require(*sceNetErrnoLoc() >= 1 && *sceNetErrnoLoc() <= 4);
    {
        alignas(8) char reentrant[64];
        GuestResolver::HostEntry entry{};
        GuestResolver::HostEntry* result = nullptr;
        int hostError = 77;
        const auto* preserved = gethostbyname_nid_postfix("127.0.0.2");
        Require(preserved && std::strcmp(preserved->name, "127.0.0.2") == 0);
        Require(scePosixForWebKit_gethostbyname_r("127.0.0.1", &entry, reentrant, 38, &result, &hostError) == 0);
        Require(result == &entry && hostError == 77 && *sceNetErrnoLoc() == 0);
        Require(entry.family == 2 && entry.length == 4);
        auto** pointers = reinterpret_cast<char**>(reentrant);
        Require(entry.addresses == pointers && pointers[0] == reentrant + 24 && pointers[1] == nullptr);
        Require(entry.aliases == pointers + 2 && pointers[2] == nullptr);
        Require(std::memcmp(entry.addresses[0], loopback.data(), loopback.size()) == 0);
        Require(entry.name == reentrant + 28 && std::strcmp(entry.name, "127.0.0.1") == 0);
        Require(std::strcmp(preserved->name, "127.0.0.2") == 0);

        Require(scePosixForWebKit_gethostbyname_r("127.0.0.1", &entry, reentrant + 1, 45, &result, &hostError) == 0);
        Require(result == &entry && reinterpret_cast<char*>(entry.addresses) == reentrant + 8);

        errno = 0;
        Require(scePosixForWebKit_gethostbyname_r("127.0.0.1", &entry, reentrant, 37, &result, &hostError) == 34);
        Require(result == nullptr && hostError == -1 && errno == 34 && *sceNetErrnoLoc() == -1);
        Require(scePosixForWebKit_gethostbyname_r("127.0.0.1", nullptr, nullptr, 0, &result, &hostError) == 34);

        const std::string longNumeric = "127.0.0.1 " + std::string(2000, 'x');
        std::array<char, 2048> large{};
        Require(scePosixForWebKit_gethostbyname_r(longNumeric.c_str(), &entry, large.data(), large.size(), &result, &hostError) == 0);
        Require(std::strlen(entry.name) == 1025);

        hostError = 77;
        result = &entry;
        Require(scePosixForWebKit_gethostbyname_r(std::string(1024, 'a').c_str(), &entry, large.data(), large.size(), &result, &hostError) == -1);
        Require(result == nullptr && hostError >= 1 && hostError <= 4 && hostError == *sceNetErrnoLoc());

        Require(scePosixForWebKit_gethostbyname_r("localhost", &entry, large.data(), large.size(), &result, &hostError) == 0);
        Require(result == &entry && entry.family == 2 && entry.length == 4 && entry.addresses[0] && entry.aliases);

        bool threw = false;
        try {
            scePosixForWebKit_gethostbyname_r(nullptr, &entry, large.data(), large.size(), &result, &hostError);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Require(threw);
    }
    std::array<unsigned char, 16> v4{16, 2, 0x6d, 0x06, 127, 0, 0, 1};
    char host[128]{}, service[32]{};
    Require(getnameinfo_nid_postfix(v4.data(), v4.size(), host, sizeof(host), service, sizeof(service), 10) == 0);
    Require(std::strcmp(host, "127.0.0.1") == 0 && std::strcmp(service, "27910") == 0);
    std::array<unsigned char, 28> v6{28, 28};
    v6[23] = 1;
    Require(getnameinfo_nid_postfix(v6.data(), v6.size(), host, sizeof(host), nullptr, 0, 10) == 0);
    Require(std::strcmp(host, "::1") == 0);
    Require(getnameinfo_nid_postfix(v4.data(), 2, host, sizeof(host), nullptr, 0, 10) == 4);
    Require(getnameinfo_nid_postfix(nullptr, 16, host, sizeof(host), nullptr, 0, 10) == 4);
    std::array<unsigned char, 128> storage{};
    std::memcpy(storage.data(), v4.data(), v4.size());
    Require(getnameinfo_nid_postfix(storage.data(), storage.size(), host, sizeof(host), nullptr, 0, 10) == 4);
    std::array<unsigned char, 16> v6Family{16, 28, 0x6d, 0x06, 127, 0, 0, 1};
    Require(getnameinfo_nid_postfix(v6Family.data(), v6Family.size(), host, sizeof(host), nullptr, 0, 10) == 4);
    std::array<unsigned char, 16> unspecified{16, 0, 0x6d, 0x06, 127, 0, 0, 1};
    Require(getnameinfo_nid_postfix(unspecified.data(), unspecified.size(), host, sizeof(host), nullptr, 0, 10) == 5);
    std::array<unsigned char, 106> local{8, 1, '/', 'v', 'a', 'r', '/', 's'};
    std::memset(service, 'S', sizeof(service));
    Require(getnameinfo_nid_postfix(local.data(), local.size(), host, sizeof(host), service, sizeof(service), 0) == 0);
    Require(std::strcmp(host, "/var/s") == 0 && service[0] == '\0' && service[1] == 'S');
    Require(getnameinfo_nid_postfix(local.data(), 3, host, 7, nullptr, 0, 4) == 0);
    Require(std::strcmp(host, "/var/s") == 0);
    std::memset(host, 'H', sizeof(host));
    Require(getnameinfo_nid_postfix(local.data(), local.size(), host, 6, nullptr, 0, 0) == 6);
    Require(host[0] == '\0' && host[1] == 'H');
    Require(getnameinfo_nid_postfix(local.data(), 2, host, sizeof(host), nullptr, 0, 0) == 4);
    Require(getnameinfo_nid_postfix(local.data(), 107, host, sizeof(host), nullptr, 0, 0) == 4);
    Require(getnameinfo_nid_postfix(local.data(), local.size(), nullptr, 0, nullptr, 0, 0) == 0);
    Require(getnameinfo_nid_postfix(local.data(), local.size(), nullptr, 16, nullptr, 0, 0) == 0);
    std::memset(host, 'H', sizeof(host));
    Require(getnameinfo_nid_postfix(local.data(), local.size(), host, std::size_t{1} << 32, nullptr, 0, 0) == 0);
    Require(std::strcmp(host, "/var/s") == 0);
    local[0] = 5;
    Require(getnameinfo_nid_postfix(local.data(), local.size(), host, sizeof(host), nullptr, 0, 0) == 0);
    Require(std::strcmp(host, "/va") == 0);
    local[0] = 106;
    Require(getnameinfo_nid_postfix(local.data(), local.size(), host, sizeof(host), nullptr, 0, 0) == 0);
    Require(std::strcmp(host, "/var/s") == 0);
    Require(getnameinfo_nid_postfix(local.data(), local.size(), host, 50, nullptr, 0, 0) == 6);
    std::memset(host, 'H', sizeof(host));
    local[0] = 1;
    Require(getnameinfo_nid_postfix(local.data(), local.size(), host, sizeof(host), nullptr, 0, 0) == 0);
    Require(host[0] == 'H');
    local[0] = 0;
    Require(getnameinfo_nid_postfix(local.data(), local.size(), host, sizeof(host), nullptr, 0, 0) == 6);
    Require(host[0] == '\0');

    std::array<unsigned char, 200> link{54, 18, 7, 0};
    std::memset(service, 'S', sizeof(service));
    Require(getnameinfo_nid_postfix(link.data(), 54, host, sizeof(host), service, sizeof(service), 0) == 0);
    Require(std::strcmp(host, "link#7") == 0 && service[0] == '\0');
    Require(getnameinfo_nid_postfix(link.data(), 9, host, 7, nullptr, 0, 0) == 0);
    Require(std::strcmp(host, "link#7") == 0);
    Require(getnameinfo_nid_postfix(link.data(), link.size(), host, 6, nullptr, 0, 0) == 6);
    Require(host[0] == '\0');
    Require(getnameinfo_nid_postfix(link.data(), 8, host, sizeof(host), nullptr, 0, 0) == 4);
    std::memset(host, 'H', sizeof(host));
    Require(getnameinfo_nid_postfix(link.data(), 54, host, 0, nullptr, 0, 0) == 6);
    Require(host[0] == '\0' && host[1] == 'H');
    Require(getnameinfo_nid_postfix(link.data(), 54, host, std::size_t{1} << 31, nullptr, 0, 0) == 0);
    Require(std::strcmp(host, "link#7") == 0);
    errno = 0;
    Require(getnameinfo_nid_postfix(link.data(), 54, host, (std::size_t{1} << 31) + 1, nullptr, 0, 0) == 6);
    Require(host[0] == '\0' && errno == 84);
    for (const unsigned char name : {0, 3}) {
        link[5] = name;
        bool nullThrew = false;
        try {
            getnameinfo_nid_postfix(link.data(), 54, nullptr, 0, nullptr, 0, 0);
        } catch (const std::invalid_argument&) {
            nullThrew = true;
        }
        Require(nullThrew);
    }
    link[5] = 3;
    std::memcpy(link.data() + 8, "em0", 3);
    Require(getnameinfo_nid_postfix(link.data(), 54, host, sizeof(host), nullptr, 0, 0) == 0);
    Require(std::strcmp(host, "em0") == 0);
    Require(getnameinfo_nid_postfix(link.data(), 54, host, 3, nullptr, 0, 0) == 6);
    Require(host[0] == '\0');
    link[4] = 6;
    link[6] = 6;
    const unsigned char ether[]{0x00, 0x1b, 0x21, 0xaa, 0xbb, 0xcc};
    std::memcpy(link.data() + 11, ether, sizeof(ether));
    Require(getnameinfo_nid_postfix(link.data(), 54, host, 18, nullptr, 0, 0) == 0);
    Require(std::strcmp(host, "00:1b:21:aa:bb:cc") == 0);
    std::memset(host, 'H', sizeof(host));
    Require(getnameinfo_nid_postfix(link.data(), 54, host, 6, nullptr, 0, 0) == 6);
    Require(std::memcmp(host, "\0" "0:1b\0H", 7) == 0);
    errno = 0;
    Require(getnameinfo_nid_postfix(link.data(), 54, host, ~std::uint32_t{0}, nullptr, 0, 0) == 6);
    Require(host[0] == '\0' && errno == 84);
    bool threw = false;
    try {
        getnameinfo_nid_postfix(link.data(), 54, nullptr, 0, nullptr, 0, 0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    link[4] = 0x90;
    link[6] = 0;
    Require(getnameinfo_nid_postfix(link.data(), 54, host, sizeof(host), nullptr, 0, 0) == 0);
    Require(std::strcmp(host, "em0") == 0);
    link[6] = 7;
    Require(getnameinfo_nid_postfix(link.data(), 54, nullptr, 0, nullptr, 0, 0) == 5);
    link[6] = 16;
    const unsigned char firewire[]{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0xff};
    std::memcpy(link.data() + 11, firewire, sizeof(firewire));
    Require(getnameinfo_nid_postfix(link.data(), 54, host, sizeof(host), nullptr, 0, 0) == 0);
    Require(std::strcmp(host, "01:02:03:04:05:06:07:08") == 0);
    link[4] = 6;
    link[5] = 0;
    link[6] = 0;
    link[7] = 1;
    std::memset(host, 'H', sizeof(host));
    Require(getnameinfo_nid_postfix(link.data(), 54, host, sizeof(host), nullptr, 0, 0) == 0);
    Require(host[0] == '\0' && host[1] == 'H');
    std::memset(host, 0, sizeof(host));
    Require(getnameinfo_nid_postfix(v4.data(), v4.size(), host, sizeof(host), nullptr, 0, 10 | 32) == 0);
    Require(std::strcmp(host, "127.0.0.1") == 0);
    auto unsizedV4 = v4;
    unsizedV4[0] = 0;
    std::memset(host, 0, sizeof(host));
    Require(getnameinfo_nid_postfix(unsizedV4.data(), unsizedV4.size(), host, sizeof(host), nullptr, 0, 10) == 0);
    Require(std::strcmp(host, "127.0.0.1") == 0);
    Require(getnameinfo_nid_postfix(v4.data(), v4.size(), nullptr, 0, nullptr, 0, 10) == 0);
    std::memset(host, 'H', sizeof(host));
    std::memset(service, 'S', sizeof(service));
    Require(getnameinfo_nid_postfix(v4.data(), v4.size(), host, sizeof(host), service, 5, 10) == 6);
    Require(std::all_of(std::begin(host), std::end(host), [](char value) { return value == 'H'; }));
    Require(std::all_of(std::begin(service), std::end(service), [](char value) { return value == 'S'; }));
    Require(getnameinfo_nid_postfix(v4.data(), v4.size(), host, 1, nullptr, 0, 10) == 6);
    Require(getnameinfo_nid_postfix(v4.data(), v4.size(), host, 9, service, sizeof(service), 10) == 6);
    Require(std::strcmp(service, "27910") == 0);
    Require(std::all_of(std::begin(host), std::end(host), [](char value) { return value == 'H'; }));
    std::memset(service, 'S', sizeof(service));
    Require(getnameinfo_nid_postfix(v4.data(), v4.size(), host, sizeof(host), service, sizeof(service), 2 | 4 | 8) == 8);
    Require(std::strcmp(service, "27910") == 0);
    Require(std::all_of(std::begin(host), std::end(host), [](char value) { return value == 'H'; }));
    std::memset(service, 'S', sizeof(service));
    Require(getnameinfo_nid_postfix(v4.data(), v4.size(), nullptr, 5, service, sizeof(service), 10) == 0);
    Require(std::strcmp(service, "27910") == 0);
    Require(getnameinfo_nid_postfix(v6.data(), v6.size(), host, 3, nullptr, 0, 10) == 14);
    Require(std::all_of(std::begin(host), std::end(host), [](char value) { return value == 'H'; }));
    auto scopedV6 = v6;
    scopedV6[24] = 5;
    Require(getnameinfo_nid_postfix(scopedV6.data(), scopedV6.size(), host, sizeof(host), nullptr, 0, 10) == 0);
    Require(std::strncmp(host, "::1%", 4) == 0 && std::strlen(host) > 4);
    std::memset(host, 'H', sizeof(host));
    Require(getnameinfo_nid_postfix(scopedV6.data(), scopedV6.size(), host, 4, nullptr, 0, 10) == 14);
    Require(std::strcmp(host, "::1") == 0);
    Require(std::strcmp(gai_strerror_nid_postfix(0), "Success") == 0);
    Require(std::strcmp(gai_strerror_nid_postfix(3), "Invalid value for ai_flags") == 0);
    Require(std::strcmp(gai_strerror_nid_postfix(8), "hostname nor servname provided, or not known") == 0);
    Require(std::strcmp(gai_strerror_nid_postfix(10), "ai_socktype not supported") == 0);
    Require(std::strcmp(gai_strerror_nid_postfix(14), "Argument buffer overflow") == 0);
    Require(std::strcmp(gai_strerror_nid_postfix(-1), "Unknown error") == 0);
    Require(std::strcmp(gai_strerror_nid_postfix(15), "Unknown error") == 0);
    GuestResolver::AddressInfo hints{};
    hints.flags = 12; // numeric host and service: no external DNS dependency
    hints.socketType = 1;
    for (const auto* numeric : {"127.0.0.1", "::1"}) {
        GuestResolver::AddressInfo* result = nullptr;
        Require(getaddrinfo_nid_postfix(numeric, "27910", &hints, &result) == 0);
        Require(result != nullptr);
        for (auto* entry = result; entry; entry = entry->next) {
            Require(entry->family == 2 || entry->family == 28);
            Require(entry->socketType == 1);
            Require(getnameinfo_nid_postfix(entry->address, entry->addressLength,
                host, sizeof(host), service, sizeof(service), 10) == 0);
            Require(std::strcmp(host, numeric) == 0 && std::strcmp(service, "27910") == 0);
        }
        freeaddrinfo_nid_postfix(result);

    }
    GuestResolver::AddressInfo* result = reinterpret_cast<GuestResolver::AddressInfo*>(1);
    hints.family = 1234;
    Require(getaddrinfo_nid_postfix("127.0.0.1", "1", &hints, &result) == 5 && !result);
    hints.family = 0;
    hints.flags = 0x200;
    Require(getaddrinfo_nid_postfix("127.0.0.1", "1", &hints, &result) == 3 && !result);
    hints.flags = 12;
    Require(getaddrinfo_nid_postfix("invalid-numeric-address", "1", &hints, &result) == 8 && !result);
    freeaddrinfo_nid_postfix(nullptr);

}
